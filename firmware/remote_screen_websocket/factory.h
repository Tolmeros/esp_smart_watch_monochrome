#pragma once
#ifndef FACTORY_H
#define FACTORY_H

#include "defines.h"

#if !defined(ESP_WIFI_MODE)
  #define ESP_WIFI_MODE WIFI_STA_MODE
#endif

// --- ESP (WiFi клиент) ---------------

// --- AP (WiFi точка доступа) ---
/* имя WiFi точки доступа, используется как при запросе SSID и пароля WiFi
  сети роутера, так и при работе в режиме ESP_MODE = 0
*/
#if !defined(AP_NAME)
  #define AP_NAME               ("LedLamp")
#endif

// пароль WiFi точки доступа
#if !defined(AP_PASS)
  #define AP_PASS               ("31415926")
#endif

// статический IP точки доступа (лучше не менять)
#define AP_STATIC_IP_4        192
#define AP_STATIC_IP_3        168
#define AP_STATIC_IP_2        4
#define AP_STATIC_IP_1        1

#endif //FACTORY_H
