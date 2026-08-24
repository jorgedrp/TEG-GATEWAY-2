#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include <errno.h>
#include "lora.h"
#include "spi.h"

#define PAYLOAD_TX_LENGTH   0x05
#define CONFIG 0x08

void* task_tx(uint8_t sensor, uint8_t frecuency_rate, uint8_t lora_mode)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {sensor, CONFIG, frecuency_rate, lora_mode, 0xFF};

    int intentos = 0;

    while(intentos < 5)
    {
        send_packet(data, PAYLOAD_TX_LENGTH);

        if(single_receive_packet(sensor, ACKNOWLEDGEMENT))
        {
            printf("Configuración establecida con éxito en el sensor %u.\n", sensor);
            fflush(stdout);
            break;
        }
        else
        {
            printf("No hubo respuesta del sensor. Reintentando... %i\n", intentos + 1);
            fflush(stdout);
        }

        intentos++;

        if(intentos == 5)
        {
            printf("No se pudo establecer comunicación con el sensor %u.\n", sensor);
            fflush(stdout);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Error: Faltan argumentos.\n");
        return 1;
    }

    int sensor = atoi(argv[1]);
    int frecuency_rate = atoi(argv[2]);
    int lora_mode = atoi(argv[3]);

    init_lora();

    sem_init(&lora_irq, 0, 0);

    init_lora_interrupt();

    task_tx(sensor, frecuency_rate, lora_mode);

    sem_destroy(&lora_irq);

    return 0;
}
