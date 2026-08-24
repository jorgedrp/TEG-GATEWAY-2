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

    // Cerrar dispositivo
    close(fd);
    return 0;
}
