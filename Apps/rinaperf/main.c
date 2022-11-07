
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <rom/ets_sys.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "configRINA.h"
#include "IPCP.h"
#include "RINA_API.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define TAG_APP "[PING-PERF]"

#define SDU_SIZE (1480)       // Size of the SDUs
#define NUMBER_OF_PACKETS (0) // Number of packets to be sended during the test
#define INTERVAL (1)          // Time to wait after each SDUs is sent (default 10 miliseconds)
#define DURATION (10)         // Test duration in seconds

#define RP_OPCODE_PING 0
#define RP_OPCODE_PERF 2
#define RP_OPCODE_DATAFLOW 3
#define RP_OPCODE_STOP 4 /* must be the last */

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

struct rp_result_msg
{
    uint64_t cnt;     /* number of packets or completed transactions
                       * as seen by the sender or the receiver */
    uint64_t pps;     /* average packet rate measured by the sender or receiver */
    uint64_t bps;     /* average bandwidth measured by the sender or receiver */
    uint64_t latency; /* in nanoseconds */
} __attribute__((packed));

struct rp_ticket_msg const tmsg;
struct rp_config_msg cfg;

struct rp_result_msg result;

portId_t xControlPortId;
struct rinaFlowSpec_t *xFlowSpec;
uint8_t Flags = 1;
portId_t xDataPortId;

size_t ret = 0;

unsigned int real_duration_ms; /* measured by the client */

unsigned long IRAM_ATTR micros()
{
    return (unsigned long)(esp_timer_get_time());
}

void IRAM_ATTR delayMicros(uint32_t us)
{
    uint32_t m = micros();
    if (us)
    {
        uint32_t e = (m + us);
        if (m > e)
        { // overflow
            while (micros() > e)
            {
                // NOP();
            }
        }
        while (micros() < e)
        {
            // NOP();
        }
    }
}

static int perf_client(portId_t xPerfPortId)
{
    unsigned limit = NUMBER_OF_PACKETS; // same config.cnt
    unsigned int interval = INTERVAL;   // number of miliseconds to wait after each SDUs is sent, if zero then default ticks is 10.
    unsigned int burst = 10;            // default how many SDU burst before waiting interval
    unsigned int cdown = burst;
    // struct rinaperf
    int64_t time_start, time_end;
    int64_t time1, time2;
    size_t size_sdu = (size_t)SDU_SIZE + 1;
    void *bufferTx;
    unsigned long us;
    unsigned int i = 0;
    int ret;

    bufferTx = pvPortMalloc((size_t)size_sdu);

    memset(bufferTx, 'x', (size_t)SDU_SIZE);
    memset(bufferTx + SDU_SIZE, '\0', 1);

    time_start = esp_timer_get_time();

#if DURATION
    int64_t delta;
    float remain = 0;
    while (remain < DURATION)
    {
#endif

#if NUMBER_OF_PACKETS
        for (i = 0; i < limit; i++)
        {
#endif
            ret = RINA_flow_write(xPerfPortId, (void *)bufferTx, strlen(bufferTx));
            if (ret != strlen(bufferTx))
            {
                if (ret < 0)
                {
                    ESP_LOGE(TAG_APP, "write(buf)");
                }
                else
                {
                    // ESP_LOGE(TAG_APP, "Partial write %d/%d\n", ret, strlen(bufferTx));
                }
                break;
            }

            if (interval > 10)
            {
                vTaskDelay(interval / portTICK_RATE_MS);
            }
            else
            {
                /* 10 ms by default*/
                vTaskDelay(10 / portTICK_RATE_MS);
            }
            cdown = burst;

#if NUMBER_OF_PACKETS
        }
#endif

#if DURATION
        i++;
        time_end = esp_timer_get_time();
        delta = time_end - time_start;
        remain = (float)delta / 1000000;
    }
#endif

    time_end = esp_timer_get_time();
    us = time_end - time_start;
    real_duration_ms = us / 1000;
    // ESP_LOGI(TAG_APP, "Duration test milliseconds: %f", (double)us / 1000.0);

    if (us)
    {
        result.cnt = i;
        result.pps = 1000000UL;
        result.pps *= i;
        result.pps /= us;
        result.bps = result.pps * 8 * SDU_SIZE;
    }

    cfg.cnt = i; /* write back packet count */
    return 0;
}

