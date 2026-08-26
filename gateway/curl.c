#include "curl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#define DEFAULT_INFLUXDB_URL "http://localhost:8086"
#define DEFAULT_INFLUXDB_ORG "UCV"
#define DEFAULT_INFLUXDB_BUCKET "lora_table"
#define MAX_LINE_SIZE 256

static char g_influx_write_url[512] = {0};
static char g_influx_auth_header[512] = {0};
static int g_config_initialized = 0;

static void trim(char *s)
{
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    // Remover comillas envolventes si existen ("..." o '...')
    if (len >= 2 && ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))) {
        s[len - 1] = '\0';
        memmove(s, s + 1, len - 1);
    }
}

static void load_env_file(const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '#' || *p == '\0') continue;

        char *eq = strchr(p, '=');
        if (eq) {
            *eq = '\0';
            char *key = p;
            char *val = eq + 1;
            trim(key);
            trim(val);
            if (strlen(key) > 0) {
                // Solo define si la variable no existía en el entorno
                setenv(key, val, 0);
            }
        }
    }
    fclose(f);
}

static void init_influx_config(void)
{
    if (g_config_initialized) return;

    // 1. Si no están en el entorno, buscar archivo .env
    if (!getenv("INFLUXDB_TOKEN")) {
        load_env_file(".env");
        load_env_file("../.env");
    }

    const char *url = getenv("INFLUXDB_URL");
    const char *token = getenv("INFLUXDB_TOKEN");
    const char *org = getenv("INFLUXDB_ORG");
    const char *bucket = getenv("INFLUXDB_BUCKET");

    if (!url || strlen(url) == 0) url = DEFAULT_INFLUXDB_URL;
    if (!org || strlen(org) == 0) org = DEFAULT_INFLUXDB_ORG;
    if (!bucket || strlen(bucket) == 0) bucket = DEFAULT_INFLUXDB_BUCKET;
    if (!token) token = "";

    // 2. Construir la URL completa de escritura de InfluxDB v2
    if (strstr(url, "/api/v2/write")) {
        snprintf(g_influx_write_url, sizeof(g_influx_write_url), "%s", url);
    } else {
        char base_url[256];
        snprintf(base_url, sizeof(base_url), "%s", url);
        size_t len = strlen(base_url);
        if (len > 0 && base_url[len - 1] == '/') {
            base_url[len - 1] = '\0';
        }
        snprintf(g_influx_write_url, sizeof(g_influx_write_url),
                 "%s/api/v2/write?org=%s&bucket=%s&precision=ms",
                 base_url, org, bucket);
    }

    // 3. Construir cabecera de autorización
    snprintf(g_influx_auth_header, sizeof(g_influx_auth_header), "Authorization: Token %s", token);

    g_config_initialized = 1;
}

int write_mpu_influx(const mpu6050_data_t* data, int count)
{
    init_influx_config();

    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    if (!post_data) return -1;
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
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, g_influx_auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, g_influx_write_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
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
    init_influx_config();

    CURL *curl;
    CURLcode res;
    int result = -1;

    size_t buffer_size = count * MAX_LINE_SIZE;
    char *post_data = malloc(buffer_size);
    if (!post_data) return -1;
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
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, g_influx_auth_header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");

        curl_easy_setopt(curl, CURLOPT_URL, g_influx_write_url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() falló: %s\n", curl_easy_strerror(res));
        } else {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code >= 200 && response_code < 300) {
                result = 0; // Éxito
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(post_data);
    return result;
}
