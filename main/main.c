

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"

#include "configRINA.h"
#include "IPCP.h"
#include "RINA_API.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define TAG_APP "[PING-PERF]"
#define SDU_SIZE (1480)
#define NUMBER_OF_PACKETS (1000)
#define INTERVAL (1)
#define RP_OPCODE_PERF 2
#define DIF "mobile.DIF"
#define SERVER "sensor1"
#define CLIENT "ping"

struct rp_config_msg
{
    uint64_t cnt;    /* packet/transaction count for the test
                      * (0 means infinite) */
    uint32_t opcode; /* opcode: ping, perf, rr ... */
    uint32_t ticket; /* valid with RP_OPCODE_DATAFLOW */
    uint32_t size;   /* packet size in bytes */
} __attribute__((packed));

struct rp_ticket_msg
{
    uint32_t ticket; /* ticket allocated by the server for the
                      * client to identify the data flow */
} __attribute__((packed));

// bufferTx = pvPortMalloc(size_sdu);

// memset(bufferTx, 'x', (size_t)SDU_SIZE);
// memset(bufferTx + SDU_SIZE, '\0', 1);

SemaphoreHandle_t xMutex;

void control_task(void *pvParameters)
{

    struct rinaFlowSpec_t *xFlowSpec;
    uint8_t Flags = 1;
    xFlowSpec = pvPortMalloc(sizeof(*xFlowSpec));

    size_t txMsg = 0;
    size_t uxTxBytes = 0;
    size_t rxMsg = 0;
    int i = 0;

    int RTT[NUMBER_OF_PACKETS];

    int64_t time_start = 0,
            time_end = 0;

    int time_delta = 0;
    int time_init = 0;

    float pps, bps;
    double time_inter;
    struct rp_config_msg cfg;
    struct rp_ticket_msg tmsg;

    size_t size_sdu = (size_t)SDU_SIZE + 1;
    void *bufferTx;
    void *bufferRx;
    volatile portId_t xControlPortId;

    for (;;)
    {
        xSemaphoreTake(xMutex, portMAX_DELAY);

        ESP_LOGI(TAG_APP, "----------- Requesting a Flow to Control ----- ");

        // Requesting a controll flow
        xControlPortId = RINA_flow_alloc(DIF, CLIENT, SERVER, xFlowSpec, Flags);

        ESP_LOGI(TAG_APP, "Flow Port id: %d ", xControlPortId);

        vTaskDelay(100);

        xSemaphoreGive(xMutex);

        // ESP_LOGI(TAG_APP, "----------- No should show ----- ");
        /*
                if (xControlPortId == -1)
                {
                    ESP_LOGE(TAG_APP, "rina_flow_alloc failed");
                }

                cfg.cnt = 10;
                cfg.size = sizeof(uint16_t);
                cfg.opcode = RP_OPCODE_PERF;

                txMsg = RINA_flow_write(xControlPortId, (void *)&cfg, sizeof(cfg));

                if (txMsg <= 0)
                {
                    ESP_LOGE(TAG_APP, "Error to send Data");
                    // break;
                }
                else
                {
                    vTaskDelay(50);
                    rxMsg = RINA_flow_read(xControlPortId, (void *)&tmsg, sizeof(tmsg));

                    if (rxMsg != sizeof(tmsg))
                    {
                        ESP_LOGE(TAG_APP, "Error read ticket: %d", rxMsg);
                    }
                    ESP_LOGE(TAG_APP, "Ticket received: %d", rxMsg);
                    // vTaskDelay(10);
                }*/
    }
}

void data_task(void *pvParameters)
{

    volatile struct rinaFlowSpec_t *xFlowSpec1;
    volatile uint8_t Flags1 = 1;
    xFlowSpec1 = pvPortMalloc(sizeof(*xFlowSpec1));

    volatile portId_t xDataPortId;

    for (;;)
    {

        xSemaphoreTake(xMutex, portMAX_DELAY);

        ESP_LOGI(TAG_APP, "----------- Requesting a Flow to Data ----- ");

        vTaskDelay(100);

        xDataPortId = RINA_flow_alloc(DIF, CLIENT, SERVER, xFlowSpec1, Flags1);
        ESP_LOGE(TAG_APP, "Flow Port id: %d ", xDataPortId);

        vTaskDelay(100);

        if (xDataPortId != -1)
        {
            ESP_LOGI(TAG_APP, "-----------------------------------------------\n ");
            ESP_LOGI(TAG_APP, "Client connecting to %s, port %d \n: ", SERVER, xDataPortId);
            ESP_LOGI(TAG_APP, "-----------------------------------------------\n ");
        }
        xSemaphoreGive(xMutex);
    }
}

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

    vTaskDelay(1000);

    xMutex = xSemaphoreCreateMutex();

    xTaskCreate(control_task, "Control", 1024 * 6, NULL, 6, NULL);
    xTaskCreate(data_task, "Data", 1024 * 4, NULL, 3, NULL);

    vTaskStartScheduler();
}