static void
perf_report(struct rp_result_msg rmsg)
{

    ESP_LOGI(TAG_APP, "%10s %12s %10s %10s\n", "", "Packets", "Kpps", "Kbps");
    ESP_LOGI(TAG_APP, "%-10s %12llu %10.3f %10.3f\n", "Sender",
             (long long unsigned)result.cnt, (double)result.pps / 1000.0,
             (double)result.bps / 1000.0);
    ESP_LOGI(TAG_APP, "%-10s %12llu %10.3f %10.3f\n", "Receiver",
             (long long unsigned)rmsg.cnt, (double)rmsg.bps / 1000000000.0,
             (double)rmsg.pps / 1000000000.0);
}

void app_main(void)
{

    nvs_flash_init();

    /*Init RINA Task*/
    RINA_IPCPInit();

    vTaskDelay(1000);

    uint32_t size = SDU_SIZE;
    uint32_t opcode = RP_OPCODE_PERF;
    struct rp_result_msg rmsg;

    /*Requesting a control flow*/
    xControlPortId = RINA_flow_alloc(DIF, CLIENT, SERVER, xFlowSpec, Flags);

    if (xControlPortId < 0)
    {
        ESP_LOGI(TAG_APP, "Error allocating the flow ");
    }

    /* Send a Message  with the parameters config to the server */
    cfg.cnt = 2000; // NUMBER_OF_PACKETS;
    cfg.size = size;
    cfg.opcode = opcode;

    ret = RINA_flow_write(xControlPortId, (void *)&cfg, sizeof(cfg));
    if (ret <= 0)
    {
        ESP_LOGE(TAG_APP, "Error to send Data");
        // break;
    }
    else
    {
        vTaskDelay(10);

        /*Read the ticket message sended by the server*/
        ret = RINA_flow_read(xControlPortId, (void *)&tmsg, sizeof(tmsg));

        if (ret != sizeof(tmsg))
        {
            ESP_LOGE(TAG_APP, "Error read ticket: %d", ret);
        }
    }

    /*Requesting Allocate a Data Flow*/
    xDataPortId = RINA_flow_alloc(DIF, CLIENT, SERVER, xFlowSpec, Flags);

    if (xDataPortId > 0)
    {
        /*Send Ticket to identify the data flow*/
        memset(&cfg, 0, sizeof(cfg));
        cfg.opcode = RP_OPCODE_DATAFLOW;
        cfg.ticket = tmsg.ticket;

        ret = RINA_flow_write(xDataPortId, &cfg, sizeof(cfg));
        if (ret != sizeof(cfg))
        {
            ESP_LOGE(TAG_APP, "Error write data");
        }
        if (cfg.opcode != opcode)
        {

            ESP_LOGI(TAG_APP, "Starting PERF; message size: %d, number of messages: %d,"
                              " duration: %d\n",
                     SDU_SIZE, NUMBER_OF_PACKETS, 100000);

            /*Calling perf_client to execute the test*/
            (void)perf_client(xDataPortId);

            ESP_LOGI(TAG_APP, "----------------------------------------");

            /*Sending Stop OpCode to finish the test*/
            memset(&cfg, 0, sizeof(cfg));
            cfg.opcode = RP_OPCODE_STOP;
            cfg.cnt = (uint64_t)1000;
            ret = RINA_flow_write(xControlPortId, &cfg, sizeof(cfg));
            if (ret != sizeof(cfg))
            {
                if (ret < 0)
                {
                    ESP_LOGE(TAG_APP, "Error write data");
                }
            }

            vTaskDelay(12000 / portTICK_RATE_MS);

            /* Reading the results message from server*/
            ret = RINA_flow_read(xControlPortId, &rmsg, sizeof(rmsg));
            if (ret != sizeof(rmsg))
            {
                if (ret < 0)
                {
                    ESP_LOGE(TAG_APP, "Error write data");
                }
                else
                {
                    ESP_LOGE(TAG_APP, "Error reading result message: wrong length %d "
                                      "(should be %lu)\n",
                             ret, (unsigned long int)sizeof(rmsg));
                }
            }
            else
            {
                // vPrintBytes((void *)&rmsg, ret);
            }

            /*Calling the Report*/
            (void)perf_report(rmsg);
        }
    }
}