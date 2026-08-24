#ifndef CURL_CLIENT_H
#define CURL_CLIENT_H

#include <curl/curl.h>
#include "lora.h"

int write_mpu_influx(const mpu6050_data_t* data, int count);
int write_bme_influx(const bme280_data_t* data, int count);

#endif // CURL_CLIENT_H
