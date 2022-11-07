#include "freertos/FreeRTOS.h"
#include <string.h>

#include "configRINA.h"
#include "IPCP.h"
#include "RINA_API.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "dht.h"

#define TAG_APP "[Sensor-APP]"

void app_main(void)
{
	nvs_flash_init();
	/*
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);*/

	RINA_IPCPInit();

	portId_t xAppPortId;
	struct rinaFlowSpec_t *xFlowSpec = pvPortMalloc(sizeof(*xFlowSpec));
	uint8_t Flags = 1;
	int i = 0;
	char json[200];
	float Humidity;
	float Temperature;
	char buffer[500];

	gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);
	vTaskDelay(1000);

	ESP_LOGI(TAG_APP, "----------- Requesting a Flow ----- ");

	xAppPortId = RINA_flow_alloc("mobile.DIF", "STH2", "sensor2", xFlowSpec, Flags);

	ESP_LOGI(TAG_APP, "Flow Port id: %d ", xAppPortId);
	if (xAppPortId != -1)
	{

		while (i < 100)
		{

			if (dht_read_float_data(DHT_TYPE_AM2301, GPIO_NUM_4,
									&Humidity, &Temperature) != ESP_OK)
			{

				ESP_LOGE(TAG_APP, "Error to read dht");
			}
			ESP_LOGI(TAG_APP, "Temperature: %.1f C", Temperature);
			ESP_LOGI(TAG_APP, "Humidity: %.1f%% ", Humidity);
			sprintf(json, "sensorId: STH1, Temperature: %.1f C, Humidity: %.1f%% \n",
					Temperature,
					Humidity);

			if (RINA_flow_write(xAppPortId, (void *)json, strlen(json)))
			{
				ESP_LOGI(TAG_APP, "Sent Data successfully");
			}

			vTaskDelay(1000000 / portTICK_RATE_MS);

			i = i + 1;
		}
	}
}
