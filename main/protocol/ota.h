#pragma once

#define OTA_URL "https://api.tenclass.net/xiaozhi/ota/"

typedef struct {
    char* activation_code;
    char* websocket_url;
    char* websocket_token;
} ota_t;

ota_t* ota_create(void);
void ota_destroy(ota_t* ota);
void ota_perform(ota_t* ota);
