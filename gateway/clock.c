#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "clock.h"
#include "lora.h"
#include "spi.h"

sensor_data_t sensor_list[SENSOR_NUM];
uint8_t num_sensores = 1;
const uint8_t sensores[4] = {0x10, 0x20, 0x30, 0x40};

void send_initial_sync_trigger(uint8_t sensor, uint8_t mode)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {BROADCAST_ID, CLOCK_SYNC, mode, 0xFF, 0xFF};
    size_t num_activos = 0;

    for(size_t i = 0 ; i < num_sensores ; i++)
    {
        size_t intentos = 0;

        if(sensor == 255)
        {
            data[0] = sensores[i];
        }
        else
        {
            data[0] = sensor;
        }

        while(intentos < 5)
        {
            send_packet(data, PAYLOAD_TX_LENGTH);

            if(single_receive_packet(data[0], ACKNOWLEDGEMENT))
            {
                printf("Sensor %u listo para la sincronización.\n", data[0]);
                fflush(stdout);
                printf("STATUS:%u:CLOCK\n", data[0]);
                fflush(stdout);
                sensor_list[i].dev_id = data[0];
                num_activos++;
                break;
            }
            else
            {
                printf("No se cambió de modo. Reintentando... %zu\n", intentos + 1);
                fflush(stdout);
                intentos++;
                retardo_milisegundos(500);
            }
        }

        if(intentos == 5)
        {
            printf("No se pudo establecer comunicación con el sensor %u.\n", data[0]);
            fflush(stdout);
            printf("STATUS:%u:OFF\n", data[0]);
            fflush(stdout);
        }
    }
    num_sensores = num_activos;
}

void adjust_clock(void)
{
    for(size_t i = 0 ; i < num_sensores ; i++)
    {
        for (size_t k = 0; k < SIMPLE_ADJUST_ATTEMPTS ; k++)
        {
            if (handle_time_sync_cycle(sensor_list[i].dev_id, false) == 0)
            {
                printf("Timeout. Sensor: %u | Ciclo: %zu. Reintentando...\n", sensor_list[i].dev_id, k + 1);
                fflush(stdout);
            }

            retardo_milisegundos(1000);
        }
    }
}

void adjust_skew(void)
{
    size_t k = 0;

    for (size_t i = 0; i < COMPLETE_ADJUST_ATTEMPTS * num_sensores; i++)
    {
        if (handle_time_sync_cycle(sensor_list[k].dev_id, false) == 0)
        {
            printf("Timeout. Sensor: %u | Ciclo: %zu. Reintentando...\n", sensor_list[k].dev_id, i + 1);
            fflush(stdout);
        }
        k = (k + 1) % num_sensores;

        if(num_sensores == 1)
        {
            retardo_milisegundos(5000);
        }
        else
        {
            retardo_milisegundos((5 - num_sensores) * 1000);
        }
    }
}

void test_mode(void)
{
    for(size_t i = 0 ; i < num_sensores ; i++)
    {
        for (size_t k = 0; k < TEST_MODE_ATTEMPTS ; k++)
        {
            if (handle_time_sync_cycle(sensor_list[i].dev_id, false) == 0)
            {
                printf("Timeout. Sensor: %u | Ciclo: %zu. Reintentando...\n", sensor_list[i].dev_id, k + 1);
                fflush(stdout);
            }

            retardo_milisegundos(4000);
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

    uint8_t sensor = (uint8_t)atoi(argv[1]);
    uint8_t mode = (uint8_t)atoi(argv[2]);

    if(sensor == 255)
    {
        num_sensores = 4;
    }

    init_lora();
    sem_init(&lora_irq, 0, 0);

    init_lora_interrupt();

    send_initial_sync_trigger(sensor, mode);

    if(mode == SIMPLE_ADJUST)
    {
        adjust_clock();
    }
    else if(mode == COMPLETE_ADJUST)
    {
        adjust_skew();
    }
    else if(mode == TEST_MODE)
    {
        test_mode();
    }

    printf("Proceso de sincronización completado.\n");
    sem_destroy(&lora_irq);
    return 0;
}
