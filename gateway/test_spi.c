#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "lora.c"
#include "spi.c"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf("   DIAGNÓSTICO DE CONEXIÓN SPI - MÓDULO LORA RA-02  \n");
    printf("====================================================\n\n");

    // 1. Verificar si el dispositivo /dev/spidev0.0 existe en el sistema
    printf("[1/4] Verificando presencia del dispositivo SPI (%s)...\n", SPI_DEVICE);
    int fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "  [ERROR] No se pudo abrir %s: %s (errno: %d)\n\n", SPI_DEVICE, strerror(errno), errno);
        if (errno == ENOENT) {
            fprintf(stderr, "  -> CAUSA: El bus SPI está DESACTIVADO en el Raspberry Pi.\n");
            fprintf(stderr, "  -> SOLUCIÓN: Ejecuta 'sudo raspi-config' -> Interface Options -> SPI -> Enable -> Finish, o ejecuta:\n");
            fprintf(stderr, "               sudo raspi-config nonint do_spi 0\n");
            fprintf(stderr, "               y luego reinicia el Raspberry Pi.\n\n");
        } else if (errno == EACCES) {
            fprintf(stderr, "  -> CAUSA: Permisos insuficientes para acceder al bus SPI.\n");
            fprintf(stderr, "  -> SOLUCIÓN: Agrega tu usuario al grupo spi:\n");
            fprintf(stderr, "               sudo usermod -aG spi $USER && newgrp spi\n\n");
        }
        return 1;
    }
    close(fd);
    printf("  [OK] Dispositivo %s disponible y accesible con permisos de lectura/escritura.\n\n", SPI_DEVICE);

    // 2. Inicializar GPIOs de Reset e interrupción
    printf("[2/4] Ejecutando pulso de RESET por GPIO y configurando registros LoRa...\n");
    init_lora();
    printf("  [OK] Inicialización de hardware completada.\n\n");

    // 3. Leer registro de versión de silicio (0x42)
    printf("[3/4] Leyendo registro de versión Semtech SX1278 (REG_VERSION = 0x42)...\n");
    uint8_t version = readRegister(REG_VERSION);
    printf("  -> Valor leído del registro 0x42: 0x%02X (Esperado: 0x12)\n", version);

    if (version == 0x12) {
        printf("  [ÉXITO] ¡Módulo Semtech SX1278 (RA-02) detectado y comunicando al 100%% por SPI!\n\n");
    } else {
        fprintf(stderr, "  [FALLO] No se obtuvo el valor esperado 0x12.\n\n");
        if (version == 0x00) {
            fprintf(stderr, "  -> DIAGNÓSTICO: La línea MISO está en nivel BAJO permanente (0x00).\n");
            fprintf(stderr, "     Posibles causas:\n");
            fprintf(stderr, "     1. El módulo LoRa no tiene alimentación de 3.3V (VCC desconectado o GND suelto).\n");
            fprintf(stderr, "     2. El pin MISO (Pin Físico 21 / GPIO 9) no hace buen contacto.\n");
            fprintf(stderr, "     3. El pin NSS/CS (Pin Físico 24 / GPIO 8) no está bajando a nivel bajo durante la transmisión.\n\n");
        } else if (version == 0xFF) {
            fprintf(stderr, "  -> DIAGNÓSTICO: La línea MISO está en nivel ALTO permanente (0xFF) o flotante.\n");
            fprintf(stderr, "     Posibles causas:\n");
            fprintf(stderr, "     1. El pin MOSI (Pin Físico 19 / GPIO 10) o SCK (Pin Físico 23 / GPIO 11) no están conectados.\n");
            fprintf(stderr, "     2. Pines MOSI y MISO invertidos.\n");
            fprintf(stderr, "     3. Módulo LoRa en reset continuo (Pin Físico 22 / GPIO 25 a masa).\n\n");
        } else {
            fprintf(stderr, "  -> DIAGNÓSTICO: Se leyó 0x%02X. Puede haber ruido en las líneas SPI o cables largos/sueltos.\n\n", version);
        }
        fprintf(stderr, "  [TABLA DE CONEXIÓN FÍSICA RASPBERRY PI 40-PIN <-> RA-02]:\n");
        fprintf(stderr, "  +-------------------+----------------------+-------------------+\n");
        fprintf(stderr, "  | Señal LoRa RA-02  | Pin Físico RPi (HDR) | Función / GPIO    |\n");
        fprintf(stderr, "  +-------------------+----------------------+-------------------+\n");
        fprintf(stderr, "  | 3.3V (VCC)        | Pin 1 o Pin 17       | 3.3V Power (¡NO 5V!)|\n");
        fprintf(stderr, "  | GND               | Pin 6, 9, 14, 20, 25 | Ground            |\n");
        fprintf(stderr, "  | MISO              | Pin 21               | GPIO 9 (SPI0 MISO)|\n");
        fprintf(stderr, "  | MOSI              | Pin 19               | GPIO 10(SPI0 MOSI)|\n");
        fprintf(stderr, "  | SCK / CLK         | Pin 23               | GPIO 11(SPI0 SCLK)|\n");
        fprintf(stderr, "  | NSS / CS          | Pin 24               | GPIO 8 (SPI0 CE0) |\n");
        fprintf(stderr, "  | RST / RESET       | Pin 22               | GPIO 25 (RESET)   |\n");
        fprintf(stderr, "  | DIO0              | Pin 18               | GPIO 24 (IRQ Rx)  |\n");
        fprintf(stderr, "  +-------------------+----------------------+-------------------+\n\n");
    }

    // 4. Volcado de todos los registros del SX1278
    printf("[4/4] Volcado de registros internos del SX1278 (0x00 - 0x3F):\n");
    lora_dump_registers();

    if (version == 0x12) {
        printf("Diagnóstico final: CONEXIÓN SPI OPERATIVA.\n");
        return 0;
    } else {
        printf("Diagnóstico final: REVISAR CABLEADO O ACTIVAR SPI.\n");
        return 1;
    }
}
