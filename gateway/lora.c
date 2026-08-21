#include "lora.h"
#include "spi.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <gpiod.h>

// Variable global que retiene el timestamp de hardware capturado por el Kernel
volatile uint64_t t2_hardware_us = 0;

#define GPIO_CHIP_NAME "gpiochip0" // En Raspberry Pi suele ser siempre gpiochip0

sem_t lora_irq;

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

static void* gpiod_irq_thread(void* arg)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;
    struct gpiod_line_event event;
    int rv;

    // 1. Abrir el controlador GPIO de la Raspberry Pi
    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        perror("Error abriendo gpiochip para libgpiod");
        pthread_exit(NULL);
    }

    // 2. Solicitar el control de la línea física (pin)
    line = gpiod_chip_get_line(chip, PIN_DIO0);
    if (!line) {
        perror("Error obteniendo la línea GPIO");
        gpiod_chip_close(chip);
        pthread_exit(NULL);
    }

    // 3. Configurar interrupción por flanco de subida (Rising Edge)
    // El Kernel sellará la hora en su búfer apenas ocurra el cambio de voltaje
    rv = gpiod_line_request_rising_edge_events(line, "lora_dio0_isr");
    if (rv < 0) {
        perror("Error solicitando eventos de flanco de subida en libgpiod");
        gpiod_chip_close(chip);
        pthread_exit(NULL);
    }

    // 4. Bucle infinito de escucha de alta prioridad
    while (1) {
        // Bloquea el hilo sin gastar CPU hasta que el hardware dispare el pin
        rv = gpiod_line_event_wait(line, NULL);
        if (rv < 0) {
            perror("Error esperando evento en gpiod_line_event_wait");
            continue;
        }

        // Leer el evento desde la cola del Kernel
        rv = gpiod_line_event_read(line, &event);
        if (rv == 0) {
            if (event.event_type == GPIOD_LINE_EVENT_RISING_EDGE) {

                // event.ts es la estructura timespec que capturó 
                // en el pasado exacto, en el reloj CLOCK_MONOTONIC.
                t2_hardware_us = (uint64_t)event.ts.tv_sec * 1000000ULL + (event.ts.tv_nsec / 1000ULL);

                // Despierta a la máquina de estados en el hilo principal
                sem_post(&lora_irq);
            }
        }
    }

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return NULL;
}

// Función para inicializar el hilo desde main()
void init_lora_interrupt(void)
{
    pthread_t irq_thread_id;
    if (pthread_create(&irq_thread_id, NULL, gpiod_irq_thread, NULL) != 0) {
        fprintf(stderr, "Error fatal al crear el hilo de interrupción para libgpiod\n");
        exit(EXIT_FAILURE);
    }
    // Desvincular el hilo para que corra independientemente en segundo plano
    pthread_detach(irq_thread_id);
}

sx1278_config_t lora_config = {
    .sf = 10,
    .bw = 125e3,
    .txpow = 20,
    .implicit = 0,
    .frequency = 433175000,
};

uint8_t _packetIndex = 0;
uint8_t lora_time_out = 2;

