#include "common.h"
#include "esp_spiffs.h"
#include "cJSON.h"

static const char *TAG = "WiFi Config";
extern char mqtt_topic[100];
extern int mqtt;
extern char ddns_uri[];
extern int ble_index;
extern ble_t ble[]; 

esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true
  };

cJSON *root;
#define MAXBUFLEN 1000
char rendered[MAXBUFLEN + 1];  // assuming a max config file size of 1000 characters

void wifi_config_init(bool boottime) {
    ESP_LOGI(TAG, "Reading SPIFFS....config init");
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
      if (ret == ESP_FAIL)
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
      else if (ret == ESP_ERR_NOT_FOUND)
        ESP_LOGE(TAG, "Failed to find spiffs partition");
      else 
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      return ;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
      ESP_LOGE(TAG, "Failed to get SPIFF partition info (%s)", esp_err_to_name(ret));
    else 
      ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    wifi_config_read();
    if (boottime == true) {
      printf("\n Reading in config into variables");
      wifi_config_read_into_params();
    }

    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "SPIFFS unmounted");
}

void wifi_config_read() {
	int pos=0;
	
    FILE* fp = fopen("/spiffs/hello","r");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    } 
    fseek(fp, 0, SEEK_SET);
    ESP_LOGI(TAG, "***** Initial read file*******\n");
    while(1) {
    	char c = fgetc(fp);
      	if( feof(fp) ) break ;
      	//printf("%c", c);
      	rendered[pos++] = c;
      }     
    rendered[pos++] = '\0';
    fclose(fp);
}

void clearConfigs() {
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
      if (ret == ESP_FAIL)
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
      else if (ret == ESP_ERR_NOT_FOUND)
        ESP_LOGE(TAG, "Failed to find spiffs partition");
      else 
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      return ESP_FAIL;
    }
    FILE* f = fopen("/spiffs/hello", "w"); // opening with w erases the file
    fclose(f);
    esp_vfs_spiffs_unregister(NULL);
    ESP_LOGI(TAG, "Config Cleared");
}

void wifi_config_read_into_params() {

  printf("\n *************************************************\n");
  printf("\ncJSON: config reading into variables...start..%s", rendered);
	root = cJSON_Parse(rendered);
	if (root == NULL) {
		printf("...creating root object at init");
		root=cJSON_CreateObject();	
    cJSON *beacons = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "beacons", beacons);
	}
	if (cJSON_GetObjectItem(root,"Name")) {
		printf("\n Name: %s", cJSON_GetObjectItem(root,"Name")->valuestring);
	}
	if (cJSON_GetObjectItem(root,"MQTT")) {
		printf("\n MQTT: %d", cJSON_GetObjectItem(root,"MQTT")->valueint);
		mqtt = cJSON_GetObjectItem(root,"MQTT")->valueint;
	}
	if (cJSON_GetObjectItem(root,"MQTT_TOKEN")) {
		printf("\n MQTT Token: %s", cJSON_GetObjectItem(root,"MQTT_TOKEN")->valuestring);
		strcpy(mqtt_topic, cJSON_GetObjectItem(root,"MQTT_TOKEN")->valuestring);
	}
  if (cJSON_GetObjectItem(root,"DDNS")) {
    printf("\n DDNS: %s", cJSON_GetObjectItem(root,"DDNS")->valuestring);
    strcpy(ddns_uri, cJSON_GetObjectItem(root,"DDNS")->valuestring);
  }
  
  cJSON *item = cJSON_GetObjectItem(root,"beacons");
  for (int i = 0 ; i < cJSON_GetArraySize(item) ; i++) {
     cJSON *subitem = cJSON_GetArrayItem(item, i);
     strcpy(ble[ble_index].bleName, cJSON_GetObjectItem(subitem, "BLENAME")->valuestring);
     strcpy(ble[ble_index].bleTag, cJSON_GetObjectItem(subitem, "BLETAG")->valuestring);
     strcpy(ble[ble_index].notifyOn, cJSON_GetObjectItem(subitem, "BLENOTIFYON")->valuestring);
     strcpy(ble[ble_index].startTime, cJSON_GetObjectItem(subitem, "BLESTARTTIME")->valuestring);
     strcpy(ble[ble_index].endTime, cJSON_GetObjectItem(subitem, "BLEENDTIME")->valuestring);
     ble[ble_index].bleID = cJSON_GetObjectItem(subitem, "BLEID")->valueint;

     printf("\n Reading in beacons: %d:%s:%s:%s:%s:%s:%d", ble_index, 
                ble[ble_index].bleName, ble[ble_index].bleTag, 
                ble[ble_index].notifyOn, ble[ble_index].startTime,
                ble[ble_index].endTime, ble[ble_index].bleID);
     ble_index++;
  }
	printf("\ncJSON: config read...end");
	printf("\n *************************************************\n");
}

