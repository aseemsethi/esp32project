#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "common.h"

#define ESP_INTR_FLAG_DEFAULT 0
#define CONFIG_BUTTON_PIN 0  // for ESP32
//#define CONFIG_BUTTON_PIN 22 // for WEMOS TTGO OLED

static SemaphoreHandle_t xSemaphore = NULL;

// interrupt service routine, called when the button is pressed
void IRAM_ATTR erase_isr_handler(void* arg) {	
    // notify the button task
	xSemaphoreGiveFromISR(xSemaphore, NULL);
}

// task that will react to button clicks
// If the user pushes the "boot" button on the ESP32 board, it raises
// a GPIO 0 interrupt and it will erase the WiFi NVS storage in flash
// clearing the username/password.
void erase_task(void* arg) {
	
	// infinite loop
	for(;;) {
		// wait for the notification from the ISR
		if(xSemaphoreTake(xSemaphore,portMAX_DELAY) == pdTRUE) {
			printf("\n !! Erase Button pressed!\n");
			nvs_flash_erase();
		}
	}
}

void wifi_eraseconfig() {
	printf("\n !! wifi eraseconfig module initialized");
	// create the binary semaphore
	xSemaphore = xSemaphoreCreateBinary();
	// configure button and led pins as GPIO pins
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	
	// set the correct direction
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
	
	// enable interrupt on falling (1->0) edge for button pin
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
	// start the task that will handle the button
	xTaskCreate(erase_task, "erase_task", 2048, NULL, 10, NULL);
	
	// install ISR service with default configuration
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	
	// attach the interrupt service routine
	gpio_isr_handler_add(CONFIG_BUTTON_PIN, erase_isr_handler, NULL);
}

