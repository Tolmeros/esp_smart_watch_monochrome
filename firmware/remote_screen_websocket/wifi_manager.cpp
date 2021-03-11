#include "wifi_manager.h"
#include "defines.h"
#include "factory.h"

WiFiManager::WiFiManager(JsonSettingsWiFi& wifi_settings, get_uptime_ms_func get_uptime_ms) {
  #if defined(WIFI_MANAGER_DEBUG)
    DEBUG_WM(F("WiFiManager init"));
  #endif

  this->wifi_settings = &wifi_settings;
  this->get_uptime_ms = get_uptime_ms;
  this->was_disconnected = false;
  this->try_add_status = try_add_idle;
  this->connection_changed = false;

  WiFi.persistent(false);

  connection_lost_time = get_uptime_ms();

  handle();
}

void WiFiManager::handle() {
  #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
    DEBUG_WM(F("WiFiManager handle()"));
  #endif

  if (try_add_start) {
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager handle() try_add_start"));
    #endif
    disconnect_all();
    try_add_run = true;
    try_add_start = false;
    return;
  }

  if (try_add_run) {
    if (try_add_tries == 4) {
      #if defined(WIFI_MANAGER_DEBUG)
        DEBUG_WM(F("WiFiManager handle() try_add_run disconnect"));
      #endif
      WiFi.disconnect();
      try_add_tries--;
      try_add_time = get_uptime_ms();
      return;
    } else {
      if ((get_uptime_ms() - try_add_time < WIFIMRG_TRY_CINNECT_TIMEOUT)) {
        return;
      }
      if (WiFi.status() != WL_CONNECTED) {
        if ((get_uptime_ms() - try_add_time >= WIFIMRG_TRY_CINNECT_TIMEOUT)) {
          #if defined(WIFI_MANAGER_DEBUG)
            DEBUG_WM(F("WiFiManager handle() try_add_run not connected timeout"));
          #endif
          try_add_tries--;
          if (try_add_tries < 1) {
            try_add_run = false;
            try_add_status = try_add_failed;
          } else {
            try_add_time = get_uptime_ms();
            connect_to_SSID(try_add_ssid, try_add_password);
          }
        }
      return;

      } else {
        int8_t id = wifi_settings->getNetworkIdBySsid(try_add_ssid);

        if (id >=0) {
          #if defined(WIFI_MANAGER_DEBUG)
            DEBUG_WM(F("WiFiManager handle() try_add_run chaged"));
          #endif
          wifi_settings->changeNetworkPassword(id, try_add_password);
          wifi_settings->changeNetworkHidden(id, try_add_hidden);
        } else {
          #if defined(WIFI_MANAGER_DEBUG)
            DEBUG_WM(F("WiFiManager handle() try_add_run added"));
          #endif
          wifi_settings->addNetwork(
            try_add_ssid,
            try_add_password,
            try_add_hidden
          );
        }

        try_add_run = false;
        try_add_status = try_add_connected;
      }
    }
    
  }


  // если в настройках установлен режим AP
  // или если не заданы сети WiFi
  if ((wifi_settings->getEspWiFiMode() == WIFI_AP_MODE) ||
    (wifi_settings->getStoredNetworksCount() == 0)) {
    #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
      DEBUG_WM(F("WiFiManager handle() WIFI_AP_MODE or no networks"));
    #endif

    if (WiFi.getMode() != WIFI_AP) {
      switch_to_AP();
    }
    return;
  } else if (wifi_settings->getEspWiFiMode() == WIFI_STA_MODE) {
    #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
      DEBUG_WM(F("WiFiManager handle() WIFI_STA_MODE."));
    #endif

    if (WiFi.status() != WL_CONNECTED) {
      if ( connection_process && (get_uptime_ms() - try_connect_time < WIFIMRG_TRY_CINNECT_TIMEOUT)) {
        #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
          DEBUG_WM(F("WiFiManager handle() WIFI_STA_MODE still in connection process to network."));
        #endif
        return;
      }

      #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
        DEBUG_WM(F("WiFiManager handle() WIFI_STA_MODE not connected."));
      #endif

      if (!was_disconnected) {
        #if defined(WIFI_MANAGER_DEBUG)
          DEBUG_WM(F("WiFiManager handle() was_disconnected"));
        #endif
        connection_lost_time = get_uptime_ms();
        was_disconnected = true;
        if (first_run) {
          #if defined(WIFI_MANAGER_DEBUG)
            DEBUG_WM(F("WiFiManager handle() first_run"));
          #endif
          switch_station();
          first_run = false;
        }

        disconnected_state = TRY_AP;
      }
      else if (((get_uptime_ms() - connection_lost_time >= WIFIMRG_RECONNECTION_TIMEOUT) &&
            (disconnected_state == TRY_STA)) ||
          (disconnected_state == TRY_STA_IMMEDIATELY))
      {
        switch_station();
        disconnected_state = TRY_AP;
        connection_lost_time = get_uptime_ms();
      }
      else if ((get_uptime_ms() - connection_lost_time >= WIFIMRG_CONNECTION_TIMEOUT) &&
          (disconnected_state == TRY_AP))
      {
        switch_to_AP();
        disconnected_state = TRY_STA;
        connection_lost_time = get_uptime_ms();
      }
      else {
        // try another wifi
        //switch_station();

        //nop
      }
    } else {
      #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
        DEBUG_WM(F("WiFiManager handle() WIFI_STA_MODE connected."));
      #endif
      was_disconnected = false;
      if (connection_process) {
        #if defined(WIFI_MANAGER_DEBUG) && !defined(WIFI_MANAGER_DEBUG_VERBOSE_HANDLE)
          DEBUG_WM(F("WiFiManager handle() WIFI_STA_MODE connected."));
          DEBUG_WM(WiFi.localIP());
        #endif
        connection_changed = true;
      }
      connection_process = false;
      return;
    }
  }
  // опционально:
  // по таймоуту если в AP режиме - отключаться и сканить сети
  // по большему таймоуту в режиме STA - отключаться и сканить сети
  // не делать такого если OTA и ещё что-то критичное
}