void configCommit() {
  struct stat st;

	char* tmp=cJSON_Print(root);
	if (tmp == NULL) {
		printf("\n Config is NULL");
    if (root == NULL) {
      printf(".....creating root object at config commit");
      root=cJSON_CreateObject();  
      cJSON *beacons = cJSON_CreateArray();
      cJSON_AddItemToObject(root, "beacons", beacons);
    }
  } else {
		printf("\nRendered String: %s\n", tmp);
	}
  
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
      if (ret == ESP_FAIL)
        ESP_LOGE(TAG, "Failed to mount or format filesystem");
      else if (ret == ESP_ERR_NOT_FOUND)
        ESP_LOGE(TAG, "Failed to find spiffs partition");
      else 
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
      return ;
    }
    
    ESP_LOGI(TAG, "Check for file");
    if (stat("/spiffs/hello", &st) == 0)
      printf("..present");
    else
      printf("..not present");

    ESP_LOGI(TAG, "Opening File for Writing..");
    // TBD - need to see if ESP-IDF upgrade will resolve this
    // r+, a+ gives no errors, but nothing gets written in the file
    // w and w+ fails at fopen with errno 2
    FILE* f = fopen("/spiffs/hello", "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %d", errno);
        esp_vfs_spiffs_unregister(NULL);
        return;
    }
    int written = fwrite(tmp, 1, strlen(tmp)+1, f);
    printf("\nConfig Committed %d:%d bytes", strlen(tmp)+1, written);

    fclose(f);
    esp_vfs_spiffs_unregister(NULL);
    printf("\nSpiffs unmounted..");
}

void wifi_config_write_string(char* type, char* val) {
	if (cJSON_GetObjectItem(root, type))
		cJSON_GetObjectItem(root, type)->valuestring = val;
	else {
		cJSON_AddItemToObject(root, type, cJSON_CreateString(val));
		printf("\n adding new string entry for %s", type);
	}
	configCommit();
}

void wifi_config_write_int(char* type, int val) {
	if (cJSON_GetObjectItem(root, type))
		cJSON_GetObjectItem(root, type)->valueint = val;
	else {
		cJSON_AddItemToObject(root, type, cJSON_CreateNumber(val));
		printf("\n adding new int entry for %s", type);
	}
	configCommit();
}

int wifi_config_write_beacons(char* name, char* tag, char* notifyOn, 
  char* startTime, char* endTime, int bleID) {
  cJSON *beacons = cJSON_GetObjectItem(root,"beacons");
  for (int i = 0 ; i < cJSON_GetArraySize(beacons) ; i++) {
     cJSON *subitem = cJSON_GetArrayItem(beacons, i);
     if (strcmp(name, cJSON_GetObjectItem(subitem, "BLENAME")->valuestring) == 0) {
        printf("\n BLE Name already present..."); return 0;
     }
  }

  int freeIndex = cJSON_GetArraySize(beacons);
  
  cJSON *beacon = cJSON_CreateObject();
  cJSON_AddItemToArray(beacons, beacon);
  
  cJSON_AddItemToObject(beacon, "BLENAME", cJSON_CreateString(name));
  cJSON_AddItemToObject(beacon, "BLETAG", cJSON_CreateString(tag));
  cJSON_AddItemToObject(beacon, "BLENOTIFYON", cJSON_CreateString(notifyOn));
  cJSON_AddItemToObject(beacon, "BLESTARTTIME", cJSON_CreateString(startTime));
  cJSON_AddItemToObject(beacon, "BLEENDTIME", cJSON_CreateString(endTime));
  cJSON_AddItemToObject(beacon, "BLEID", cJSON_CreateNumber(bleID));

  printf("\nConfig: adding new beacon entry for %s:%s, size:x:%d", name, tag, freeIndex);
  configCommit();
  return 1;
}