void config_lora(int mode)
{
    switch (mode)
    {
    case 1:
        lora_config.sf = 7;
        lora_config.bw = 125e3;
        lora_config.frequency = 433775000;
        lora_config.txpow = 17;
        lora_time_out = 2;
        break;
    case 2:
        lora_config.sf = 7;
        lora_config.bw = 250e3;
        lora_config.frequency = 433775000;
        lora_config.txpow = 17;
        lora_time_out = 2;
        break;
    case 3:
        lora_config.sf = 7;
        lora_config.bw = 500e3;
        lora_config.frequency = 433775000;
        lora_config.txpow = 17;
        lora_time_out = 2;
        break;
    case 4:
        lora_config.sf = 8;
        lora_config.bw = 125e3;
        lora_config.frequency = 433775000;
        lora_config.txpow = 17;
        lora_time_out = 2;
        break;
    case 5:
        lora_config.sf = 9;
        lora_config.bw = 125e3;
        lora_config.frequency = 433775000;
        lora_config.txpow = 17;
        lora_time_out = 2;
        break;
    case 6:
        lora_config.sf = 10;
        lora_config.bw = 125e3;
        lora_config.frequency = 433175000;
        lora_config.txpow = 20;
        lora_time_out = 2;
        break;
    case 7:
        lora_config.sf = 12;
        lora_config.bw = 125e3;
        lora_config.frequency = 433175000;
        lora_config.txpow = 20;
        lora_time_out = 6;
        break;
    }

    idle();

    if (mode == 3)
    {
        writeRegister(0x36, 0x02);
        writeRegister(0x3A, 0x64);
        setCodingRate(5);
        setPreambleLength(8);
    }
    else if (mode >= 6)
    {
        writeRegister(0x36, 0x03);
        writeRegister(0x3A, 0x65);
        setCodingRate(8);
        setPreambleLength(16);
    }
    else
    {
        writeRegister(0x36, 0x03);
        writeRegister(0x3A, 0x65);
        setCodingRate(5);
        setPreambleLength(8);
    }

    setSignalBandwidth(lora_config.bw);
    setSpreadingFactor(lora_config.sf);
    setFrequency(lora_config.frequency);
    setTxPower(lora_config.txpow, PA_OUTPUT_PA_BOOST_PIN);
}

