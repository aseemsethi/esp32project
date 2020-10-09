#include "common.h"
#include "mqtt_client.h"
#include "esp_sntp.h"
/* True if ESP is connected to the MQTT broker, false otherwise */
static bool MQTT_CONNECTED = false;
extern const int WIFI_CONNECTED_BIT;
/* Client variable for MQTT connection */
static esp_mqtt_client_handle_t client;
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
void time_sync_notification_cb(struct timeval *tv);
char mqtt_topic[100];
static const char *TAG = "MQTT ";
extern EventGroupHandle_t s_wifi_event_group;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    client = event->client;

    //your_context_t *context = event->context;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "[MQTT] Connected");
            MQTT_CONNECTED = true;
            wifi_send_mqtt("aseem mqtt connected");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "[MQTT] Disconnected");
            MQTT_CONNECTED = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "[MQTT] EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "[MQTT] EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "[MQTT] EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "[MQTT] EVENT_DATA");
            ESP_LOGI(TAG, "[MQTT] TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "[MQTT] DATA=%.*s\r\n", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "[MQTT] MQTT_EVENT_ERROR");
            break;
        case MQTT_EVENT_ANY:
            ESP_LOGI(TAG, "[MQTT] MQTT_EVENT_ANY");
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "[MQTT] MQTT_EVENT_BEFORE_CONNECT");
            break;
    }

    return ESP_OK;
}

void mqttTask(void* param) {
    bool timeSet = pdFALSE;
    char strftime_buf[64];
    int started = 0;

    strcpy(mqtt_topic, "/aseem/secdev");
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtt://mqtt.eclipse.org",
        .port = 1883,
        .event_handle = mqtt_event_handler,
        // .user_context = (void *)your_context
    };
    client = esp_mqtt_client_init(&mqtt_cfg);

    while(1) {
    	int bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, /* clear bit */
                    1, 3000/portTICK_RATE_MS);
	    if (bits & WIFI_CONNECTED_BIT) {
                if (started == 0) {
                    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
                    ESP_LOGI(TAG,"wifi_mqtt_start: Wifi connected..starting MQTT");
                    started = 1;
                } else {
                    ESP_LOGI(TAG,"wifi_mqtt_start: not restarting MQTT");
                }
	    		wifi_send_mqtt("WiFi is up...");
            if (timeSet == pdFALSE) {
                ESP_LOGI(TAG, "\n Trying to get time from NTP");
                timeSet = pdTRUE;

                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                sntp_setservername(0, "pool.ntp.org");
                sntp_set_time_sync_notification_cb(time_sync_notification_cb);
                sntp_init();

                time_t now = 0;
                struct tm timeinfo = { 0 };
                int retry = 0;
                const int retry_count = 10;
                while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
                    ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                }
                time(&now);
                //setenv("TZ", "CST+18:30", 1);
                setenv("TZ", "UTC-5:30", 1);
                tzset();
                localtime_r(&now, &timeinfo);
                strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
                ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
            }
   	    }
	}
}

void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void wifi_send_mqtt(char* msg) {
	if (MQTT_CONNECTED == true) {
        time_t now;
        struct tm timeinfo;
        char strftime_buf[64];
        char temp[200];

        strcpy(temp, msg);
        strcat(temp, " : ");

        time(&now);
        //setenv("TZ", "CST+18:30", 1);
        //tzset();
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time : %s", strftime_buf);
        strcat(temp, strftime_buf);

		int msg_id = esp_mqtt_client_publish(client, mqtt_topic, temp, 0, 0, 0);
		ESP_LOGI(TAG, "\nwifi_send_mqtt: MQTT Msg: %s : to %s , with id: %d !!!", 
							temp, mqtt_topic, msg_id);
	}
}