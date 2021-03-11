#pragma once
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#elif defined(ESP32)
  #include <WiFi.h>
#endif

#include "json_settings.h"

typedef uint64_t (*get_uptime_ms_func)(void);

struct listWiFiNetworks {
  char ssid[WIFI_CONFIG_SSID_MAX_LENGTH];
  int rssi;
  int id;
};

#define WIFIMRG_CONNECTION_TIMEOUT 10*1000
#define WIFIMRG_RECONNECTION_TIMEOUT 30*1000
#define WIFIMRG_TRY_CINNECT_TIMEOUT 15*1000

enum DisconnectedState {
  TRY_AP,
  TRY_STA,
  TRY_STA_IMMEDIATELY
};

enum TryAddStatus {
  try_add_failed=-1,
  try_add_idle=0,
  try_add_connected=1
};

class WiFiManager {
private:
  JsonSettingsWiFi *wifi_settings;
  get_uptime_ms_func get_uptime_ms;
  uint64_t connection_lost_time = 0;
  uint64_t try_connect_time = 0;
  bool was_disconnected;
  byte disconnected_state = TRY_STA_IMMEDIATELY;
  bool connection_process = false;
  bool first_run = true;
  bool try_add_start = false;
  bool try_add_run = false;
  char try_add_ssid[WIFI_CONFIG_SSID_MAX_LENGTH];
  char try_add_password[WIFI_CONFIG_PASS_MAX_LENGTH];
  bool try_add_hidden;
  byte try_add_tries;
  uint64_t try_add_time = 0;
  int8_t try_add_status = try_add_idle;
  bool connection_changed = false;

  #if defined(WIFI_MANAGER_DEBUG)
    bool _debug = true;
  #endif

  void switch_to_AP();
  void connect_to_SSID(const char* ssid, const char* password);
  void switch_station();
  void disconnect_all();

  #if defined(WIFI_MANAGER_DEBUG)
  template <typename Generic>
  void          DEBUG_WM(Generic text);
  #endif

public:
  WiFiManager(JsonSettingsWiFi& wifi_settings, get_uptime_ms_func get_uptime_ms);

  void handle();
  bool try_add(const char* ssid, const char* password, const bool hidden=false);
  int8_t get_try_add_status();
  bool get_connection_changed();
};
#endif //WIFI_MANAGER_H
