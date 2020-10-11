// code from github.com/ESP-EOS/ESP32-WiFi-Sniffer/blob/master/WIFI_SNIFFER_ESP32.ino#L77

#include "esp_sleep.h"
#include "common.h"
#include <driver/adc.h>

void wifi_init_sta(void);
void wifi_sniffer_run(void);
void setupEvents(void);
void initCLI(void*);
void checkClients(void*);
void httpServer(void*);
void ddnsClient(void*);
void oled_init(void);
void wifi_start_station(void);
void wifi_eraseconfig(void);
void camera_app_main(void);
void wifi_spiffs_init(void);
void task_test_SSD1306i2c(void *ignore);
void wifi_mdns_init(void);
void wifi_mqtt_start(void*);
void wifi_send_mqtt(char* msg);
void wifi_spiffs_config_read(void);

static void event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data);

EventGroupHandle_t s_wifi_event_group;
bool nimbleRunning = true;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;
uint8_t mac[6] ={0};  // WiFi MAC address;
uint8_t s_retries_count=0;
static const char *TAG = "SecDev  ";
static TaskHandle_t xhandleSniffer = NULL;
static TaskHandle_t xhandleMqtt = NULL;
static TaskHandle_t xhandleHttp = NULL;


#define EXAMPLE_ESP_WIFI_SSID      "JioFi2_D0B75C"  // not used with SmarConfig
#define EXAMPLE_ESP_WIFI_PASS      "xyz"            // not used
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
wifi_config_t wifi_config = {
    .sta = {
        //.ssid = EXAMPLE_ESP_WIFI_SSID,
        //.password = EXAMPLE_ESP_WIFI_PASS,
        /* Setting a password implies station will connect to all security modes including WEP/WPA.
         * However these modes are deprecated and not advisable to be used. Incase your Access point
         * doesn't support WPA2, these mode can be enabled by commenting below line */
     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};
// Forces data into RTC slow memory. See "docs/deep-sleep-stub.rst"
// Any variable marked with this attribute will keep its value
// during a deep sleep / wake cycle.

// Forces data into RTC slow memory of .noinit section.
// Any variable marked with this attribute will keep its value
// after restart or during a deep sleep / wake cycle.
TaskHandle_t snifferHandle = NULL;

void app_main(void)
{
    esp_log_level_set("httpd", ESP_LOG_INFO); 
    esp_log_level_set("wifi", ESP_LOG_INFO); 
    esp_log_level_set("ble", ESP_LOG_INFO); 
    esp_log_level_set("ssd1306", ESP_LOG_INFO); 
    esp_log_level_set("u8g2_hal", ESP_LOG_INFO);
    esp_log_level_set("efuse", ESP_LOG_INFO);
    esp_log_level_set("wpa", ESP_LOG_INFO);


    printf("Hello world!\n");
        esp_efuse_mac_get_default(mac); // reads existing default mac
    esp_wifi_set_mac(WIFI_IF_STA, mac);
    char str[19];
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",mac[0], 
        mac[1], mac[2], mac[3], mac[4],mac[5]);
    printf("\n MAC address of ESP32: %s", str);
    

    // This is the new U8g2 driver; works for inbuilt OLED on WEMOS TTGO
    task_test_SSD1306i2c(NULL);
    oledClear();
    oledDisplay(10, 5, "Init..");

   //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /* Block for 500ms. */
    TickType_t xDelay = 500 / portTICK_PERIOD_MS;
    vTaskDelay(xDelay);

    // file to setup an INTR to clear WiFi NVS storage
    // On standard ESP32:
    //      Boot button is connected to GPIO0 and pressing that, erases the WiFi storage
    // data that contains username/password
    // Learnings from https://github.com/lucadentella/esp32-tutorial
    wifi_eraseconfig(); 

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    int lvl=0;
    gpio_pad_select_gpio(22); // Boot button
    gpio_set_direction(22, GPIO_MODE_INPUT);
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1<<22);
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "[!] Starting sniffing task...");
    xTaskCreate(&snifferTask, "sniffig_task", 10000, NULL, 1, &xhandleSniffer);
    if(xhandleSniffer == NULL)
        ESP_LOGI(TAG, "Cannot to create sniffer task");
    
    xTaskCreate(&mqttTask, "mqttTask", 10000, NULL, 0, &xhandleMqtt);
    if(xhandleMqtt == NULL)
        ESP_LOGI(TAG, "Cannot to create MQTT task");

    xTaskCreate(&httpServer, "httpServer", 10000, NULL, 0, &xhandleHttp);
    if(xhandleHttp == NULL)
        ESP_LOGI(TAG, "Cannot to create HTTP Server task");

    //adc1_config_width(ADC_WIDTH_BIT_12);
    while(1) {
        //int bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, /* clear bit */
        //            1, 3000/portTICK_RATE_MS);
        TickType_t xDelay = 500 / portTICK_PERIOD_MS;
        vTaskDelay(xDelay);
        
        lvl=gpio_get_level(22);
        if (lvl == 1) {
            printf("\n Boot button pressed - erase flash !!!");
            nvs_flash_erase();
        }
    }
    printf("SecDev - exited !");
    vTaskDelete(xhandleSniffer);

}

