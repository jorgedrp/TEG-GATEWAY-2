#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h> // Para el manejo de señales (Ctrl+C)
#include <curl/curl.h>
#include "lora.h"
#include "spi.h"
#include "curl.h"

// --- Constantes del Programa ---
#define PAYLOAD_RX_LENGTH   0xF3
#define PAYLOAD_TX_LENGTH   0x05
#define MPU_BATCH_SIZE      2000
#define BME_BATCH_SIZE      16

#define MODO_TIEMPO 0x01
#define MODO_EVENTO 0x02
#define MODO_STANDBY 0x04
#define DATA_PULL   0x20

#define A_R 16384.0 // 32768/2
#define G_R 131.0   // 32768/250

#define MPU_PROCESSED_QUEUE_SIZE 50000
#define BME_PROCESSED_QUEUE_SIZE 1000
#define RX_QUEUE_SIZE 64

// --- ESTADOS DEL RADIO ---
typedef enum {
    STATE_POLLING,
    STATE_RX_CONTINUOUS,
    STATE_SYNC // Para el hilo sincronizador en el futuro
} radio_state_t;

// --- COMANDOS DEL PROCESADOR HACIA EL COMUNICADOR ---
typedef enum {
    CMD_NONE,
    CMD_SEND_NACK,
    CMD_SEND_ACK
} radio_cmd_t;

// --- ESTRUCTURAS PARA LA COLA (RING BUFFER) ---
typedef enum {
    MSG_SESSION_START,
    MSG_DATA,
    MSG_STOP_SESSION
} msg_type_t;

typedef struct {
    msg_type_t type;
    uint8_t dev_id;         // Solo válido en MSG_SESSION_START
    uint64_t timestamp;     // Solo válido en MSG_SESSION_START
    uint8_t payload[PAYLOAD_RX_LENGTH]; // Solo válido en MSG_DATA
} queue_item_t;

volatile sig_atomic_t keep_running = 1;
volatile uint8_t num_sensores = 0;
volatile size_t crc_count = 0;
volatile size_t first_packet_count = 0;

// --- VARIABLES COMPARTIDAS (IPC) ---

// 1. Control de Estado y Comandos (Protegido por radio_mutex)
pthread_mutex_t radio_mutex = PTHREAD_MUTEX_INITIALIZER;
radio_cmd_t pending_cmd = CMD_NONE;
uint8_t nack_packet_l = 0;
uint8_t nack_packet_h = 0;
bool sync_requested = false;

// 2. Semáforos para el hardware
//sem_t lora_irq; // Semáforo de hardware

// 3. Variables del Ring Buffer (Rx Queue)
queue_item_t rx_queue[RX_QUEUE_SIZE];
int rx_queue_head = 0; // Índice de escritura (Productor / Comunicador)
int rx_queue_tail = 0; // Índice de lectura (Consumidor / Procesador)

pthread_mutex_t rx_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_items_cola_rx; // Cuenta cuántos mensajes hay listos para leer
sem_t sem_slots_cola_rx; // Cuenta cuántos espacios vacíos quedan en el arreglo

// 4. Variables del Ring Buffer de datos procesados del MPU6050
mpu6050_data_t mpu_processed_data_queue[MPU_PROCESSED_QUEUE_SIZE];
int mpu_queue_in = 0;  // Índice de escritura (Productor / Procesador)
int mpu_queue_out = 0; // Índice de lectura (Consumidor / Almacenador)
pthread_mutex_t mpu_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t mpu_queue_count;             // Semáforo de datos disponibles para leer
sem_t mpu_queue_space_available;   // Semáforo de espacios vacíos para escribir

// 5. Variables de Ring Buffer de datos procesados del BME280
bme280_data_t bme_processed_data_queue[BME_PROCESSED_QUEUE_SIZE];
int bme_queue_in = 0;  // Índice de escritura (Productor / Procesador)
int bme_queue_out = 0; // Índice de lectura (Consumidor / Almacenador)
pthread_mutex_t bme_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t bme_queue_count;             // Semáforo de datos disponibles para leer
sem_t bme_queue_space_available;   // Semáforo de espacios vacíos para escribir

pthread_mutex_t write_influx_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int sig) {
    (void)sig;
    printf("\nSeñal de interrupción (Ctrl+C) recibida. Iniciando cierre limpio...\n");
    fflush(stdout);
    keep_running = 0;
}

