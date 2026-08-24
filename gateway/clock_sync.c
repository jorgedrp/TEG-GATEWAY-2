#include "clock.h"
#include "lora.h"
#include "spi.h"
#include <stdio.h>
#include <errno.h>

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
