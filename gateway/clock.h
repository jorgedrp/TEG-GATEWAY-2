#ifndef CLOCK_H
#define CLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "lora.h"

#define CMD_TIME_SYNC_REQUEST       0xA1 // ESP32 -> RPi
#define CMD_TIME_SYNC_RESPONSE      0xA2 // RPi -> ESP32
#define SIMPLE_ADJUST_ATTEMPTS      20
#define COMPLETE_ADJUST_ATTEMPTS    120
#define TEST_MODE_ATTEMPTS          720
#define CLOCK_SYNC                  0x10
#define SIMPLE_ADJUST               0x01
#define COMPLETE_ADJUST             0x02
#define TEST_MODE                   0x03

typedef struct __attribute__((packed)) {
    uint8_t command;
    uint8_t t2_bytes[8]; // Timestamp T2 como 8 bytes big-endian
    uint8_t t3_bytes[8]; // Timestamp T3 como 8 bytes big-endian
} time_sync_response_packet_t;

static inline uint64_t rpi_timer_get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000ULL);
}

static inline uint64_t tv_to_us(const struct timeval *tv)
{
    return (uint64_t)tv->tv_sec * 1000000L + tv->tv_usec;
}

static inline void encode_u64_be(uint8_t *dest, uint64_t value)
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

int handle_time_sync_cycle(uint8_t sensor);

#endif // CLOCK_H