// Función que usa el Hilo COMUNICADOR (Productor)
void queue_push(queue_item_t* item)
{
    // 1. Esperar a que haya al menos un espacio vacío en el arreglo
    sem_wait(&sem_slots_cola_rx);

    // 2. Bloquear el mutex para acceder al arreglo de forma exclusiva
    pthread_mutex_lock(&rx_queue_mutex);

    // 3. Copiar el elemento en la posición actual de la cabeza (head)
    rx_queue[rx_queue_head] = *item;

    // 4. Avanzar la cabeza de forma circular
    rx_queue_head = (rx_queue_head + 1) % RX_QUEUE_SIZE;

    // 5. Desbloquear el mutex
    pthread_mutex_unlock(&rx_queue_mutex);

    // 6. Señalizar que hay un nuevo elemento listo para ser leído
    sem_post(&sem_items_cola_rx);
}

// Función que usa el Hilo PROCESADOR (Consumidor)
void queue_pop(queue_item_t* item)
{
    // 1. Esperar a que haya al menos un elemento listo para leer
    sem_wait(&sem_items_cola_rx);

    // 2. Bloquear el mutex para acceder al arreglo de forma exclusiva
    pthread_mutex_lock(&rx_queue_mutex);

    // 3. Copiar el elemento desde la posición actual de la cola (tail)
    *item = rx_queue[rx_queue_tail];

    // 4. Avanzar la cola de forma circular
    rx_queue_tail = (rx_queue_tail + 1) % RX_QUEUE_SIZE;

    // 5. Desbloquear el mutex
    pthread_mutex_unlock(&rx_queue_mutex);

    // 6. Señalizar que se ha liberado un espacio (slot) en el arreglo
    sem_post(&sem_slots_cola_rx);
}

