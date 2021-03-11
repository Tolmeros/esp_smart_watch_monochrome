#include "json_settings.h"
#include "factory.h"

#if defined(ESP8266)
	#include "FS.h"
#elif defined(ESP32)
	#include "SPIFFS.h"
#endif

bool loadJson(const char *filename, JsonDocument& json) {
	File configFile = SPIFFS.open(filename, "r");
	if (!configFile) {
		return false;
	}

	DeserializationError error = deserializeJson(json, configFile);
	if (error) {
		configFile.close();
		return false;
	}

	configFile.close();
	return true;
}

bool saveJson(const char *filename, JsonDocument& json) {
	File configFile = SPIFFS.open(filename, "w");
	if (!configFile) {
		return false;
	}

	if (serializeJson(json, configFile) == 0) {
		configFile.close();
		return false;
	}

	configFile.close();
	return true;
}

JsonSettingsWiFi::JsonSettingsWiFi() {
	strcpy_P(this->filename, PSTR(WIFI_CONFIG_FILE_NAME));
}

JsonSettingsWiFi::JsonSettingsWiFi(const char* filename){
	strlcpy(this->filename, filename, sizeof(this->filename));
}

bool JsonSettingsWiFi::load() {
	StaticJsonDocument<1024> wifi_config;
	bool load_status = loadJson(filename, wifi_config);
	uint8_t t_mode;
	bool status = false;

	if (!load_status) {
		return false;
	}

	if (wifi_config.containsKey(F(WIFI_CONFIG_ESP_WIFI_MODE_KEY)))
	{
		//t_mode = wifi_config[F(WIFI_CONFIG_ESP_WIFI_MODE_KEY)].as<uint8_t>();
		esp_wifi_mode = wifi_config[F(WIFI_CONFIG_ESP_WIFI_MODE_KEY)].as<uint8_t>();

		changed = false;
		status = true;
	}

	if (wifi_config.containsKey(F(WIFI_CONFIG_NETWORKS_KEY))) {
		size_t length = wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)].size();
		networks_count = 0;

		for (byte i = 0; i < length; i++) {
			if ( wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i].containsKey(F(WIFI_CONFIG_NETWORKS_SSID_KEY)) &&
				 wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i].containsKey(F(WIFI_CONFIG_NETWORKS_PASSWORD_KEY)) &&
				 wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i].containsKey(F(WIFI_CONFIG_NETWORKS_HIDDEN_KEY)))
			{
				this->addNetwork(wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i][F(WIFI_CONFIG_NETWORKS_SSID_KEY)].as<String>().c_str(),
								wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i][F(WIFI_CONFIG_NETWORKS_PASSWORD_KEY)].as<String>().c_str(),
								wifi_config[F(WIFI_CONFIG_NETWORKS_KEY)][i][F(WIFI_CONFIG_NETWORKS_HIDDEN_KEY)].as<bool>()
				);
			}
		}

		changed = false;
		status = true;
	}

	if (wifi_config.containsKey(F(WIFI_CONFIG_PRIORITY_NETWORK_KEY)))
	{
		if (this->setPriorityNetwork(wifi_config[F(WIFI_CONFIG_PRIORITY_NETWORK_KEY)].as<String>().c_str())) {
			changed = false;
			status = true;
		}
	}

	return status;
}

bool JsonSettingsWiFi::save() {
	StaticJsonDocument<1024> wifi_config;
	JsonObject nested;

	wifi_config[F(WIFI_CONFIG_ESP_WIFI_MODE_KEY)] = esp_wifi_mode;

	wifi_config[F(WIFI_CONFIG_PRIORITY_NETWORK_KEY)] = priority_ssid;

	JsonArray wifi_config_networks = wifi_config.createNestedArray(F(WIFI_CONFIG_NETWORKS_KEY));

	for (uint8_t i=0; i < networks_count; i++) {
		nested = wifi_config_networks.createNestedObject();
		nested[F(WIFI_CONFIG_NETWORKS_SSID_KEY)] = networks[i].ssid;
        nested[F(WIFI_CONFIG_NETWORKS_PASSWORD_KEY)] = networks[i].password;
        nested[F(WIFI_CONFIG_NETWORKS_HIDDEN_KEY)] = networks[i].hidden;
	}

	bool save_status = saveJson(filename, wifi_config);
	if (save_status) {
		changed = false;
		return true;
	}
	return false;
}

bool JsonSettingsWiFi::begin() {
	save_time = millis();
	bool load_status = load();

	if (!load_status) {
		// Make defaults if cannot load settings.
		esp_wifi_mode = ESP_WIFI_MODE;
	}

	return load_status;
}

bool JsonSettingsWiFi::begin(uint8_t defalut) {
	save_time = millis();
	bool load_status = load();

	if (!load_status) {
		// Make defaults if cannot load settings.
		esp_wifi_mode = defalut;
	}

	return load_status;
}

bool JsonSettingsWiFi::handle() {
	if (changed && (millis() - save_time > SAVE_INTERVAL)) {
		save_time = millis();
		return save();
	}
	return true;
}

void JsonSettingsWiFi::save_now() {
	save();
	save_time = millis();
}

bool JsonSettingsWiFi::setEspWiFiMode(uint8_t mode) {
	if (mode < 2) {
		esp_wifi_mode = mode;
		changed = true;
		return true;
	} else {
		return false;
	}
}