void init_lora(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line *reset_line;

    // 1. Abrir el controlador GPIO nativo de la Raspberry Pi
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("Error abriendo gpiochip0 en init_lora");
        exit(EXIT_FAILURE);
    }

    // 2. Obtener el control exclusivo sobre la línea física del pin de Reset
    reset_line = gpiod_chip_get_line(chip, PIN_NUM_RESET);
    if (!reset_line) {
        perror("Error obteniendo la línea de RESET");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // 3. Configurar el pin como SALIDA con un estado inicial en ALTO (1) de forma atómica
    if (gpiod_line_request_output(reset_line, "lora_reset", 1) < 0) {
        perror("Error configurando el pin RESET como salida");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    retardo_milisegundos(200);

    gpiod_line_set_value(reset_line, 0); // digitalWrite LOW
    retardo_milisegundos(10);
    
    gpiod_line_set_value(reset_line, 1); // digitalWrite HIGH
    retardo_milisegundos(100);

    // 5. Liberar la línea GPIO y el controlador
    gpiod_line_release(reset_line);
    gpiod_chip_close(chip);

    tsleep();

    setFrequency(lora_config.frequency);
    setSignalBandwidth(lora_config.bw);
    setSpreadingFactor(lora_config.sf);
    setTxPower(lora_config.txpow, PA_OUTPUT_PA_BOOST_PIN);

    enableCrc();
    setCodingRate(8);
    setPreambleLength(16);

    writeRegister(REG_FIFO_TX_BASE_ADDR, 0x00);
    writeRegister(REG_FIFO_RX_BASE_ADDR, 0x00);
    writeRegister(REG_SYNC_WORD, 0x33);
    writeRegister(REG_MODEM_CONFIG_3, 0x00);
    writeRegister(REG_LNA, 0x20);

    idle();
}

int8_t packetRssi(void)
{
    return (readRegister(REG_PKT_RSSI_VALUE) - (lora_config.frequency < RF_MID_BAND_THRESHOLD ? RSSI_OFFSET_LF_PORT : RSSI_OFFSET_HF_PORT));
}

float packetSnr(void)
{
    return ((int8_t)readRegister(REG_PKT_SNR_VALUE)) * 0.25;
}

int8_t rssi(void)
{
    return (readRegister(REG_RSSI_VALUE) - (lora_config.frequency < RF_MID_BAND_THRESHOLD ? RSSI_OFFSET_LF_PORT : RSSI_OFFSET_HF_PORT));
}

void idle(void)
{
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void tsleep(void)
{
    writeRegister(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void setTxPower(uint8_t level, uint8_t outputPin)
{
    if (PA_OUTPUT_RFO_PIN == outputPin)
    {
        // RFO
        if (level > 14)
        {
            level = 14;
        }
        writeRegister(REG_PA_CONFIG, 0x70 | level);
    }
    else
    {
        // PA BOOST
        if (level > 17)
        {
            if (level > 20)
            {
                level = 20;
            }

            // subtract 3 from level, so 18 - 20 maps to 15 - 17
            level -= 3;

            // High Power +20 dBm Operation (Semtech SX1276/77/78/79 5.4.3.)
            writeRegister(REG_PA_DAC, 0x87);
            setOCP(140);
        }
        else
        {
            if (level < 2)
            {
                level = 2;
            }
            //Default value PA_HF/LF or +17dBm
            writeRegister(REG_PA_DAC, 0x84);
            setOCP(100);
        }

        writeRegister(REG_PA_CONFIG, PA_BOOST | (level - 2));
    }
}

void setFrequency(uint32_t frequency)
{
  uint64_t frf = ((uint64_t)frequency << 19) / 32000000;

  writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
  writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
  writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

void setSpreadingFactor(uint8_t sf)
{
    if (sf < 6)
    {
        sf = 6;
    }
    else if (sf > 12)
    {
        sf = 12;
    }

    if (sf == 6)
    {
        writeRegister(REG_DETECTION_OPTIMIZE, 0xc5);
        writeRegister(REG_DETECTION_THRESHOLD, 0x0c);
    }
    else
    {
        writeRegister(REG_DETECTION_OPTIMIZE, 0xc3);
        writeRegister(REG_DETECTION_THRESHOLD, 0x0a);
    }

    writeRegister(REG_MODEM_CONFIG_2, (readRegister(REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
    //setLdoFlag();
}

void setSignalBandwidth(uint32_t sbw)
{
    int bw;

    if (sbw <= 7.8E3)
    {
        bw = 0;
    }
    else if (sbw <= 10.4E3)
    {
        bw = 1;
    }
    else if (sbw <= 15.6E3)
    {
        bw = 2;
    }
    else if (sbw <= 20.8E3)
    {
        bw = 3;
    }
    else if (sbw <= 31.25E3)
    {
        bw = 4;
    }
    else if (sbw <= 41.7E3)
    {
        bw = 5;
    }
    else if (sbw <= 62.5E3)
    {
        bw = 6;
    }
    else if (sbw <= 125E3)
    {
        bw = 7;
    }
    else if (sbw <= 250E3)
    {
        bw = 8;
    }
    else /*if (sbw >= 250E3)*/
    {
        bw = 9;
    }

    writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0x0f) | (bw << 4));
    //setLdoFlag();
}

int32_t getSignalBandwidth(void)
{
    uint8_t bw = (readRegister(REG_MODEM_CONFIG_1) >> 4);

    switch (bw)
    {
        case 0: return 7.8E3;
        case 1: return 10.4E3;
        case 2: return 15.6E3;
        case 3: return 20.8E3;
        case 4: return 31.25E3;
        case 5: return 41.7E3;
        case 6: return 62.5E3;
        case 7: return 125E3;
        case 8: return 250E3;
        case 9: return 500E3;
    }

    return -1;
}

uint8_t getSpreadingFactor(void)
{
    return readRegister(REG_MODEM_CONFIG_2) >> 4;
}

// void setLdoFlag(void)
// {
//     // Section 4.1.1.5
//     long symbolDuration = 1000 / ( getSignalBandwidth() / (1L << getSpreadingFactor()) ) ;

//     // Section 4.1.1.6
//     boolean ldoOn = symbolDuration > 16;

//     uint8_t config3 = readRegister(REG_MODEM_CONFIG_3);
//     bitWrite(config3, 3, ldoOn);
//     writeRegister(REG_MODEM_CONFIG_3, config3);
// }

void setCodingRate(uint8_t denominator)
{
    if (denominator < 5)
    {
        denominator = 5;
    }
    else if (denominator > 8)
    {
        denominator = 8;
    }

    int cr = denominator - 4;

    writeRegister(REG_MODEM_CONFIG_1, (readRegister(REG_MODEM_CONFIG_1) & 0xf1) | (cr << 1));
}

void setPreambleLength(uint16_t length)
{
  writeRegister(REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
  writeRegister(REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

void enableCrc(void)
{
    writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) | 0x04);
}

void disableCrc(void)
{
    writeRegister(REG_MODEM_CONFIG_2, readRegister(REG_MODEM_CONFIG_2) & 0xfb);
}

void setOCP(uint8_t mA)
{
    uint8_t ocpTrim = 27;

    if (mA <= 120)
    {
        ocpTrim = (mA - 45) / 5;
    }
    else if (mA <=240)
    {
        ocpTrim = (mA + 30) / 10;
    }

    writeRegister(REG_OCP, 0x20 | (0x1F & ocpTrim));
}

uint8_t readRegister(uint8_t address)
{
    uint8_t out[2] = {address & 0x7f, 0x00};
    uint8_t in[2];
    spi_transaction(out, in, sizeof(out));
    return in[1];
}

void writeRegister(uint8_t address, uint8_t value)
{
    uint8_t out[2] = {0x80 | address, value};
    uint8_t in[2];
    spi_transaction(out, in, sizeof(out));
}

void lora_dump_registers(void)
{
   int i;
   printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
   for(i=0; i<0x40; i++) {
      printf("%02X ", readRegister(i));
      if((i & 0x0f) == 0x0f) printf("\n");
   }
   printf("\n");
}

int send_packet(uint8_t *data_buffer, size_t size)
{
    struct timespec ts;

    writeRegister(REG_PAYLOAD_LENGTH, size);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    writeRegister(REG_DIO_MAPPING_1, 0x40); // DIO0 = TxDone

    for(size_t i = 0 ; i < size ; i++)
    {
        writeRegister(REG_FIFO, data_buffer[i]);
    }

    writeRegister(REG_OP_MODE, 0x8B);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += lora_time_out; // Timeout de 2 segundos

    if (sem_timedwait(&lora_irq, &ts) == -1)
    {
        if (errno == ETIMEDOUT)
        {
            writeRegister(REG_OP_MODE, 0x89);
            printf("Error al enviar el paquete.\n");
            fflush(stdout);
            return 0;
        }
    }
    else
    {
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        return 1;
    }
}

int single_receive_packet(uint8_t sender_id, uint8_t command)
{
    struct timespec ts;

    writeRegister(REG_PAYLOAD_LENGTH, 0x05);
    writeRegister(REG_FIFO_ADDR_PTR, 0x00);
    writeRegister(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
    writeRegister(REG_OP_MODE, 0x8D);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += lora_time_out; // Timeout de 2 segundos

    if (sem_timedwait(&lora_irq, &ts) == -1)
    {
        if (errno == ETIMEDOUT)
        {
            writeRegister(REG_OP_MODE, 0x89);
            return 0;
        }
    }
    else
    {
        uint8_t rx_irq_flags = readRegister(REG_IRQ_FLAGS);
        writeRegister(REG_IRQ_FLAGS, rx_irq_flags);

        if((rx_irq_flags & IRQ_RX_DONE_MASK) != 0 && (rx_irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) == 0)
        {
            writeRegister(REG_FIFO_ADDR_PTR, readRegister(REG_FIFO_RX_CURRENT_ADDR));

            uint8_t dev_id = readRegister(REG_FIFO);
            uint8_t cmd = readRegister(REG_FIFO);

            if(dev_id == sender_id && cmd == command)
            {
                return 1;
            }
        }
    }

    return 0;
}