void* task_communicator(void* p)
{
    sensor_data_t* sensor_list = (sensor_data_t*)p;
    size_t k = 0;
    size_t retrys = 0;

    // Iniciar en estado de búsqueda
    radio_state_t current_state = STATE_POLLING;
    uint8_t active_dev_id = 0;

    while (keep_running)
    {
        // 1. REVISAR COMANDOS PENDIENTES DEL PROCESADOR O SINCRONIZADOR
        pthread_mutex_lock(&radio_mutex);
        radio_cmd_t local_cmd = pending_cmd;
        uint8_t local_nack_l = nack_packet_l;
        uint8_t local_nack_h = nack_packet_h;
        bool local_sync_req = sync_requested;

        // Si procesamos el comando, lo limpiamos
        if (pending_cmd != CMD_NONE) pending_cmd = CMD_NONE;
        pthread_mutex_unlock(&radio_mutex);

        // 2. PROCESAR COMANDOS ANTES DE ACTUAR SEGÚN EL ESTADO
        if (local_cmd == CMD_SEND_NACK && current_state == STATE_RX_CONTINUOUS)
        {
            uint8_t data[PAYLOAD_TX_LENGTH] = {active_dev_id, DATA_LOSS, 0x01, local_nack_l, local_nack_h};
            retardo_milisegundos(1000);
            send_packet(data, PAYLOAD_TX_LENGTH);

            writeRegister(REG_FIFO_ADDR_PTR, 0x00);
            writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_RX_LENGTH);
            writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
            writeRegister(REG_OP_MODE, 0x8D); // Modo RX continuo
            // Seguimos en STATE_RX_CONTINUOUS esperando la respuesta al NACK
        }
        else if (local_cmd == CMD_SEND_ACK && current_state == STATE_RX_CONTINUOUS)
        {
            size_t intentos = 0;

            while(intentos < 5)
            {
                uint8_t data[PAYLOAD_TX_LENGTH] = {active_dev_id, DATA_LOSS, 0x00, 0xFF, 0xFF};
                send_packet(data, PAYLOAD_TX_LENGTH);

                if(single_receive_packet(active_dev_id, ACKNOWLEDGEMENT))
                {
                    printf("STATUS:%u:STANDBY\n", sensor_list[k].dev_id);
                    fflush(stdout);
                    break;
                }
                else
                {
                    intentos++;
                }
            }

            config_lora(SIGNAL_CHANNEL);
            writeRegister(REG_FIFO_ADDR_PTR, 0x00);
            writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_RX_LENGTH);
            writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
            writeRegister(REG_OP_MODE, 0x8D); // Modo RX continuo

            current_state = STATE_POLLING;
            k = (k + 1) % num_sensores;
        }
        else if(retrys == 10)
        {
            queue_item_t stop_msg;
            stop_msg.type = MSG_STOP_SESSION;
            queue_push(&stop_msg);

            if(first_packet_count == num_sensores)
            {
                keep_running = 0;
                break;
            }

            config_lora(SIGNAL_CHANNEL);
            writeRegister(REG_FIFO_ADDR_PTR, 0x00);
            writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_RX_LENGTH);
            writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
            writeRegister(REG_OP_MODE, 0x8D); // Modo RX continuo

            current_state = STATE_POLLING;
            k = (k + 1) % num_sensores;
        }

        if (local_sync_req && current_state == STATE_POLLING)
        {
            current_state = STATE_SYNC;
        }


        // 3. EJECUTAR LÓGICA SEGÚN EL ESTADO ACTUAL
        switch (current_state)
        {
            case STATE_POLLING:
            {
                uint8_t data[PAYLOAD_TX_LENGTH] = {sensor_list[k].dev_id, STATUS_CODE, 0xFF, 0xFF, 0xFF};
                send_packet(data, PAYLOAD_TX_LENGTH);

                if (single_receive_packet(sensor_list[k].dev_id, STATUS_CODE))
                {
                    uint8_t modo = readRegister(REG_FIFO);
                    uint8_t haveData = readRegister(REG_FIFO);

                    if ((modo != MODO_TIEMPO) && (haveData != DATA_AVAILABLE))
                    {
                        printf("Se encontró el sensor %u pero no en modo tiempo y sin datos.\n", sensor_list[k].dev_id);
                        fflush(stdout);
                        for (int i = k; i < num_sensores - 1; i++)
                        {
                            sensor_list[i] = sensor_list[i + 1];
                        }

                        first_packet_count++;

                        if (first_packet_count == num_sensores)
                        {
                            keep_running = 0;
                        }

                        num_sensores--;
                    }
                    else
                    {
                        data[1] = DATA_PULL;
                        send_packet(data, PAYLOAD_TX_LENGTH);

                        // Configurar LoRa para RX continuo
                        writeRegister(REG_FIFO_ADDR_PTR, 0x00);
                        writeRegister(REG_PAYLOAD_LENGTH, 0x0A);
                        writeRegister(REG_DIO_MAPPING_1, 0x00);
                        writeRegister(REG_OP_MODE, 0x8D); // Modo RX continuo

                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_sec += 12;

                        if (sem_timedwait(&lora_irq, &ts) == -1)
                        {
                            if (errno == ETIMEDOUT) continue;
                        }

                        uint8_t irq_flags = readRegister(REG_IRQ_FLAGS);
                        writeRegister(REG_IRQ_FLAGS, 0xFF); // Limpiar banderas

                        if(irq_flags & IRQ_RX_DONE_MASK)
                        {
                            writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

                            // Extraer los 10 primeros bytes para ver qué es
                            uint8_t reg_data[10];

                            for (size_t i = 0; i < 10; i++) reg_data[i] = readRegister(REG_FIFO);

                            active_dev_id = reg_data[0];

                            // ¿Es el paquete inicial con el Timestamp?
                            if (sensor_list[k].dev_id == active_dev_id && reg_data[1] == DATA_AVAILABLE)
                            {
                                uint64_t timestamp = (uint64_t)((uint64_t)reg_data[2] << 56 |
                                                              (uint64_t)reg_data[3] << 48 |
                                                              (uint64_t)reg_data[4] << 40 |
                                                              (uint64_t)reg_data[5] << 32 |
                                                              (uint64_t)reg_data[6] << 24 |
                                                              (uint64_t)reg_data[7] << 16 |
                                                              (uint64_t)reg_data[8] << 8 |
                                                              (uint64_t)reg_data[9]);

                                printf("Solicitud de transmisión de datos del sensor %u recibida.\n", active_dev_id);
                                fflush(stdout);
                                printf("DATA:%u:%llu:%llu\n", active_dev_id, (unsigned long long)(timestamp / 1000), (unsigned long long)((timestamp / 1000) + sensor_list[k].time * 1000));
                                fflush(stdout);

                                // Respondemos confirmando que empezamos
                                uint8_t ack_data[PAYLOAD_TX_LENGTH] = {active_dev_id, DOWNLOAD_DATA, 0xFF, 0xFF, 0xFF};
                                send_packet(ack_data, PAYLOAD_TX_LENGTH);

                                // === ENVIAR METADATOS AL PROCESADOR ===
                                queue_item_t start_msg;
                                start_msg.type = MSG_SESSION_START;
                                start_msg.dev_id = active_dev_id;
                                start_msg.timestamp = timestamp;
                                queue_push(&start_msg);
                                // =======================================

                                // Dejar módulo en RX continuo para la ráfaga
                                config_lora(sensor_list[k].lora_mode);
                                writeRegister(REG_FIFO_ADDR_PTR, 0x00);
                                writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_RX_LENGTH);
                                writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
                                writeRegister(REG_OP_MODE, 0x8D); // Modo RX continuo

                                retrys = 0;
                                current_state = STATE_RX_CONTINUOUS;
                            }
                        }
                    }
                }
                else
                {
                    printf("STATUS:%u:OFF\n", sensor_list[k].dev_id);
                    fflush(stdout);
                    for (int i = k; i < num_sensores - 1; i++)
                    {
                        sensor_list[i] = sensor_list[i + 1];
                    }

                    first_packet_count++;

                    if (first_packet_count == num_sensores)
                    {
                        keep_running = 0;
                    }

                    num_sensores--;
                }

                k = (k + 1) % num_sensores;
                break;
            }

            case STATE_RX_CONTINUOUS:
            {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                if (sensor_list[k].lora_mode <= 5)
                {
                    ts.tv_sec += 2; // Timeout corto para revisar comandos del procesador
                }
                else if (sensor_list[k].lora_mode == 6)
                {
                    ts.tv_sec += 4;
                }
                else
                {
                    ts.tv_sec += 10;
                }

                if (sem_timedwait(&lora_irq, &ts) == -1)
                {
                    retrys++;
                    // Si hubo timeout, el loop vuelve al inicio y revisará si hay "NACKs" pendientes
                    if (errno == ETIMEDOUT) continue;
                }

                retrys = 0;

                uint8_t irq_flags = readRegister(REG_IRQ_FLAGS);
                writeRegister(REG_IRQ_FLAGS, 0xFF); // Limpiar banderas

                if (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK)
                {
                    crc_count++;
                }
                else if (irq_flags & IRQ_RX_DONE_MASK)
                {
                    writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

                    // ES UN PAQUETE DE DATOS NORMAL
                    queue_item_t data_msg;
                    data_msg.type = MSG_DATA;
                    for (int i = 0; i < PAYLOAD_RX_LENGTH; i++) {
                        data_msg.payload[i] = readRegister(REG_FIFO);
                    }

                    // === ENVIAR DATOS CRUDOS AL PROCESADOR ===
                    queue_push(&data_msg); // Esta función debe hacer sem_post(&items_in_rx_queue)
                    // =========================================
                }
                break;
            }

            case STATE_SYNC:
            {
                // Lógica futura para el Sincronizador
                // Cuando termine, la variable current_state volverá a STATE_POLLING
                break;
            }
        }
    }
    return NULL;
}

