idf_component_register(SRCS "wifi_main.c" "wifi_eraseconfig.c" "u8g2_oled.c" 
			"u8g2_esp32_hal.c" "smartconfig_main.c" "wifi_sniffer.c"
			"wifi_mqtt.c" "wifi_http.c" "wifi_config.c"
                    INCLUDE_DIRS "."
                    REQUIRES md5 u8g2 nvs_flash esp_http_server mqtt spiffs json)

# Create a SPIFFS image from the contents of the 'spiffs_image' directory
# that fits the partition named 'storage'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py flash'. 
spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)