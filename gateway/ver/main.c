#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "lora.c"
#include "spi.c"
#include "curl.c"

#define PAYLOAD_RX_LENGTH 0xF0
#define PAYLOAD_TX_LENGTH 0x03
#define PACKET_SIZE 240

#define A_R 16384.0 // 32768/2
#define G_R 131.0 // 32768/250

#define PROCESSED_QUEUE_SIZE 50000
#define EXPECTED_PACKETS 3600

mpu6050_data_t mpu_processed_data_queue[PROCESSED_QUEUE_SIZE];
int mpu_queue_in = 0;
int mpu_queue_out = 0;
sem_t mpu_queue_count; // Semáforo para contar elementos en la cola
//sem_t queue_space_available;
pthread_mutex_t mpu_queue_mutex; // Mutex para proteger los índices de la cola

typedef struct {
    uint8_t data[PACKET_SIZE];
    uint8_t index;
} buffer_t;

buffer_t buffer_a, buffer_b;
buffer_t* active_buffer;
buffer_t* processing_buffer;

pthread_mutex_t buffer_mutex;
sem_t buffer_full;
sem_t lora_rx;

int first_packet = 1;
int packet_count = 0;
int crc_count = 0;
int data_saved = 0;

void packet_arrived_isr(void)
{
    sem_post(&lora_rx);
}

void* task_process_data(void* p)
{
    struct timeval pi_start_time;
    uint64_t pi_start_unix;

    mpu6050_data_t medicion;
    mpu6050_dataraw_t medicion_raw;

    while(1)
    {
        sem_wait(&buffer_full); // Espera a que el buffer se llene

        uint8_t buffer_copy[PACKET_SIZE];
        pthread_mutex_lock(&buffer_mutex);
        memcpy(buffer_copy, processing_buffer->data, PACKET_SIZE);
        pthread_mutex_unlock(&buffer_mutex);

        for(int i = 0 ; i < PACKET_SIZE ; i += 16)
        {
            memcpy(&medicion_raw, &buffer_copy[i], sizeof(mpu6050_dataraw_t));

            if(medicion_raw.dev_id == 0x11 || medicion_raw.dev_id == 0x12)
            {
                medicion.dev_id = medicion_raw.dev_id;
            }
            else if(medicion_raw.dev_id == 0xFF)
            {
                packet_count--;
                first_packet = 1;
                printf("Procesamiento finalizado.\n");
                break;
            }
            else
            {
                packet_count--;
		        printf("Paquete corrupto recibido.\n");
                break;
            }

            if(first_packet)
            {
		        printf("Recepcion iniciada.\n");
                gettimeofday(&pi_start_time, NULL);
                pi_start_unix = (uint64_t)pi_start_time.tv_sec * 1000 + (uint64_t)(pi_start_time.tv_usec / 1000);
                medicion.timestamp = pi_start_unix;
                first_packet = 0;
            }
            else
            {
                medicion.timestamp = pi_start_unix + ((uint64_t)medicion_raw.timestamp_h << 16 | (uint64_t)medicion_raw.timestamp_m << 8  | (uint64_t)medicion_raw.timestamp_l);
            }

            medicion.ax = (float)medicion_raw.ax / A_R;
            medicion.ay = (float)medicion_raw.ay / A_R;
            medicion.az = (float)medicion_raw.az / A_R;
            medicion.gx = (float)medicion_raw.gx / G_R;
            medicion.gy = (float)medicion_raw.gy / G_R;
            medicion.gz = (float)medicion_raw.gz / G_R;

            //printf("\rAccel [X:%.2f Y:%.2f Z:%.2f] | Gyro [X:%.2f Y:%.2f Z:%.2f]", medicion.ax, medicion.ay, medicion.az, medicion.gx, medicion.gy, medicion.gz);
            //fflush(stdout);

            //sem_wait(&queue_space_available); // Asegurarse de que hay espacio
            pthread_mutex_lock(&mpu_queue_mutex);
            mpu_processed_data_queue[mpu_queue_in] = medicion;
            mpu_queue_in = (mpu_queue_in + 1) % PROCESSED_QUEUE_SIZE;
            pthread_mutex_unlock(&mpu_queue_mutex);
            sem_post(&mpu_queue_count); // Incrementar contador de elementos
        }
        packet_count++;

        if(first_packet)
        {
            printf("Paquetes procesados: %i\n", packet_count);
            printf("Errores de CRC: %i\n", crc_count);
        }
    }
    return NULL;
}