void* task_process_data(void* p)
{
    // Recibimos la lista de sensores al crear el hilo (una sola vez)
    sensor_data_t* sensor_list = (sensor_data_t*)p;

    // Variables de contexto para la sesión actual
    bool session_active = false;
    uint8_t active_dev_id = 0;
    int active_rate = 0;
    uint64_t start_time = 0;
    uint16_t cant_paq_mpu = 0;
    char* received_mpu = NULL;

    int packet_count = 0;
    int first_packet = 1;

    while (keep_running)
    {
        queue_item_t msg;

        // Función ficticia: extrae el siguiente mensaje de la Cola Rx (Ring Buffer)
        // Internamente debe hacer sem_wait(&sem_items_cola_rx) y usar el mutex_cola_rx
        queue_pop(&msg);

        if (!keep_running) break;

        // --- INICIO DE NUEVA RÁFAGA DE DATOS ---
        if (msg.type == MSG_SESSION_START)
        {
            active_dev_id = msg.dev_id;
            start_time = (uint64_t)(msg.timestamp / 1000);

            // Buscar los parámetros del sensor en la lista
            int time_s = 0;
            for (size_t i = 0; i < num_sensores; i++) {
                if (sensor_list[i].dev_id == active_dev_id) {
                    active_rate = sensor_list[i].rate;
                    time_s = sensor_list[i].time;
                    break;
                }
            }

            cant_paq_mpu = (uint16_t)((time_s * 1000) / (active_rate * 16));

            // Limpiar memoria de la sesión anterior si quedó colgada por algún error
            if (received_mpu != NULL) free(received_mpu);

            // Reservar memoria para el mapa de paquetes de esta sesión
            received_mpu = (char*)calloc(cant_paq_mpu, sizeof(char));

            session_active = true;
            first_packet = 1;
            packet_count = 0;

            time_t segundos = (time_t)(msg.timestamp / 1000000L);
            struct tm *tm_local = localtime(&segundos);
            char buffer_fecha_hora[100];
            strftime(buffer_fecha_hora, sizeof(buffer_fecha_hora), "%Y-%m-%d %H:%M:%S", tm_local);
            printf("Recepción iniciada con sensor %u. Fecha de evento: %s\n", active_dev_id, buffer_fecha_hora);
            fflush(stdout);
        }

        // --- PROCESAMIENTO DE DATOS ---
        else if (msg.type == MSG_DATA && session_active)
        {
            uint8_t sensor_id = msg.payload[0];

            // 1. ES UN PAQUETE DE DATOS DEL MPU
            if(sensor_id == (active_dev_id | MPU_ID))
            {
                uint16_t mpu_packet_num = (uint16_t)(msg.payload[2] << 8) | (uint16_t)msg.payload[1];

                for (int i = 3; i < PAYLOAD_RX_LENGTH; i += 15)
                {
                    mpu6050_dataraw_t medicion_raw;
                    mpu6050_data_t medicion;

                    memcpy(&medicion_raw, &msg.payload[i], sizeof(mpu6050_dataraw_t));
                    medicion.dev_id = (active_dev_id | MPU_ID);

                    if (first_packet)
                    {
                        first_packet = 0;
                        first_packet_count++;
                    }

                    int32_t delta_time = (int32_t)((uint32_t)medicion_raw.timestamp_h << 16 | (uint32_t)medicion_raw.timestamp_m << 8 | (uint32_t)medicion_raw.timestamp_l);
                    delta_time = delta_time << 8;
                    delta_time = delta_time >> 8;
                    medicion.timestamp = start_time + (delta_time * active_rate);

                    medicion.ax = (float)medicion_raw.ax / A_R;
                    medicion.ay = (float)medicion_raw.ay / A_R;
                    medicion.az = (float)medicion_raw.az / A_R;
                    medicion.gx = (float)medicion_raw.gx / G_R;
                    medicion.gy = (float)medicion_raw.gy / G_R;
                    medicion.gz = (float)medicion_raw.gz / G_R;

                    // Enviar a la cola de base de datos
                    sem_wait(&mpu_queue_space_available);
                    pthread_mutex_lock(&mpu_queue_mutex);
                    mpu_processed_data_queue[mpu_queue_in] = medicion;
                    mpu_queue_in = (mpu_queue_in + 1) % MPU_PROCESSED_QUEUE_SIZE;
                    pthread_mutex_unlock(&mpu_queue_mutex);
                    sem_post(&mpu_queue_count);
                }

                // Marcar como recibido
                if (mpu_packet_num < cant_paq_mpu) {
                    received_mpu[mpu_packet_num] = 1;
                }
                packet_count++;
            }
            // 2. ES UN PAQUETE DE DATOS DEL BME
            else if(sensor_id == (active_dev_id | BME_ID))
            {
                uint16_t bme_packet_num = (uint16_t)(msg.payload[2] << 8) | (uint16_t)msg.payload[1];
                (void)bme_packet_num;

                for(size_t i = 3 ; i < PAYLOAD_RX_LENGTH; i += 15)
                {
                    bme280_dataraw_t medicion_raw;
                    bme280_data_t medicion;

                    memcpy(&medicion_raw, &msg.payload[i], sizeof(bme280_dataraw_t));
                    medicion.dev_id = (active_dev_id | BME_ID);

                    int32_t delta_time = (int32_t)((uint32_t)medicion_raw.timestamp_h << 16 | (uint32_t)medicion_raw.timestamp_m << 8 | (uint32_t)medicion_raw.timestamp_l);
                    medicion.timestamp = start_time + delta_time;

                    medicion.temperature = medicion_raw.temperature;
                    medicion.humidity = medicion_raw.humidity;
                    medicion.pressure = medicion_raw.pressure;

                    // Enviar a la cola de base de datos
                    sem_wait(&bme_queue_space_available);
                    pthread_mutex_lock(&bme_queue_mutex);
                    bme_processed_data_queue[bme_queue_in] = medicion;
                    bme_queue_in = (bme_queue_in + 1) % BME_PROCESSED_QUEUE_SIZE;
                    pthread_mutex_unlock(&bme_queue_mutex);
                    sem_post(&bme_queue_count);
                }
            }
            // 3. ES EL PAQUETE DE FINALIZACIÓN (EOF)
            else if((sensor_id == 0xFA) && (msg.payload[1] == (active_dev_id | MPU_ID)))
            {
                printf("Paquete de finalización del sensor %i recibido.\n", active_dev_id);
                fflush(stdout);

                bool missing_packet_found = false;

                size_t total_lost = 0;

                for(uint16_t k = 0 ; k < cant_paq_mpu ; k++)
                {
                    if(received_mpu[k] == 0)
                    {
                        total_lost++;
                    }
                }

                printf("Total de paquetes perdidos: %zu\n", total_lost);
                fflush(stdout);

                // Buscar el primer paquete perdido
                for(uint16_t k = 0 ; k < cant_paq_mpu ; k++)
                {
                    if(received_mpu[k] == 0)
                    {
                        printf("Paquete número %u perdido. Solicitándolo nuevamente...\n", (unsigned int)k);
                        fflush(stdout);

                        // Escribir comando NACK para el Hilo Comunicador
                        pthread_mutex_lock(&radio_mutex);
                        nack_packet_l = (uint8_t)(k & 0xFF);
                        nack_packet_h = (uint8_t)((k >> 8) & 0xFF);
                        pending_cmd = CMD_SEND_NACK;
                        pthread_mutex_unlock(&radio_mutex);

                        missing_packet_found = true;
                        break; // Salimos del for, esperamos a que llegue el paquete perdido
                    }
                }

                // Si no falta ninguno, finalizamos la sesión
                if (!missing_packet_found)
                {
                    printf("Todos los paquetes recibidos. Total: %i\n", packet_count);
                    fflush(stdout);

                    // Escribir comando ACK para que el Comunicador vuelva a POLLING
                    pthread_mutex_lock(&radio_mutex);
                    pending_cmd = CMD_SEND_ACK;
                    pthread_mutex_unlock(&radio_mutex);

                    // Limpiar contexto
                    free(received_mpu);
                    received_mpu = NULL;
                    session_active = false;
                }
            }

            // 3. PAQUETE CORRUPTO O DE OTRO SENSOR
            else
            {
                printf("Paquete ignorado (ID: 0x%02X)\n", sensor_id);
                fflush(stdout);
            }
        }
        else if(msg.type == MSG_STOP_SESSION)
        {
            free(received_mpu);
            received_mpu = NULL;
            session_active = false;
        }
    }

    if (received_mpu != NULL) free(received_mpu);
    return NULL;
}

