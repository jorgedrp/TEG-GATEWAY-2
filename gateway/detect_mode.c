#include <semaphore.h>
#include <errno.h>
#include "lora.c"
#include "spi.c"

#define PAYLOAD_TX_LENGTH   0x05
static const uint8_t sensor_list[] = {0x10, 0x20, 0x30, 0x40};

void task_tx(void)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {0x10, STATUS_CODE, 0xFF, 0xFF, 0xFF};
    int trys = 0;

    for (size_t i = 0; i < sizeof(sensor_list); i++)
    {
        data[0] = sensor_list[i];

        while (trys < 5)
        {
            send_packet(data, PAYLOAD_TX_LENGTH);

            if (single_receive_packet(sensor_list[i], STATUS_CODE))
            {
                uint8_t mode = readRegister(REG_FIFO);

                printf("Sensor %u detectado.\n", sensor_list[i]);
                fflush(stdout);

                switch (mode)
                {
                case 0x01:
                    printf("STATUS:%u:TIEMPO\n", sensor_list[i]);
                    fflush(stdout);
                    break;
                case 0x02:
                    printf("STATUS:%u:EVENTO\n", sensor_list[i]);
                    fflush(stdout);
                    break;
                case 0x04:
                    printf("STATUS:%u:STANDBY\n", sensor_list[i]);
                    fflush(stdout);
                    break;
                }

                break;
            }
            else
            {
                printf("No hubo respuesta del sensor %u. Reintentando... %i\n", sensor_list[i], trys + 1);
                fflush(stdout);
            }

            trys++;

            if (trys == 5)
            {
                printf("No se pudo establecer comunicacion con el sensor %u.\n", sensor_list[i]);
                fflush(stdout);
                printf("STATUS:%u:OFF\n", sensor_list[i]);
                fflush(stdout);
            }
        }

        trys = 0;
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    init_lora();
    sem_init(&lora_irq, 0, 0);

    init_lora_interrupt();

    task_tx();

    sem_destroy(&lora_irq);
    return 0;
}
