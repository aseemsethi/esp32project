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
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"  // for ESP_LOGE
#include "esp_event.h"
#include "string.h"
#include "sdkconfig.h"
#include <errno.h>
#include <esp_http_server.h>


#define WIFI_CHANNEL_SWITCH_INTERVAL    (700)
#define WIFI_CHANNEL_MAX                (13)
#define MAX_TAGS 10

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
static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data);
int getCurrentTime();
void stop_webserver(httpd_handle_t server);
void ble_main(void);
