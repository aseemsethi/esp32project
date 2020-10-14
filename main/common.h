#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"  // FOR EventGroupHandle_t
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
//#include "esp_event_loop.h"

#include "esp_event.h"
#include <nvs_flash.h>
#include "driver/gpio.h"
#include "esp_log.h"  // for ESP_LOGE
#include "esp_event.h"
#include "string.h"
#include "sdkconfig.h"
#include <errno.h>
#include <esp_http_server.h>
#include "esp_smartconfig.h"
#include "md5.h"
//void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest);

#define WIFI_CHANNEL_SWITCH_INTERVAL    (700)
#define WIFI_CHANNEL_MAX                (13)
#define MAX_TAGS 10
#define CONFIG_VERBOSE 1
#define CONFIG_CHANNEL 11
#define SSID_MAX_LEN (32+1) //max length of a SSID
#define MD5_LEN (32+1) //length of md5 hash

typedef struct {
  unsigned frame_ctrl:16;
  unsigned duration_id:16;
  uint8_t addr1[6]; // receiver address
  uint8_t addr2[6]; // sender address
  uint8_t addr3[6]; // filtering address
  unsigned sequence_ctrl:16;
  uint8_t addr4[6];   // optional
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; // network data + checksum
} wifi_ieee80211_packet_t;

typedef struct {
    char bleName[20];
    char bleTag[20];
    char notifyOn[10];
    char startTime[10];
    char endTime[10];
    int bleID;
} ble_t;

typedef struct {
  //uint8_t cl[6];
  char wl[18];
  char name[10];
  bool sentNote;
} cListStruct;

void oledDisplay(int x, int y, char* str);
void oledClear(void);
void wifi_config_init(bool runtime);
void wifi_config_read(void);
void wifi_config_read_into_params(void);
void wifi_spiffs_write(char*);
void wifi_config_write_string(char* type, char* val);
void wifi_config_write_int(char* type, int val);
int wifi_config_write_beacons(char* name, char* tag, char* notifyOn, 
                              char* startTime, char* endTime, int id);
int getCurrentTime();
void stop_webserver(httpd_handle_t server);
void ble_main(void);
void smartconfig_run_task(void*);
void smartconfig_event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data);
void snifferTask(void *pvParameter);
void wifi_sniffer_init(void);
void wifi_sniffer_packet_handler(void *buff, wifi_promiscuous_pkt_type_t type);
void get_hash(unsigned char *data, int len_res, char hash[MD5_LEN]);
void get_ssid(unsigned char *data, char ssid[SSID_MAX_LEN], uint8_t ssid_len);
int get_sn(unsigned char *data);
void get_ht_capabilites_info(unsigned char *data, char htci[5], int pkt_len, int ssid_len);
void dumb(unsigned char *data, int len);
void save_pkt_info(uint8_t address[6], char *ssid, time_t timestamp, char *hash, int8_t rssi, int sn, char htci[5]);
int get_start_timestamp(void);
void mqttTask(void*);
void wifi_send_mqtt(char* msg);
void httpServer(void* param);
int wifi_config_write_macs(char* macaddress, char* name);
