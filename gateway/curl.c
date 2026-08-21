#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <curl/curl.h>

// --- Configuracion InfluxDB con soporte para variables de entorno ---
#define DEFAULT_INFLUXDB_URL   "http://localhost:8086"
#define DEFAULT_INFLUXDB_ORG   "UCV"
#define DEFAULT_INFLUXDB_BUCKET "lora_table"
#define DEFAULT_INFLUXDB_TOKEN  ""
#define MAX_LINE_SIZE           256
#define MAX_URL_SIZE            512

static void get_influx_endpoint(char *url_buffer, size_t max_size) {
    const char *env_write_url = getenv("INFLUXDB_WRITE_URL");
    if (env_write_url && strlen(env_write_url) > 0) {
        snprintf(url_buffer, max_size, "%s", env_write_url);
        return;
    }

    const char *base_url = getenv("INFLUXDB_URL");
    if (!base_url || strlen(base_url) == 0) {
        base_url = DEFAULT_INFLUXDB_URL;
    }

    const char *org = getenv("INFLUXDB_ORG");
    if (!org || strlen(org) == 0) {
        org = DEFAULT_INFLUXDB_ORG;
    }

    const char *bucket = getenv("INFLUXDB_BUCKET");
    if (!bucket || strlen(bucket) == 0) {
        bucket = DEFAULT_INFLUXDB_BUCKET;
    }

    snprintf(url_buffer, max_size, "%s/api/v2/write?org=%s&bucket=%s&precision=ms", base_url, org, bucket);
}

static const char* get_influx_token(void) {
    const char *token = getenv("INFLUXDB_TOKEN");
    if (token && strlen(token) > 0) {
        return token;
    }
    return DEFAULT_INFLUXDB_TOKEN;
}

int write_mpu_influx(const mpu6050_data_t* data, int count)
{
    if (count <= 0 || !data) return 0;

    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    if (!post_data) {
        fprintf(stderr, "Error: Memoria insuficiente para buffer de InfluxDB\n");
        return -1;
    }
    post_data[0] = '\0';
    char *p = post_data;

    // Formatear los datos en el protocolo de línea de InfluxDB
    for (int i = 0; i < count; i++)
    {
        int written = snprintf(p, buffer_size - (p - post_data),
                 "mpu6050,device_id=%u ax=%.4f,ay=%.4f,az=%.4f,gx=%.4f,gy=%.4f,gz=%.4f %llu\n",
                 data[i].dev_id, data[i].ax, data[i].ay, data[i].az,
                 data[i].gx, data[i].gy, data[i].gz, (unsigned long long)data[i].timestamp);
        
        p += written;
    }

    curl = curl_easy_init();
    if(curl)
    {
        char target_url[MAX_URL_SIZE];
        get_influx_endpoint(target_url, sizeof(target_url));
        const char *token = get_influx_token();

        struct curl_slist *headers = NULL;
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", token);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
            } else {
                fprintf(stderr, "InfluxDB respondió con código HTTP %ld\n", response_code);
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(post_data);
    return result;
}

int write_bme_influx(const bme280_data_t* data, int count)
{
    if (count <= 0 || !data) return 0;

    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    if (!post_data) {
        fprintf(stderr, "Error: Memoria insuficiente para buffer de InfluxDB\n");
        return -1;
    }
    post_data[0] = '\0';
    char *p = post_data;

    // Formatear los datos en el protocolo de línea de InfluxDB
    for (int i = 0; i < count; i++)
    {   
        int written = snprintf(p, buffer_size - (p - post_data),
                 "bme280,device_id=%u temperatura=%.1f,presion=%.1f,humedad=%.1f %llu\n",
                 data[i].dev_id, data[i].temperature, data[i].pressure, data[i].humidity,
                 (unsigned long long)data[i].timestamp);
        
        p += written;
    }

    curl = curl_easy_init();
    if(curl)
    {
        char target_url[MAX_URL_SIZE];
        get_influx_endpoint(target_url, sizeof(target_url));
        const char *token = get_influx_token();

        struct curl_slist *headers = NULL;
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Token %s", token);
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
            } else {
                fprintf(stderr, "InfluxDB respondió con código HTTP %ld\n", response_code);
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(post_data);
    return result;
}
