#pragma once
//#ifndef JSON_SETTINGS_H
//#define JSON_SETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>

// replace with SPIFFS_OBJ_NAME_LEN from core `spiffs_config.h`
#define SPIFFS_FILENAME_LENGTH_LIMIT	32
#define SAVE_INTERVAL          30*1000

#define WIFI_CONFIG_FILE_NAME				"/wifi.json"
#define WIFI_CONFIG_ESP_WIFI_MODE_KEY		"mode"
#define WIFI_CONFIG_PRIORITY_NETWORK_KEY 	"priority_ssid"
#define WIFI_CONFIG_NETWORKS_KEY			"networks"
#define WIFI_CONFIG_NETWORKS_SSID_KEY		"ssid"
#define WIFI_CONFIG_NETWORKS_PASSWORD_KEY	"password"
#define WIFI_CONFIG_NETWORKS_HIDDEN_KEY		"hidden"

#define WIFI_CONFIG_NETWORKS_AMOUNT	10
#define WIFI_CONFIG_SSID_MAX_LENGTH	32
#define WIFI_CONFIG_PASS_MAX_LENGTH	64

struct settingsWiFiNetworks {
    char ssid[WIFI_CONFIG_SSID_MAX_LENGTH];
    char password[WIFI_CONFIG_PASS_MAX_LENGTH];
    bool hidden;
};

bool loadJson(const char *filename, JsonDocument& json);
bool saveJson(const char *filename, JsonDocument& json);

class JsonSettingsWiFi{
private:
	uint8_t esp_wifi_mode;

	char filename[SPIFFS_FILENAME_LENGTH_LIMIT];
	bool changed;
	unsigned long save_time;

	settingsWiFiNetworks networks[WIFI_CONFIG_NETWORKS_AMOUNT];
	uint8_t networks_count;
	char priority_ssid[WIFI_CONFIG_SSID_MAX_LENGTH];

	bool load();
	bool save();

public:
	JsonSettingsWiFi();
	JsonSettingsWiFi(const char* filename);

	bool begin();
	bool begin(uint8_t defalut);

	bool handle();
	void save_now();

	bool setEspWiFiMode(uint8_t mode);
	uint8_t getEspWiFiMode();

	uint8_t getStoredNetworksCount();

	bool addNetwork(const char* ssid, const char* password);
	bool addNetwork(const char* ssid, const char* password, const bool hidden);

	bool delNetwork(const char* ssid);
	bool delNetwork(const uint8_t id);

	bool changeNetworkSsid(const uint8_t id, const char* ssid);
	bool changeNetworkPassword(const uint8_t id, const char* password);
	bool changeNetworkHidden(const uint8_t id, const bool hidden);

	bool networkExsists(const char* ssid);

	int8_t getNetworkIdBySsid(const char* ssid);
	const char* getNetworkSsidById(int8_t id);
	const char* getNetworkPasswordById(int8_t id);

	bool getNetworkHiddenById(int8_t id);

	bool isIdValid(uint8_t id);

	bool setPriorityNetwork(const char* ssid);
	const char* getPriorityNetworkSsid();
	int8_t getPriorityNetworkId();

};
//#endif //JSON_SETTINGS_H