void* task_send_mpu_to_influx(void* p)
{
    (void)p;
    mpu6050_data_t data_buffer[MPU_BATCH_SIZE];
    int mpu_data_count = 0;

    while (keep_running)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; // Timeout de 1 segundo

        // 1. Esperar a que haya un dato procesado (o que pase 1 segundo)
        if (sem_timedwait(&mpu_queue_count, &ts) == -1)
        {
            if (errno == ETIMEDOUT)
            {
                // Si pasó 1 segundo y tenemos datos colgados, los enviamos
                if (mpu_data_count > 0)
                {
                    //printf("Timeout. Enviando lote parcial de %d puntos...\n", mpu_data_count);
                    //fflush(stdout);
                    pthread_mutex_lock(&write_influx_mutex);
                    write_mpu_influx(data_buffer, mpu_data_count);
                    pthread_mutex_unlock(&write_influx_mutex);
                    mpu_data_count = 0;
                }
                continue;
            }
            else if (errno == EINTR)
            {
                // Interrupción por señal del Sistema Operativo, volvemos a intentar
                continue;
            }
            else
            {
                // Otro tipo de error
                continue;
            }
        }

        // Verificación de seguridad por si keep_running cambia durante la espera
        if (!keep_running) break;

        // 2. Extraer el dato del Ring Buffer de forma segura
        pthread_mutex_lock(&mpu_queue_mutex);
        data_buffer[mpu_data_count] = mpu_processed_data_queue[mpu_queue_out];
        mpu_queue_out = (mpu_queue_out + 1) % MPU_PROCESSED_QUEUE_SIZE;
        pthread_mutex_unlock(&mpu_queue_mutex);

        // 3. Señalizar al Hilo Procesador que se liberó un espacio en el arreglo
        sem_post(&mpu_queue_space_available);

        mpu_data_count++;

        // 4. Si alcanzamos el límite del lote, enviamos a InfluxDB
        if (mpu_data_count == MPU_BATCH_SIZE)
        {
            pthread_mutex_lock(&write_influx_mutex);
            write_mpu_influx(data_buffer, mpu_data_count);
            pthread_mutex_unlock(&write_influx_mutex);
            mpu_data_count = 0;
        }
    }

    // 5. Limpieza final: Vaciar el buffer remanente al cerrar el programa
    if (mpu_data_count > 0)
    {
        //printf("Cierre del programa. Enviando lote final de %d puntos...\n", mpu_data_count);
        //fflush(stdout);
        pthread_mutex_lock(&write_influx_mutex);
        write_mpu_influx(data_buffer, mpu_data_count);
        pthread_mutex_unlock(&write_influx_mutex);
    }

    return NULL;
}

