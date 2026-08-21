#include "spi.h"

int spi_transaction(uint8_t *tx_data, uint8_t *rx_data, size_t len) {

    int fd;
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 8000000;

    // Abrir dispositivo SPI
    fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("No se pudo abrir el dispositivo SPI");
        return 1;
    }

    // Configurar SPI
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    ioctl(fd, SPI_IOC_RD_MODE, &mode);
    ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);

    // Estructura de transferencia
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx_data,
        .rx_buf = (unsigned long)rx_data,
        .len = len,
        .delay_usecs = 0,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    // Transferencia SPI
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 1) {
        perror("Error en la transferencia SPI");
        close(fd);
        return 1;
    }

    // Mostrar resultados
//    printf("TX: ");
//    for (int i = 0; i < sizeof(tx_data); i++) {
//        printf("0x%02X ", tx_data[i]);
//    }
//    printf("\nRX: ");
//    for (int i = 0; i < sizeof(rx_data); i++) {
//        printf("0x%02X ", rx_data[i]);
//    }
//    printf("\n");

    close(fd);
    return 0;
}

//void main(void)
//{
//    uint8_t dataOut[8] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11};
//    uint8_t dataIn[8];
//
//    spi_transaction(dataOut, dataIn);
//}
