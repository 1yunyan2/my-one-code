#pragma once

#include <stdint.h>

void ui_init(void);

void ui_update_wifi(int rssi);

void ui_update_battery(int soc);

void ui_update_status(const char* status);

void ui_update_emotion(const char* emotion);

void ui_update_text(const char* text);

void ui_show_notification(const char* title, const char* message, uint32_t timeout_ms);

void ui_show_qrcode(const char* title, const char* content);