void* task_send_bme_to_influx(void* p)
{
    (void)p;
    bme280_data_t data_buffer[BME_BATCH_SIZE];
    int bme_data_count = 0;

    while (keep_running)
    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; // Timeout de 1 segundo

        // 1. Esperar a que haya un dato procesado (o que pase 1 segundo)
        if (sem_timedwait(&bme_queue_count, &ts) == -1)
        {
            if (errno == ETIMEDOUT)
            {
                // Si pasó 1 segundo y tenemos datos colgados, los enviamos
                if (bme_data_count > 0)
                {
                    printf("Timeout. Enviando lote parcial de %d puntos...\n", bme_data_count);
                    fflush(stdout);
                    pthread_mutex_lock(&write_influx_mutex);
                    write_bme_influx(data_buffer, bme_data_count);
                    pthread_mutex_unlock(&write_influx_mutex);
                    bme_data_count = 0;
                }
                continue;
            }
            else if (errno == EINTR)
            {
                // Interrupción por señal del Sistema Operativo, volvemos a intentar
                continue;
            }
            else
            {
                // Otro tipo de error
                continue;
            }
        }

        // Verificación de seguridad por si keep_running cambia durante la espera
        if (!keep_running) break;

        // 2. Extraer el dato del Ring Buffer de forma segura
        pthread_mutex_lock(&bme_queue_mutex);
        data_buffer[bme_data_count] = bme_processed_data_queue[bme_queue_out];
        bme_queue_out = (bme_queue_out + 1) % BME_PROCESSED_QUEUE_SIZE;
        pthread_mutex_unlock(&bme_queue_mutex);

        // 3. Señalizar al Hilo Procesador que se liberó un espacio en el arreglo
        sem_post(&bme_queue_space_available);

        bme_data_count++;

        // 4. Si alcanzamos el límite del lote, enviamos a InfluxDB
        if (bme_data_count == BME_BATCH_SIZE)
        {
            pthread_mutex_lock(&write_influx_mutex);
            write_bme_influx(data_buffer, bme_data_count);
            pthread_mutex_unlock(&write_influx_mutex);
            bme_data_count = 0;
        }
    }

    // 5. Limpieza final: Vaciar el buffer remanente al cerrar el programa
    if (bme_data_count > 0)
    {
        printf("Cierre del programa. Enviando lote final de %d puntos...\n", bme_data_count);
        fflush(stdout);
        pthread_mutex_lock(&write_influx_mutex);
        write_bme_influx(data_buffer, bme_data_count);
        pthread_mutex_unlock(&write_influx_mutex);
    }

    return NULL;
}