void printDiags() {
    tcpip_adapter_ip_info_t ip_info;
    uint8_t l_Mac[6];

    esp_wifi_get_mac(ESP_IF_WIFI_STA, l_Mac);
    ESP_LOGI(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x", l_Mac[0], l_Mac[1], l_Mac[2], l_Mac[3], l_Mac[4], l_Mac[5]);

    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    ESP_LOGI(TAG, "IP Address:  %s", ip4addr_ntoa(&ip_info.ip));
    ESP_LOGI(TAG, "Subnet mask: %s", ip4addr_ntoa(&ip_info.netmask));
    ESP_LOGI(TAG, "Gateway:     %s", ip4addr_ntoa(&ip_info.gw));

    ESP_LOGI(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                ESP_EVENT_ANY_ID, &event_handler,
                                NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                IP_EVENT_STA_GOT_IP, &event_handler,
                                NULL, &instance_got_ip));
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, 
                                ESP_EVENT_ANY_ID, &smartconfig_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to AP");
        printDiags();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to AP");
    } else {
        ESP_LOGE(TAG, "wifi init sta - UNEXPECTED EVENT");
    }
    ESP_LOGI(TAG, "=========================================================================");
    /* The event will not be processed after unregister */
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    //vEventGroupDelete(s_wifi_event_group);
}

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    tcpip_adapter_ip_info_t ip_info;
    ESP_LOGI(TAG, "event_handler eventid: %d", event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG,"WIFI_EVENT_STA_START recvd");
        //ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));  
        esp_err_t ret = esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_config);
        if (ret == ESP_OK) {
            printf("\n !! Config found in Flash");
            printf("\nSize of esp_wifi_config: %i", sizeof(wifi_config));
            if (strlen((char*)wifi_config.sta.ssid) == 0) {
                printf("\n But, SSID is NULL...start Smart Config");
                oledDisplay(40, 30, "- Smart Cfg");
                xTaskCreate(smartconfig_run_task, "Smart Config Task", 3072, NULL, 1, NULL);
                return;
            }
            printf("\nStored SSID: %s", (char *)wifi_config.sta.ssid);
            //printf("\nStored PASSWORD: %s", (char*)wifi_config.sta.password); 
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
            esp_wifi_connect();
        } else {
            printf("\n !!!! No config found in Flash");
            ESP_LOGI(TAG, "!!!!!!!!! Station Start Handler - start smart config"); 
            xTaskCreate(smartconfig_run_task, "Smart Config Task", 3072, NULL, 1, NULL);
        }
        //esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG,"WIFI_EVENT_STA_DISCONNECTED recvd");
        oledClear();  oledDisplay(7, 10, "DisConnected:"); 
        //if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
        //    s_retry_num++;
        //    ESP_LOGI(TAG, "retry connect to AP");
        //} else {
        //    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        //}
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG,"WIFI_EVENT_STA_CONNECTED recvd");
        oledClear(); 
        oledDisplay(7, 10, "Connected:"); oledDisplay(7, 25, (char *)wifi_config.sta.ssid);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
        oledClear(); 
        oledDisplay(7, 10, "Connected:"); oledDisplay(7, 25, (char *)(ip4addr_ntoa(&ip_info.ip)));
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

   
 
  