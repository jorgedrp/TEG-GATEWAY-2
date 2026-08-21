#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h> // Para el manejo de señales (Ctrl+C)
#include "lora.c"
#include "spi.c"

#define PAYLOAD_TX_LENGTH   0x05
#define MODO_STANDBY        0x04

pthread_t detect_thread;

volatile sig_atomic_t keep_running = 1;

sem_t lora_irq;
size_t num_sensores = 1;

void sigint_handler(int sig) {
    printf("\nSeñal de interrupción (Ctrl+C) recibida. Iniciando cierre limpio...\n");
    fflush(stdout);
    keep_running = 0;
}

void retardo_milisegundos(long milisegundos) {
    struct timespec req, rem;

    // Convertir milisegundos a segundos y nanosegundos
    req.tv_sec = milisegundos / 1000;
    req.tv_nsec = (milisegundos % 1000) * 1000000L;

    // nanosleep devuelve -1 si es interrumpida por una señal
    while (nanosleep(&req, &rem) == -1) {
        // Si fue interrumpida, 'rem' contiene el tiempo restante.
        // Actualizamos 'req' con ese tiempo y volvemos a dormir.
        req = rem;
    }
}

void* task_detect(void *p)
{
    sensor_data_t* sensor_list = (sensor_data_t*)p;
    size_t k = 0;

    while(keep_running)
    {
        retardo_milisegundos(10000);

        uint8_t data[PAYLOAD_TX_LENGTH] = {sensor_list[k].dev_id, STATUS_CODE, 0xFF, 0xFF, 0xFF};
        send_packet(data, PAYLOAD_TX_LENGTH);

        if (single_receive_packet(sensor_list[k].dev_id, STATUS_CODE))
        {
            uint8_t modo = readRegister(REG_FIFO);
        
            if(modo == 0x04)
            {
                printf("STATUS:%u:STANDBY\n", sensor_list[k].dev_id);
                fflush(stdout);
            }
            else
            {
                printf("Sensor %u detectado pero no en modo Standby.\n", sensor_list[k].dev_id);
                fflush(stdout);
            }
        }
        else
        {
            printf("STATUS:%u:OFF\n", sensor_list[k].dev_id);
            fflush(stdout);
        }

        k = (k + 1) % num_sensores;
    }

    return NULL;
}

void task_tx(sensor_data_t *sensor_list, size_t num_sensores)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {BROADCAST_ID, MODO_STANDBY, 0xFF, 0xFF, 0xFF};

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
                printf("STATUS:%u:STANDBY", sensor_list[0].dev_id);
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
            break;
        }

        intentos++;

        if(intentos == 5)
        {
            printf("No se pudo establecer comunicación con el sensor %u.\n", sensor_list[0].dev_id);
            fflush(stdout);
            printf("STATUS:%u:OFF", sensor_list[0].dev_id);
            fflush(stdout);
        }
    }

    pthread_create(&detect_thread, NULL, task_detect, (void*)sensor_list);
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
            sensor.dev_id = atoi(argv[i + 2]);

            sensor_list[i] = sensor;
        }
        num_sensores = SENSOR_NUM;
    }
    else
    {
        sensor_list[0] = sensor;
        num_sensores = 1;
    }

    init_lora();
    sem_init(&lora_irq, 0, 0);

    if (wiringPiISR(PIN_DIO0, INT_EDGE_RISING, &packet_isr) < 0) {
        fprintf(stderr, "No se pudo configurar la ISR: %s\n", strerror(errno));
        fflush(stdout);
        return 1;
    }

    task_tx(sensor_list, num_sensores);

    while (keep_running) {
        sleep(1);
    }

    printf("Iniciando secuencia de apagado en main...\n");
    fflush(stdout);

    sem_post(&lora_irq);

    pthread_join(detect_thread, NULL);

    printf("Todos los hilos han finalizado.\n");
    fflush(stdout);

    sem_destroy(&lora_irq);

    return 0;
}