void WiFiManager::switch_to_AP() {
  IPAddress  _ap_static_ip;
  IPAddress  _ap_static_sn;

  const char*   _apName = AP_NAME;
  const char*   _apPassword = AP_PASS;

  #if defined(WIFI_MANAGER_DEBUG) && defined(WIFI_MANAGER_DEBUG_VERBOSE_SWITCH_TO_AP)
    DEBUG_WM(F("WiFiManager switch_to_AP()"));
    DEBUG_WM(WiFi.getMode());
    DEBUG_WM(WIFI_AP);
  #endif

  if (WiFi.getMode() != WIFI_AP) {
    #if defined(WIFI_MANAGER_DEBUG) && !defined(WIFI_MANAGER_DEBUG_VERBOSE_SWITCH_TO_AP)
      DEBUG_WM(F("WiFiManager switch_to_AP()"));
    #endif

    _ap_static_ip = IPAddress(AP_STATIC_IP_4,
                  AP_STATIC_IP_3,
                  AP_STATIC_IP_2,
                  AP_STATIC_IP_1
    );

    _ap_static_sn = IPAddress(255, 255, 255, 0);

    disconnect_all();

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(_ap_static_ip, _ap_static_ip, _ap_static_sn);

    if (_apPassword != NULL) {
      WiFi.softAP(_apName, _apPassword);
    } else {
      WiFi.softAP(_apName);
    }
  }
  connection_process = false;
}

void WiFiManager::connect_to_SSID(const char* ssid, const char* password) {
  #if defined(WIFI_MANAGER_DEBUG_WAIT_AFTER_WIFI_BEGIN)
    int counter = 0;
  #endif

  if (WiFi.getMode() == WIFI_AP) {
    WiFi.softAPdisconnect();
  } else {
    WiFi.disconnect();
  }
  WiFi.mode(WIFI_STA);

  #if defined(WIFI_MANAGER_DEBUG)
    DEBUG_WM(F("WiFiManager connect_to_SSID() WiFi.begin()"));
  #endif

  WiFi.begin(ssid, password);
  try_connect_time = get_uptime_ms();
  connection_process = true;

  #if defined(WIFI_MANAGER_DEBUG_WAIT_AFTER_WIFI_BEGIN)
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager connect_to_SSID() WiFi.begin() wait"));
    #endif
    while ((WiFi.status() != WL_CONNECTED) && (counter < 256)) {
      #if defined(WIFI_MANAGER_DEBUG)
        DEBUG_WM(F("."));
      #endif
      delay(500);
    }
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager connect_to_SSID() WiFi.begin() wait end"));
    #endif
  #endif
}