void task_tx(sensor_data_t *sensor_list, size_t num_sensores)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {BROADCAST_ID, MODO_TIEMPO, sensor_list[0].time, 0xFF, 0xFF};

    if(num_sensores == 1)
    {
        data[0] = sensor_list[0].dev_id;
    }

    int intentos = 0;

    while(intentos < 5)
    {
        send_packet(data, PAYLOAD_TX_LENGTH);

        if(num_sensores == 1)
        {
            if(single_receive_packet(sensor_list[0].dev_id, ACKNOWLEDGEMENT))
            {
                printf("Confirmado el cambio de modo del sensor %u.\n", sensor_list[0].dev_id);
                fflush(stdout);
                printf("STATUS:%u:TIEMPO\n", sensor_list[0].dev_id);
                fflush(stdout);
                break;
            }
            else
            {
                printf("No se cambio de modo. Reintentando... %i\n", intentos + 1);
                fflush(stdout);
            }
        }
        else
        {
            retardo_milisegundos(1000);

            for(size_t i = 0 ; i < SENSOR_NUM ; i++)
            {
                uint8_t data[PAYLOAD_TX_LENGTH] = {sensor_list[i].dev_id, STATUS_CODE, 0xFF, 0xFF, 0xFF};
                send_packet(data, PAYLOAD_TX_LENGTH);

                if (single_receive_packet(sensor_list[i].dev_id, STATUS_CODE))
                {
                    uint8_t modo = readRegister(REG_FIFO);

                    if(modo == MODO_TIEMPO)
                    {
                        printf("STATUS:%u:TIEMPO\n", sensor_list[i].dev_id);
                        fflush(stdout);
                    }
                    else
                    {
                        printf("El sensor %u no cambió a modo tiempo.\n", sensor_list[i].dev_id);
                        fflush(stdout);
                        if(modo == MODO_EVENTO)
                        {
                            printf("STATUS:%u:EVENTO\n", sensor_list[i].dev_id);
                            fflush(stdout);
                        }
                        else if(modo == MODO_STANDBY)
                        {
                            printf("STATUS:%u:STANDBY\n", sensor_list[i].dev_id);
                            fflush(stdout);
                        }
                    }
                }
                else
                {
                    printf("El sensor %u no respondió.\n", sensor_list[i].dev_id);
                    fflush(stdout);
                    printf("STATUS:%u:OFF\n", sensor_list[i].dev_id);
                    fflush(stdout);
                }
            }
            break;
        }

        intentos++;

        if(intentos == 5)
        {
            printf("No se pudo establecer comunicación con el sensor %u.\n", sensor_list[0].dev_id);
            fflush(stdout);
            printf("STATUS:%u:OFF\n", sensor_list[0].dev_id);
            fflush(stdout);
        }
    }

}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: Faltan argumentos.\n");
        return 1;
    }

    sensor_data_t sensor_list[SENSOR_NUM];
    sensor_data_t sensor;

    sensor.dev_id = atoi(argv[1]);

    if(sensor.dev_id == 0xFF)
    {
        for(size_t i = 0; i < SENSOR_NUM; i++)
        {
            sensor.dev_id = atoi(argv[i*4 + 2]);
            sensor.lora_mode = atoi(argv[i*4 + 3]);
            sensor.rate = atoi(argv[i*4 + 4]);
            sensor.time = atoi(argv[i*4 + 5]);

            sensor_list[i] = sensor;
        }
        num_sensores = SENSOR_NUM;
    }
    else
    {
        sensor.lora_mode = atoi(argv[2]);
        sensor.rate = atoi(argv[3]);
        sensor.time = atoi(argv[4]);

        sensor_list[0] = sensor;
        num_sensores = 1;
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Error fatal: No se pudo inicializar libcurl.\n");
        fflush(stdout);
        return 1;
    }

    init_lora();

    // --- 1. Inicialización de IPC (Mutex y Semáforos) ---

    // Mutex de control y colas
    pthread_mutex_init(&radio_mutex, NULL);
    pthread_mutex_init(&rx_queue_mutex, NULL);
    pthread_mutex_init(&mpu_queue_mutex, NULL);
    pthread_mutex_init(&bme_queue_mutex, NULL);
    pthread_mutex_init(&write_influx_mutex, NULL);

    // Semáforo de hardware
    sem_init(&lora_irq, 0, 0);

    // Semáforos del Ring Buffer 1 (Comunicador -> Procesador)
    sem_init(&sem_items_cola_rx, 0, 0);
    sem_init(&sem_slots_cola_rx, 0, RX_QUEUE_SIZE);

    // Semáforos del Ring Buffer 2 (Procesador -> Almacenador)
    sem_init(&mpu_queue_count, 0, 0);
    sem_init(&mpu_queue_space_available, 0, MPU_PROCESSED_QUEUE_SIZE);
    sem_init(&bme_queue_count, 0, 0);
    sem_init(&bme_queue_space_available, 0, BME_PROCESSED_QUEUE_SIZE);

    // Configurar el manejador de señal para Ctrl+C
    signal(SIGINT, sigint_handler);

    init_lora_interrupt();

    // Activar los nodos sensores
    task_tx(sensor_list, num_sensores);

    // --- 2. Creación de Hilos Estáticos ---
    pthread_t comm_thread, proc_thread, mpu_sender_thread, bme_sender_thread;

    pthread_create(&comm_thread, NULL, task_communicator, (void*)sensor_list);
    pthread_create(&proc_thread, NULL, task_process_data, (void*)sensor_list);
    pthread_create(&mpu_sender_thread, NULL, task_send_mpu_to_influx, NULL);
    pthread_create(&bme_sender_thread, NULL, task_send_bme_to_influx, NULL);

    // --- 3. Bucle Principal de Espera ---
    while (keep_running) {
        sleep(1);
    }

    // --- 4. Secuencia de Cierre Limpio ---
    printf("Iniciando secuencia de apagado en main...\n");
    fflush(stdout);

    // Desbloquear todos los hilos que puedan estar dormidos en un sem_wait
    sem_post(&lora_irq);           // Despierta al Comunicador
    sem_post(&sem_items_cola_rx);  // Despierta al Procesador
    sem_post(&mpu_queue_count);        // Despierta al Almacenador (por si acaso)
    sem_post(&bme_queue_count);

    // Esperar a que terminen de ejecutar su ciclo final y salgan
    pthread_join(comm_thread, NULL);
    pthread_join(proc_thread, NULL);
    pthread_join(mpu_sender_thread, NULL);
    pthread_join(bme_sender_thread, NULL);

    printf("Todos los hilos han finalizado.\n");
    fflush(stdout);

    // --- 5. Destrucción de recursos ---
    pthread_mutex_destroy(&radio_mutex);
    pthread_mutex_destroy(&rx_queue_mutex);
    pthread_mutex_destroy(&mpu_queue_mutex);
    pthread_mutex_destroy(&bme_queue_mutex);
    pthread_mutex_destroy(&write_influx_mutex);

    sem_destroy(&lora_irq);
    sem_destroy(&sem_items_cola_rx);
    sem_destroy(&sem_slots_cola_rx);
    sem_destroy(&mpu_queue_count);
    sem_destroy(&mpu_queue_space_available);
    sem_destroy(&bme_queue_count);
    sem_destroy(&bme_queue_space_available);

    curl_global_cleanup();

    return 0;
}
