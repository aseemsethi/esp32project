// Example taken from
// https://github.com/espressif/esp-idf/blob/master/
// examples/protocols/http_server/simple/main/main.c
#include "common.h"
#include <esp_http_server.h>
#include <sys/param.h>  // for #define MIN
#include "esp_spiffs.h"

extern EventGroupHandle_t s_wifi_event_group;
extern const int WIFI_CONNECTED_BIT;

static const char *TAG = "SecDev HTTP ";
void clearConfigs();

httpd_handle_t server = NULL;
char cfgMqttTopic[100];
extern cListStruct cList[];
extern int cListIndex;

//char ddns_uri[100];
void wifi_send_mqtt(char* msg);
//char version[6] = "1.0";
//extern char rendered[];

//int ble_index = 0;
//ble_t ble[MAX_TAGS]; 

static esp_err_t clear_handler(httpd_req_t *req)
{
    const char* resp = "Config Cleared";
    ESP_LOGI(TAG, "\nHTTP Clear Config");
    //clearConfigs();
    httpd_resp_send(req, resp, strlen(resp));
    wifi_send_mqtt("MQTT: Config Cleared");
    return ESP_OK;
}

static const httpd_uri_t clear = {
    .uri       = "/clear",
    .method    = HTTP_GET,
    .handler   = clear_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Clear Config!"
};

/**********************************************************************************/

/* An HTTP GET handler 
 Example - 
     http://192.168.68.111:8080/putval?mqtt_topic=/aseem/secdev
     http://192.168.68.111:8080/putval?mac=18:19:d6:85:95:b2&name=kavita
     http://192.168.68.111:8080/putval?mac=80:ad:16:99:8b:54&name=dhruv
*/
static esp_err_t params_set_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    ESP_LOGI(TAG, "\nHTTP Set Values");
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, " => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "mqtt_topic", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => mqtt topic=%s", param);
                strcpy(cfgMqttTopic, param);
                char msg[100]; strcpy(msg, "MQTT: Set Topic "); strcat(msg, cfgMqttTopic);
                wifi_send_mqtt(msg);
                //wifi_config_write_string("MQTT_TOKEN", mqtt_topic);
            }
            if (httpd_query_key_value(buf, "mac", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => mac=%s", param);
                strcpy(cList[cListIndex].wl, param); 
                //wifi_config_write_string("MQTT_TOKEN", mqtt_topic);
                if (httpd_query_key_value(buf, "name", param, sizeof(param)) == ESP_OK) {
                    ESP_LOGI(TAG, "Found URL query parameter => name=%s", param);
                    strcpy(cList[cListIndex].name, param); 
                    //wifi_config_write_string("MQTT_TOKEN", mqtt_topic);
                }
                cListIndex++;
            }

            /*
            if (httpd_query_key_value(buf, "ddns", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => DDNS uri =%s", param);
                strcpy(ddns_uri, param);
                wifi_config_write_string("DDNS", ddns_uri);
            }
            if (httpd_query_key_value(buf, "ble", param, sizeof(param)) == ESP_OK) {
                ESP_LOGI(TAG, "Found URL query parameter => BLE =%s", param);
                char* pch;
                pch = strtok (param,":");
                ESP_LOGI(TAG, "BLE Name:Tag recvd: %s:",pch);
                strcpy(ble[ble_index].bleName, pch);
                pch = strtok (NULL, ":");
                ESP_LOGI(TAG, "%s", pch);
                strcpy(ble[ble_index].bleTag, pch);
                pch = strtok (NULL, ":");
                ESP_LOGI(TAG, "%s", pch);
                int id = atoi(pch);
                ble[ble_index].bleID = id;

                int ret = wifi_config_write_beacons(ble[ble_index].bleName, 
                    ble[ble_index].bleTag, id);
                if (ret == 1) {
                    ble_index++; if (ble_index==9) ble_index=0;
                }
            } */
        }
        free(buf);
    }

    // End response
    // httpd_resp_send_chunk(req, NULL, 0);
    const char* resp = "HTTP Params applied";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static const httpd_uri_t putVal = {
    .uri       = "/putval",
    .method    = HTTP_GET,
    .handler   = params_set_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Params Put!"
};

/**********************************************************************************/
static esp_err_t readval_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;
    char resp[600];

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    ESP_LOGI(TAG, "\nHTTP Read Values");
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            
            char param[32];
            /*
            if (httpd_query_key_value(buf, "config", param, sizeof(param)) == ESP_OK) {
                wifi_config_init(false);
                resp[0] = '\0';
                char temp[600];
                sprintf(temp, "Version: %s, Config: \n %s", version, rendered);
                strcat(resp, temp);
            } else*/
            if (httpd_query_key_value(buf, "mem", param, sizeof(param)) == ESP_OK) {
                resp[0] = '\0';
                char temp[50];
                int avail = esp_get_minimum_free_heap_size();
                sprintf(temp, "Free Mem: %d", avail);
                strcat(resp, temp);
            }
        }
        free(buf);
    }
    // End response
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static const httpd_uri_t readVal = {
    .uri       = "/readval",
    .method    = HTTP_GET,
    .handler   = readval_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Read Values!"
};

/**********************************************************************************/

// Need to call these 2 functions from WIFI connect/disconnect handlers
void stop_webserver(httpd_handle_t server) {
    // Stop the httpd server
    httpd_stop(server);
}

int start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.server_port = 8080;
        // Start the httpd server
    ESP_LOGI(TAG, "Starting HTTP server on port: '%d'", 8080);
    if (httpd_start(&server, &config) == ESP_OK) {
    	printf("...http success !");
        // Set URI handlers
        httpd_register_uri_handler(server, &putVal);
        httpd_register_uri_handler(server, &readVal);
        httpd_register_uri_handler(server, &clear);
        return 1;
	} else {
        printf("\nHTTP Server failed to start !!!");
        return 0;
    }
}

void httpServer(void* param) {
    int ret = 0;
    ESP_LOGI(TAG, "Starting HTTP task");
    int bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, 
            pdFALSE, pdFALSE, portMAX_DELAY);  // Dont clear the BITs
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wifi connected..starting HTTP Server");
		ret = start_webserver();
        if (ret == 0) {
            ESP_LOGI(TAG, "Failed to start HTTP Server");
            const TickType_t xDelay = 2000 / portTICK_PERIOD_MS;
            vTaskDelay(xDelay);
            ret = start_webserver();
            if (ret == 0) {
                oledDisplay(50, 20, "..HTTP Fail");
                return;
            }
        } else {
            // HTTP Server started...we don't return from here
            while (1) {
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                // TBD - if Wifi Disconnects, we need to restart this process
            }
        }
	}
}