void WiFiManager::switch_station() {
  listWiFiNetworks wifi_list[WIFI_CONFIG_NETWORKS_AMOUNT];
  int counter;
  String foundSSID;
  byte wifi_list_count = 0;
  int8_t wifi_id;
  char buf[WIFI_CONFIG_SSID_MAX_LENGTH];
  int8_t prio_wifi_id;

  #if defined(WIFI_MANAGER_DEBUG)
    DEBUG_WM(F("WiFiManager switch_station()"));
  #endif

  disconnect_all();

  if (wifi_settings->getStoredNetworksCount() == 0) {
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager switch_station() networks list empty"));
    #endif
    return;
  }

  int foundNetworkCount = WiFi.scanNetworks();
  if(foundNetworkCount <= 0) {
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager switch_station() scan no networks"));
    #endif
    return;
  }

  #if defined(WIFI_MANAGER_DEBUG)
    DEBUG_WM("found networks count: " + String(foundNetworkCount));
  #endif
  int bestRSSI = -1000;
  int bestNetworkIndex = -1;

  counter = foundNetworkCount;
  #if defined(WIFI_MANAGER_DEBUG)
  DEBUG_WM(F("WiFiManager switch_station() scan networks sort:"));
  #endif

  prio_wifi_id = wifi_settings->getPriorityNetworkId();

  while (counter-- > 0) {
    foundSSID = WiFi.SSID(counter);
    foundSSID.toCharArray(buf, WIFI_CONFIG_SSID_MAX_LENGTH);
    wifi_id = wifi_settings->getNetworkIdBySsid(buf);
    if (wifi_id >= 0) {
      #if defined(WIFI_MANAGER_DEBUG)
        DEBUG_WM(F("WiFiManager switch_station() network in settings list"));
        DEBUG_WM(buf);
      #endif

      if (wifi_id == prio_wifi_id) {
        #if defined(WIFI_MANAGER_DEBUG)
          DEBUG_WM(F("WiFiManager switch_station() priority network found"));
          DEBUG_WM(buf);
        #endif
        connect_to_SSID(buf,
                wifi_settings->getNetworkPasswordById(wifi_id)
        );
        return;
      }

      strlcpy(wifi_list[wifi_list_count].ssid,
          buf,
          sizeof(wifi_list[wifi_list_count].ssid)
      );

      wifi_list[wifi_list_count].rssi = WiFi.RSSI(counter);
      wifi_list[wifi_list_count].id = wifi_id;
      // тут нужен оптимизейшен!
      // не нужен массив сетей, одной записи bestNetwrok будет достаточно
      

      if(bestRSSI < wifi_list[wifi_list_count].rssi) {
        bestRSSI = wifi_list[wifi_list_count].rssi;
        bestNetworkIndex = wifi_list_count;
      }

      wifi_list_count++;
    } // if (wifi_id >= 0)
  } // while (counter-- > 0)

  if (wifi_list_count>0) {
    #if defined(WIFI_MANAGER_DEBUG)
      DEBUG_WM(F("WiFiManager switch_station() filtered list not empty"));
      DEBUG_WM(wifi_list_count);
    #endif

    if (bestNetworkIndex >= 0) {
      connect_to_SSID(wifi_list[bestNetworkIndex].ssid,
              wifi_settings->getNetworkPasswordById(wifi_list[bestNetworkIndex].id)
      );
    }
  }
}

void WiFiManager::disconnect_all() {
  #warning Let make it inline/static? 
  WiFi.softAPdisconnect();
  WiFi.disconnect();
}

bool WiFiManager::try_add(const char* ssid, const char* password, const bool hidden) {

  if ((wifi_settings->getNetworkIdBySsid(try_add_ssid) < 0) &&
    (wifi_settings->getStoredNetworksCount() >= WIFI_CONFIG_NETWORKS_AMOUNT))
  {
    return false;
  }

  strlcpy(try_add_ssid,
    ssid,
    sizeof(try_add_ssid)
  );
  strlcpy(try_add_password,
    password,
    sizeof(try_add_password)
  );
  try_add_hidden = hidden;
  try_add_start = true;
  try_add_run = false;
  try_add_tries = 4;
  try_add_status = try_add_idle;
  if (get_uptime_ms() - WIFIMRG_TRY_CINNECT_TIMEOUT > 0) {
    try_add_time = get_uptime_ms() - WIFIMRG_TRY_CINNECT_TIMEOUT;
  }

  return true;
}

int8_t WiFiManager::get_try_add_status() {
  int8_t tmp = try_add_status;
  try_add_status = try_add_idle;
  return tmp;
}

bool WiFiManager::get_connection_changed() {
  bool tmp = connection_changed;
  connection_changed = false;
  return tmp;
}

#if defined(WIFI_MANAGER_DEBUG)
template <typename Generic>
void WiFiManager::DEBUG_WM(Generic text) {
  //if (_debug) {
    Serial.print(F("*WiFiManager: "));
    Serial.println(text);
  //}
}
#endif
