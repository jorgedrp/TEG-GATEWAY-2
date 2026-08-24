#ifndef LORA_H
#define LORA_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <semaphore.h>

/*--REGISTROS--*/
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_OCP                  0x0b
#define REG_LNA                  0x0c
#define REG_FIFO_ADDR_PTR        0x0d
#define REG_FIFO_TX_BASE_ADDR    0x0e
#define REG_FIFO_RX_BASE_ADDR    0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_PKT_SNR_VALUE        0x19
#define REG_PKT_RSSI_VALUE       0x1a
#define REG_RSSI_VALUE           0x1b
#define REG_MODEM_CONFIG_1       0x1d
#define REG_MODEM_CONFIG_2       0x1e
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_FREQ_ERROR_MSB       0x28
#define REG_FREQ_ERROR_MID       0x29
#define REG_FREQ_ERROR_LSB       0x2a
#define REG_RSSI_WIDEBAND        0x2c
#define REG_DETECTION_OPTIMIZE   0x31
#define REG_INVERTIQ             0x33
#define REG_DETECTION_THRESHOLD  0x37
#define REG_SYNC_WORD            0x39
#define REG_INVERTIQ2            0x3b
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4d

/*--MODOS--*/
#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x08
#define MODE_STDBY               0x09
#define MODE_TX                  0x0B
#define MODE_RX_CONTINUOUS       0x0D
#define MODE_RX_SINGLE           0x0E
#define MODE_CAD                 0x0F

// PA config
#define PA_BOOST                 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK           0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK           0x40
#define IRQ_CAD_DONE_MASK          0x04
#define IRQ_CAD_DETECTED_MASK      0x01

#define RF_MID_BAND_THRESHOLD      525E6
#define RSSI_OFFSET_HF_PORT        157
#define RSSI_OFFSET_LF_PORT        164

#define MAX_PKT_LENGTH             255

#define PA_OUTPUT_RFO_PIN          0
#define PA_OUTPUT_PA_BOOST_PIN     1

#define PIN_NUM_RESET 25
#define PIN_NUM_CS 8
#define PIN_DIO0 24

#define PAYLOAD_RX_LENGTH   0xF3
#define PAYLOAD_TX_LENGTH   0x05

#define SENSOR_NUM      4
#define MPU_ID          0x01
#define BME_ID          0x02
#define ACKNOWLEDGEMENT 0xFB
#define STATUS_CODE     0xFC
#define DATA_AVAILABLE  0xFE
#define DOWNLOAD_DATA   0xFD
#define BROADCAST_ID    0xFF
#define DATA_LOSS       0xF9
#define DATA_CHANNEL    3
#define SIGNAL_CHANNEL  6

typedef struct {
    uint8_t sf;
    uint32_t bw;
    uint8_t txpow;
    uint8_t implicit;
    uint32_t frequency;
} sx1278_config_t;

typedef struct __attribute__((packed)) {
    uint8_t timestamp_l;
    uint8_t timestamp_m;
    uint8_t timestamp_h;
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} mpu6050_dataraw_t;

typedef struct {
    uint8_t dev_id;
    uint64_t timestamp;
    float ax, ay, az;
    float gx, gy, gz;
} mpu6050_data_t;

typedef struct __attribute__((packed)) {
    uint8_t timestamp_l;
    uint8_t timestamp_m;
    uint8_t timestamp_h;
    float temperature;
    float pressure;
    float humidity;
} bme280_dataraw_t;

typedef struct {
    uint8_t dev_id;
    uint64_t timestamp;
    float temperature;
    float pressure;
    float humidity;
} bme280_data_t;

typedef struct {
    int dev_id;
    int umbral;
    int lora_mode;
    int rate;
    int time;
    uint64_t timestamp;
} sensor_data_t;

// Variables globales compartidas
extern sem_t lora_irq;
extern volatile uint64_t t2_hardware_us;

// Prototipos de funciones
void config_lora(int mode);
void init_lora(void);
void init_lora_interrupt(void);
void retardo_milisegundos(long milisegundos);
int send_packet(uint8_t *data_buffer, size_t size);
int single_receive_packet(uint8_t sender_id, uint8_t command);
int8_t packetRssi(void);
float packetSnr(void);
int8_t rssi(void);
void idle(void);
void tsleep(void);
void setTxPower(uint8_t level, uint8_t outputPin);
void setFrequency(uint32_t frequency);
void setSpreadingFactor(uint8_t sf);
void setSignalBandwidth(uint32_t sbw);
int32_t getSignalBandwidth(void);
uint8_t getSpreadingFactor(void);
void setCodingRate(uint8_t denominator);
void setPreambleLength(uint16_t length);
void enableCrc(void);
void disableCrc(void);
void setOCP(uint8_t mA);
uint8_t readRegister(uint8_t address);
void writeRegister(uint8_t address, uint8_t value);
void lora_dump_registers(void);

#endif // LORA_H
