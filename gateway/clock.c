#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include "lora.c"
#include "spi.c"

#define CMD_TIME_SYNC_REQUEST       0xA1 // ESP32 -> RPi
#define CMD_TIME_SYNC_RESPONSE      0xA2 // RPi -> ESP32
#define SIMPLE_ADJUST_ATTEMPTS      20
#define COMPLETE_ADJUST_ATTEMPTS    120
#define TEST_MODE_ATTEMPTS          720
#define CLOCK_SYNC                  0x10
#define PAYLOAD_TX_LENGTH           0x05
#define SIMPLE_ADJUST               0x01
#define COMPLETE_ADJUST             0x02
#define TEST_MODE                   0x03

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t t2_bytes[8]; // Timestamp T2 como 8 bytes big-endian
    uint8_t t3_bytes[8]; // Timestamp T3 como 8 bytes big-endian
} time_sync_response_packet_t;

sensor_data_t sensor_list[SENSOR_NUM];
uint8_t num_sensores = 1;
const uint8_t sensores[4] = {0x10, 0x20, 0x30, 0x40};

static inline uint64_t rpi_timer_get_time_us(void)
{
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000ULL);
}

static inline uint64_t tv_to_us(const struct timeval *tv)
{
    return (uint64_t)tv->tv_sec * 1000000L + tv->tv_usec;
}

void encode_u64_be(uint8_t *dest, uint64_t value)
{
    dest[0] = (value >> 56) & 0xFF;
    dest[1] = (value >> 48) & 0xFF;
    dest[2] = (value >> 40) & 0xFF;
    dest[3] = (value >> 32) & 0xFF;
    dest[4] = (value >> 24) & 0xFF;
    dest[5] = (value >> 16) & 0xFF;
    dest[6] = (value >> 8) & 0xFF;
    dest[7] = (value >> 0) & 0xFF;
}

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
                printf("No se cambió de modo. Reintentando... %i\n", intentos + 1);
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

int handle_time_sync_cycle(uint8_t sensor)
{
    uint8_t data[PAYLOAD_TX_LENGTH] = {sensor, CMD_TIME_SYNC_RESPONSE, 0xFF, 0xFF, 0xFF};
    send_packet(data, sizeof(data)/sizeof(data[0]));

    writeRegister(REG_PAYLOAD_LENGTH, 0x02);
    writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    writeRegister(REG_OP_MODE, 0x8D);

    struct timespec ts;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;

    if (sem_timedwait(&lora_irq, &ts) == -1)
    {
        return 0; // Timeout
    }

    extern volatile uint64_t t2_hardware_us;
    uint64_t t2_us = t2_hardware_us;

    uint8_t rx_irq_flags = readRegister(REG_IRQ_FLAGS);
    writeRegister(REG_IRQ_FLAGS, 0xFF);

    if ((rx_irq_flags & IRQ_RX_DONE_MASK) && !(rx_irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK))
    {
        writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

        uint8_t dev_id = readRegister(REG_FIFO);
        uint8_t comm = readRegister(REG_FIFO);

        if ((dev_id == sensor) && (comm == CMD_TIME_SYNC_REQUEST))
        {
            time_sync_response_packet_t response_packet;
            response_packet.command = CMD_TIME_SYNC_RESPONSE;
            encode_u64_be(response_packet.t2_bytes, t2_us);
            size_t response_size = sizeof(response_packet);

            writeRegister(REG_PAYLOAD_LENGTH, response_size + 1);
            writeRegister(REG_DIO_MAPPING_1, 0x40); // DIO0 = TxDone
            writeRegister(REG_FIFO_ADDR_PTR, 0x00);
            writeRegister(REG_FIFO, dev_id);

            uint64_t t3_us = rpi_timer_get_time_us();
            encode_u64_be(response_packet.t3_bytes, t3_us);

            const uint8_t *packet_ptr = (const uint8_t *)&response_packet;

            for (size_t j = 0; j < response_size; j++)
            {
                writeRegister(REG_FIFO, packet_ptr[j]);
            }

            writeRegister(REG_OP_MODE, 0x8B);
            
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;

            if (sem_timedwait(&lora_irq, &ts) == -1)
            {
                return 0; // Timeout
            }
            
            writeRegister(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

            return 1;
        }
    }
    return 0;
}

void adjust_clock(void)
{
    for(size_t i = 0 ; i < num_sensores ; i++)
    {
        for (size_t k = 0; k < SIMPLE_ADJUST_ATTEMPTS ; k++)
        {
            if (handle_time_sync_cycle(sensor_list[i].dev_id) == 0)
            {
                printf("Timeout. Sensor: %u | Ciclo: %d. Reintentando...\n", sensor_list[i].dev_id, k + 1);
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
        if (handle_time_sync_cycle(sensor_list[k].dev_id) == 0)
        {
            printf("Timeout. Sensor: %u | Ciclo: %d. Reintentando...\n", sensor_list[k].dev_id, i + 1);
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
            if (handle_time_sync_cycle(sensor_list[i].dev_id) == 0)
            {
                printf("Timeout. Sensor: %u | Ciclo: %d. Reintentando...\n", sensor_list[i].dev_id, k + 1);
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