uint8_t JsonSettingsWiFi::getEspWiFiMode() {
	return esp_wifi_mode;
}

uint8_t JsonSettingsWiFi::getStoredNetworksCount() {
	return networks_count;
}

bool JsonSettingsWiFi::addNetwork(const char* ssid, const char* password){
	return addNetwork(ssid, password, false);
}

bool JsonSettingsWiFi::addNetwork(const char* ssid, const char* password, const bool hidden) {
	if (networks_count < WIFI_CONFIG_NETWORKS_AMOUNT) {
		if (this->networkExsists(ssid)) {
			return false;
		} else {
			strlcpy(networks[networks_count].ssid, ssid, sizeof(networks[networks_count].ssid));
			strlcpy(networks[networks_count].password, password, sizeof(networks[networks_count].password));
			networks[networks_count].hidden = hidden;

			networks_count++;
			changed = true;
			return true;
		}
	}

	return false;
}

bool JsonSettingsWiFi::delNetwork(const char* ssid){
	bool status = false;

	int8_t id = getNetworkIdBySsid(ssid);
	if (id >= 0){
		networks_count--;
		if ((networks_count > 0) && (id != networks_count)) {
			strlcpy(networks[id].ssid,
					networks[networks_count].ssid,
					sizeof(networks[id].ssid)
			);
			strlcpy(networks[id].password,
					networks[networks_count].password,
					sizeof(networks[id].password)
			);
			networks[id].hidden = networks[networks_count].hidden;
		}
		status = true;
		changed = true;
	}
	return status;
}

bool JsonSettingsWiFi::delNetwork(const uint8_t id) {
	bool status = false;
	if ( (networks_count>0) && (id<networks_count) ){
		networks_count--;
		if ((networks_count > 0) && (id != networks_count)) {
			strlcpy(networks[id].ssid,
					networks[networks_count].ssid,
					sizeof(networks[id].ssid)
			);
			strlcpy(networks[id].password,
					networks[networks_count].password,
					sizeof(networks[id].password)
			);
			networks[id].hidden = networks[networks_count].hidden;
		}
		status = true;
		changed = true;
	}

	return status;
}

bool JsonSettingsWiFi::changeNetworkSsid(const uint8_t id, const char* ssid) {
	if ((this->isIdValid(id))&&(!networkExsists(ssid))) {
		strlcpy(networks[id].ssid,
				ssid,
				sizeof(networks[id].ssid)
		);
		changed = true;
		return true;
	} else {
		return false;
	}
}

bool JsonSettingsWiFi::changeNetworkPassword(const uint8_t id, const char* password){
	if (this->isIdValid(id)) {
		strlcpy(networks[id].password,
				password,
				sizeof(networks[id].ssid)
		);
		changed = true;
		return true;
	} else {
		return false;
	}
}

bool JsonSettingsWiFi::changeNetworkHidden(const uint8_t id, const bool hidden){
	if (this->isIdValid(id)) {
		networks[id].hidden = hidden;
		changed = true;
		return true;
	} else {
		return false;
	}
}

bool JsonSettingsWiFi::networkExsists(const char* ssid) {
    bool found = false;

    for (byte i=0; i<networks_count; i++) {
        if (strcmp(networks[i].ssid, ssid) == 0) {
            found = true;
            break;
        }
    }

    return found;
}

int8_t JsonSettingsWiFi::getNetworkIdBySsid(const char* ssid) {
	int8_t id = -1;

	for (byte i = 0; i < networks_count; i++) {
		if (strcmp(networks[i].ssid, ssid) == 0) {
			id = i;
			break;
		}
	}

	return id;
}

const char* JsonSettingsWiFi::getNetworkSsidById(int8_t id) {
	if ((id >= 0) && (id < networks_count)) {
		return networks[id].ssid;
	} else {
		return NULL;
	}
}

const char* JsonSettingsWiFi::getNetworkPasswordById(int8_t id) {
	if ((id >= 0) && (id < networks_count)) {
		return networks[id].password;
	} else {
		return NULL;
	}
}

bool JsonSettingsWiFi::getNetworkHiddenById(int8_t id) {
	if ((id >= 0) && (id < networks_count)) {
		return networks[id].hidden;
	} else {
		return false;
	}
}

bool JsonSettingsWiFi::isIdValid(uint8_t id) {
	if ((id >= 0) && (id < networks_count)) {
		return true;
	} else {
		return false;
	}
}

bool JsonSettingsWiFi::setPriorityNetwork(const char* ssid) {
	int8_t id = getNetworkIdBySsid(ssid);
	//if ((ssid == NULL) || (strcmp(ssid, "") == 0)) {
	if ((ssid == NULL) || (ssid[0] == '\0')) {
		priority_ssid[0] = '\0';
    changed = true;
		return true;
	}

	if (id < 0 ) {
		return false;
	} else {
		strlcpy(priority_ssid,
				ssid,
				sizeof(priority_ssid)
		);
    changed = true;
		return true;
	}

}

const char* JsonSettingsWiFi::getPriorityNetworkSsid() {
	return priority_ssid;
}

int8_t JsonSettingsWiFi::getPriorityNetworkId() {
	return getNetworkIdBySsid(priority_ssid);
}
