#include "curl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>

// --- Configuracion InfluxDB ---
#define INFLUXDB_URL "http://localhost:8086/api/v2/write?org=UCV&bucket=lora_table&precision=ms"
#define INFLUXDB_TOKEN "X4PSSQwUe4Msqf7tfB1TzHwEZ_DKBscyp4f1djeN3K4xOr7F1-HE9wzavbaCLIkoX27JtpCtIi_Akp-HQrWfZQ=="
#define MAX_LINE_SIZE 256

int write_mpu_influx(const mpu6050_data_t* data, int count)
{
    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    post_data[0] = '\0';
    char *p = post_data;

        // Formatear los datos en el protocolo de línea de InfluxDB
    for (int i = 0; i < count; i++)
    {
        int written = snprintf(p, buffer_size - (p - post_data),
                 "mpu6050,device_id=%u ax=%.4f,ay=%.4f,az=%.4f,gx=%.4f,gy=%.4f,gz=%.4f %llu\n",
                 data[i].dev_id, data[i].ax, data[i].ay, data[i].az,
                 data[i].gx, data[i].gy, data[i].gz, (unsigned long long)data[i].timestamp);
        
        // Mover el puntero para la siguiente línea
        p += written;
    }

    curl = curl_easy_init();
    if(curl)
    {
        // Configurar la cabecera de autenticación
        struct curl_slist *headers = NULL;
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", INFLUXDB_TOKEN);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        // Configurar la solicitud cURL
        curl_easy_setopt(curl, CURLOPT_URL, INFLUXDB_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Realizar la solicitud
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            //printf("Respuesta del servidor InfluxDB: %ld\n", response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
            }
        }

        // Limpieza
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(post_data);

    return result;
}

int write_bme_influx(const bme280_data_t* data, int count)
{
    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    post_data[0] = '\0';
    char *p = post_data;

        // Formatear los datos en el protocolo de línea de InfluxDB
    for (int i = 0; i < count; i++)
    {   
        int written = snprintf(p, buffer_size - (p - post_data),
                 "bme280,device_id=%u temperatura=%.1f,presion=%.1f,humedad=%.1f %llu\n",
                 data[i].dev_id, data[i].temperature, data[i].pressure, data[i].humidity,
                 (unsigned long long)data[i].timestamp);
        
        // Mover el puntero para la siguiente línea
        p += written;
    }

    curl = curl_easy_init();
    if(curl)
    {
        // Configurar la cabecera de autenticación
        struct curl_slist *headers = NULL;
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", INFLUXDB_TOKEN);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        // Configurar la solicitud cURL
        curl_easy_setopt(curl, CURLOPT_URL, INFLUXDB_URL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // Realizar la solicitud
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            //printf("Respuesta del servidor InfluxDB: %ld\n", response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
            }
        }

        // Limpieza
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(post_data);

    return result;
}