#ifndef SPI_H
#define SPI_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE "/dev/spidev0.0"

int spi_transaction(uint8_t *tx_data, uint8_t *rx_data, size_t len);

#endif // SPI_H