void* task_send_mpu_to_influx(void* p) {
    mpu6050_data_t data_to_send;

    while(1)
    {
        sem_wait(&mpu_queue_count); // Esperar a que haya un elemento en la cola

        pthread_mutex_lock(&mpu_queue_mutex);
        data_to_send = mpu_processed_data_queue[mpu_queue_out];
        mpu_queue_out = (mpu_queue_out + 1) % PROCESSED_QUEUE_SIZE;
        pthread_mutex_unlock(&mpu_queue_mutex);
        //sem_post(&queue_space_available);
        
        // La llamada lenta a la red está aislada aquí
        write_influx(&data_to_send);
        data_saved++;

        if(data_saved == packet_count * 15)
        {
            printf("Almacenamiento finalizado.\n");
        }
    }
}

void* task_rx(void* p)
{
    writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_RX_LENGTH);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
    writeRegister(REG_OP_MODE, 0x8D);

    while(1)
    {
        sem_wait(&lora_rx);

        uint8_t irq_flags = readRegister(REG_IRQ_FLAGS);

        if(irq_flags & IRQ_RX_DONE_MASK)
	    {
            writeRegister(REG_IRQ_FLAGS, 0xFF);

		    if(irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK)
		    {
			    crc_count++;
//			    printf("\rError de CRC. %i", crc_count);
//			    fflush(stdout);
                continue;
		    }

            pthread_mutex_lock(&buffer_mutex); 

            writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

            for(int i = 0 ; i < PACKET_SIZE ; i++)
            {
                active_buffer->data[i] = readRegister(REG_FIFO);
//              printf("%02X", buffer[i]);
            }
//		    printf("\n");
//		    printf("RSSI: %d\n", packetRssi());

            active_buffer->index = PACKET_SIZE;

            // Swap buffers
            processing_buffer = active_buffer;

            if (active_buffer == &buffer_a)
            {
                active_buffer = &buffer_b;
            }
            else
            {
                active_buffer = &buffer_a;
            }

            active_buffer->index = 0;
            pthread_mutex_unlock(&buffer_mutex);
            sem_post(&buffer_full); // Señala que hay un buffer lleno
        }
    }
    return NULL;
}

void* task_tx(void)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {0x10, 0x01, 0x1E};
    //for(uint8_t i = 0; i < PAYLOAD_TX_LENGTH; i++)
    //{
    //    medicion[i] = i;
    //}
    //size_t medicion_len = sizeof(medicion) / sizeof(medicion[0]);

    writeRegister(REG_PAYLOAD_LENGTH, PAYLOAD_TX_LENGTH);

//    while(1)
//    {
        writeRegister(REG_FIFO_ADDR_PTR, 0x00);

        for(size_t i = 0 ; i < PAYLOAD_TX_LENGTH ; i++)
        {
            writeRegister(REG_FIFO, data[i]);
        }

        writeRegister(REG_OP_MODE, 0x8B);

        while(!(readRegister(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK))
        {
            delay(1);
        }

        printf("Paquete transmitido con exito.\n");
        writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

//        delay(10000);
//    }
}

int main(void)
{
    init_lora();

    pthread_t producer_thread, processor_thread, sender_thread;

    active_buffer = &buffer_a;
    active_buffer->index = 0;
    buffer_b.index = 0;

    pthread_mutex_init(&mpu_queue_mutex, NULL);
    sem_init(&mpu_queue_count, 0, 0);
//    sem_init(&queue_space_available, 0, PROCESSED_QUEUE_SIZE);


    pthread_mutex_init(&buffer_mutex, NULL);
    sem_init(&buffer_full, 0, 0);
    sem_init(&lora_rx, 0, 0);

    pinMode(PIN_DIO0, INPUT);
    if (wiringPiISR(PIN_DIO0, INT_EDGE_RISING, &packet_arrived_isr) < 0) {
        fprintf(stderr, "No se pudo configurar la ISR: %s\n", strerror(errno));
        return 1;
    }

    task_tx();

    pthread_create(&producer_thread, NULL, task_rx, NULL);
    pthread_create(&processor_thread, NULL, task_process_data, NULL);
    pthread_create(&sender_thread, NULL, task_send_mpu_to_influx, NULL);

    pthread_join(producer_thread, NULL);
    pthread_join(processor_thread, NULL);

    return 0;
}
