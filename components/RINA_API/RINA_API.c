/*
 * RINA_API.c
 *
 *  Created on: 12 jan. 2022
 *      Author: i2CAT- David Sarabia
 */

#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "BufferManagement.h"
#include "configSensor.h"
#include "common.h"
#include "esp_log.h"
#include "RINA_API.h"
#include "rstr.h"
#include "FlowAllocator.h"
#include "normalIpcp.h"

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

struct appRegistration_t *RINA_application_register(string_t pcNameDif,
                                                    string_t pcLocalApp,
                                                    uint8_t Flags);

struct appRegistration_t *RINA_application_register(string_t pcNameDif,
                                                    string_t pcLocalApp,
                                                    uint8_t Flags)
{
    name_t *xDn, *xAppn, *xDan;
    registerApplicationHandle_t *xRegAppRequest = pvPortMalloc(sizeof(*xRegAppRequest));
    RINAStackEvent_t xStackAppRegistrationEvent = {eStackAppRegistrationEvent, NULL};

    /*Check Flags (RINA_F_NOWAIT)*/

    /*Create name_t objects*/
    xDn = pxRStrNameCreate();
    xAppn = pxRStrNameCreate();
    xDan = pxRStrNameCreate();
#if 0
    if (pcNameDif && xRinaNameFromString(pcNameDif, xDn))
    {
        ESP_LOGE(TAG_RINA, "DIFName incorrect");
        xRinaNameFree(xDn);
        return NULL;
    }
    if (pcLocalApp && xRinaNameFromString(pcLocalApp, xAppn))
    {
        ESP_LOGE(TAG_RINA, "LocalName incorrect");
        xRinaNameFree(pcNameDif);
        xRinaNameFree(xAppn);
        return NULL;
    }
    if (!xDan)
    {
        xRinaNameFree(xDn);
        xRinaNameFree(xAppn);
        return NULL;
    }

    /*Structure and event to be send to the RINA Stack */

    xRegAppRequest->xSrcIpcpId = 0;
    xRegAppRequest->xDestIpcpId = 0;
    xRegAppRequest->xDestPort = 1;
    xRegAppRequest->xSrcPort = 1; // Should be random?

    xRegAppRequest->pxAppName = xAppn;
    xRegAppRequest->pxDifName = xDn;
    xRegAppRequest->pxDafName = xDan;

    xStackAppRegistrationEvent.pvData = xRegAppRequest;

    if (xSendEventStructToIPCPTask(&xStackAppRegistrationEvent, (TickType_t)0U) == pdPASS)
    {
        return NULL; // Should return the object Application Registration.
        // It's not clear how to wait until the response.
    }
#endif
    return NULL;
}

BaseType_t xRINA_bind(flowAllocateHandle_t *pxFlowRequest)
{
    RINAStackEvent_t xBindEvent;

    if (!pxFlowRequest)
    {
        ESP_LOGE(TAG_RINA, "No flow Request");
    }
    xBindEvent.eEventType = eFlowBindEvent;
    xBindEvent.pvData = (void *)pxFlowRequest;

    if (xSendEventStructToIPCPTask(&xBindEvent, (TickType_t)portMAX_DELAY) == pdFAIL)
    {
        /* Failed to wake-up the IPCP-task, no use to wait for it */
        return -1;
    }
    else
    {
        /* The IPCP-task will set the 'eFLOW_BOUND' bit when it has done its
         * job. */
        (void)xEventGroupWaitBits(pxFlowRequest->xEventGroup,
                                  (EventBits_t)eFLOW_BOUND,
                                  pdTRUE /*xClearOnExit*/,
                                  pdFALSE /*xWaitAllBits*/,
                                  portMAX_DELAY);

        ESP_LOGD(TAG_RINA, "Flow Bound");
        return 0;
    }
}

void vRINA_WeakUpUser(flowAllocateHandle_t *pxFlowAllocateResponse)
{
    ESP_LOGD(TAG_RINA, "Weaking up user");
    if (!pxFlowAllocateResponse)
    {
        ESP_LOGE(TAG_RINA, "No Bits set");
    }

    if ((pxFlowAllocateResponse->xEventGroup != NULL) && (pxFlowAllocateResponse->xEventBits != 0U))
    {
        (void)xEventGroupSetBits(pxFlowAllocateResponse->xEventGroup, pxFlowAllocateResponse->xEventBits);
    }

    pxFlowAllocateResponse->xEventBits = 0U;
}

static flowAllocateHandle_t *prvRINACreateFlowRequest(string_t pcNameDIF,
                                                      string_t pcLocalApp,
                                                      string_t pcRemoteApp,
                                                      struct rinaFlowSpec_t *xFlowSpec)
{
    ESP_LOGD(TAG_RINA, "Creating a new flow request");
    portId_t xPortId; /* PortId to return to the user*/
    name_t *pxDIFName, *pxLocalName, *pxRemoteName;
    struct flowSpec_t *pxFlowSpecTmp;
    EventGroupHandle_t xEventGroup;
    flowAllocateHandle_t *pxFlowAllocateRequest;

    pxFlowSpecTmp = pvPortMalloc(sizeof(*pxFlowSpecTmp));

    pxFlowAllocateRequest = pvPortMalloc(sizeof(*pxFlowAllocateRequest));

    xEventGroup = xEventGroupCreate();

    if (xEventGroup == NULL)
    {
        vPortFree(pxFlowAllocateRequest);
        vPortFree(pxFlowSpecTmp);
        return 0;
    }
    else
    {

        /* Check Flow spec ok version*/

        /*Check Flags*/
        (void)memset(pxFlowAllocateRequest, 0, sizeof(*pxFlowAllocateRequest));

        /*Create objetcs type name_t from string_t*/
        pxDIFName = pxRStrNameCreate();
        pxLocalName = pxRStrNameCreate();
        pxRemoteName = pxRStrNameCreate();

        if (!pxDIFName || !pxLocalName || !pxRemoteName)
        {
            ESP_LOGE(TAG_RINA, "Rina Names were not created properly");
        }

        // ESP_LOGD(TAG_RINA, "Rina Names created properly");

        if (!xRinaNameFromString(pcNameDIF, pxDIFName))
        {
            ESP_LOGE(TAG_RINA, "No possible to convert String to Rina Name");
            vRstrNameFree(pxDIFName);
            return -1;
        }

        if (!pcLocalApp || !xRinaNameFromString(pcLocalApp, pxLocalName))
        {
            ESP_LOGE(TAG_RINA, "LocalName incorrect");
            vRstrNameFree(pxDIFName);
            vRstrNameFree(pxLocalName);
            return -1;
        }
        if (!pcRemoteApp || !xRinaNameFromString(pcRemoteApp, pxRemoteName))
        {
            ESP_LOGE(TAG_RINA, "RemoteName incorrect");
            vRstrNameFree(pxDIFName);
            vRstrNameFree(pxLocalName);
            vRstrNameFree(pxRemoteName);
            return -1;
        }

        /*xPortId set to zero until the TASK fill properly.*/
        xPortId = xIPCPAllocatePortId();
        ESP_LOGD(TAG_RINA, "Port Id: %d Allocated", xPortId);

        /*Struct Data to sent attached into the event*/

        if (pxFlowAllocateRequest != NULL)
        {
            pxFlowAllocateRequest->xEventGroup = xEventGroup;
            pxFlowAllocateRequest->xReceiveBlockTime = FLOW_DEFAULT_RECEIVE_BLOCK_TIME;
            pxFlowAllocateRequest->xSendBlockTime = FLOW_DEFAULT_SEND_BLOCK_TIME;

            pxFlowAllocateRequest->pxLocal = pxLocalName;
            pxFlowAllocateRequest->pxRemote = pxRemoteName;
            pxFlowAllocateRequest->pxDifName = pxDIFName;
            pxFlowAllocateRequest->pxFspec = pxFlowSpecTmp;
            pxFlowAllocateRequest->xPortId = xPortId;

            vListInitialise(&pxFlowAllocateRequest->xListWaitingPackets);

            if (!xFlowSpec)
            {
                pxFlowAllocateRequest->pxFspec->ulAverageBandwidth = 0;
                pxFlowAllocateRequest->pxFspec->ulAverageSduBandwidth = 0;
                pxFlowAllocateRequest->pxFspec->ulDelay = 0;
                pxFlowAllocateRequest->pxFspec->ulJitter = 0;
                pxFlowAllocateRequest->pxFspec->usLoss = 10000;
                pxFlowAllocateRequest->pxFspec->ulMaxAllowableGap = 10;
                pxFlowAllocateRequest->pxFspec->xOrderedDelivery = false;
                pxFlowAllocateRequest->pxFspec->ulUndetectedBitErrorRate = 0;
                pxFlowAllocateRequest->pxFspec->xPartialDelivery = true;
                pxFlowAllocateRequest->pxFspec->xMsgBoundaries = false;
            }
            else
            {
                pxFlowAllocateRequest->pxFspec->ulAverageBandwidth = xFlowSpec->avg_bandwidth;
                pxFlowAllocateRequest->pxFspec->ulAverageSduBandwidth = 0;
                pxFlowAllocateRequest->pxFspec->ulDelay = xFlowSpec->max_delay;
                pxFlowAllocateRequest->pxFspec->ulJitter = xFlowSpec->max_jitter;
                pxFlowAllocateRequest->pxFspec->usLoss = xFlowSpec->max_loss;
                pxFlowAllocateRequest->pxFspec->ulMaxAllowableGap = xFlowSpec->max_sdu_gap;
                pxFlowAllocateRequest->pxFspec->xOrderedDelivery = xFlowSpec->in_order_delivery;
                pxFlowAllocateRequest->pxFspec->ulUndetectedBitErrorRate = 0;
                pxFlowAllocateRequest->pxFspec->xPartialDelivery = true;
                pxFlowAllocateRequest->pxFspec->xMsgBoundaries = xFlowSpec->msg_boundaries;
            }
        }
    }
    return pxFlowAllocateRequest;
}

BaseType_t RINA_flowStatus(portId_t xAppPortId)
{
    // Request to the Ipcm to check what is the flow status of that port Id

    if (xNormalIsFlowAllocated(xAppPortId) == pdFALSE)
    {
        return -1;
    }

    return 1;
}

BaseType_t prvConnect(flowAllocateHandle_t *pxFlowAllocateRequest)
{
    BaseType_t xResult = 0;
    RINAStackEvent_t xStackFlowAllocateEvent = {eStackFlowAllocateEvent, NULL};

    if (pxFlowAllocateRequest == NULL)
    {
        ESP_LOGE(TAG_RINA, "No flow request passed");
        xResult = -1;
    }
    else if (RINA_flowStatus(pxFlowAllocateRequest->xPortId) == 1) // check if the flow is already allocated
    {
        ESP_LOGE(TAG_RINA, "There is a flow allocated for that port Id");
        xResult = -1;
    }

    xResult = xRINA_bind(pxFlowAllocateRequest);

    if (xResult == 0)
    {

        ESP_LOGD(TAG_RINA, "Sending Flow Allocate Request");
        /*This must be changed: The idea is that the main task receives the request to allocate
        the Flow and the send it to the FlowAllocator module.*/
        xStackFlowAllocateEvent.pvData = (void *)pxFlowAllocateRequest;

        if (xSendEventStructToIPCPTask(&xStackFlowAllocateEvent, (TickType_t)portMAX_DELAY) == pdFAIL)
        {
            /* Failed to wake-up the RINA-task, no use to wait for it */
            return -1;
        }

        pxFlowAllocateRequest->usTimeout = 1U;

        if (xSendEventToIPCPTask(eFATimerEvent) != pdPASS)
        {
            xResult = -1;
        }
    }
    return xResult;
}

portId_t RINA_flow_alloc(string_t pcNameDIF,
                         string_t pcLocalApp,
                         string_t pcRemoteApp,
                         struct rinaFlowSpec_t *xFlowSpec,
                         uint8_t Flags);

portId_t RINA_flow_alloc(string_t pcNameDIF,
                         string_t pcLocalApp,
                         string_t pcRemoteApp,
                         struct rinaFlowSpec_t *xFlowSpec,
                         uint8_t Flags)
{

    flowAllocateHandle_t *pxFlowAllocateRequest;
    TickType_t xRemainingTime = portMAX_DELAY;
    BaseType_t xTimed = pdFALSE; /* Check non-blocking*/
    TimeOut_t xTimeOut;
    BaseType_t xResult = -1;

    pxFlowAllocateRequest = prvRINACreateFlowRequest(pcNameDIF, pcLocalApp, pcRemoteApp, xFlowSpec);

    ESP_LOGI(TAG_RINA, "Connecting to IPCP Task");

    xResult = prvConnect(pxFlowAllocateRequest);

    if (xResult == 0)
    {
        /* Waiting for the flow allocation response*/

        for (;;)
        {
            if (xTimed == pdFALSE)
            {
                xRemainingTime = portMAX_DELAY; // pxFlowAllocateRequest->xReceiveBlockTime;

                if (xRemainingTime == (TickType_t)0)
                {
                    xResult = -1;
                    break;
                }

                xTimed = pdTRUE;

                vTaskSetTimeOutState(&xTimeOut);
            }

            xResult = RINA_flowStatus(pxFlowAllocateRequest->xPortId);

            if (xResult > 0)
            {
                xResult = 0;
                break;
            }

            if (xTaskCheckForTimeOut(&xTimeOut, &xRemainingTime) != pdFALSE)
            {
                xResult = -1;
                break;
            }

            (void)xEventGroupWaitBits(pxFlowAllocateRequest->xEventGroup,
                                      (EventBits_t)eFLOW_ACCEPT,
                                      pdTRUE,
                                      pdFALSE,
                                      xRemainingTime);
        }
        /* The IPCP-task will set the 'eFLOW_ACCEPT' bit when it has done its
         * job. */
    }

    if (xResult != 0)
    {
        ESP_LOGE(TAG_RINA, "Flow was not allocated");
        return xResult;
    }

    ESP_LOGI(TAG_RINA, "Flow allocated in the port Id:%d", pxFlowAllocateRequest->xPortId);
    return pxFlowAllocateRequest->xPortId;
}

size_t RINA_flow_write(portId_t xPortId, void *pvBuffer, size_t uxTotalDataLength);
size_t RINA_flow_write(portId_t xPortId, void *pvBuffer, size_t uxTotalDataLength)
{
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    void *pvCopyDest;
    TimeOut_t xTimeOut;
    TickType_t xTicksToWait;
    RINAStackEvent_t xStackTxEvent = {eStackTxEvent, NULL};

    int32_t time_Init_Tx;

    time_Init_Tx = esp_timer_get_time();

    // ESP_LOGE(TAG_RINA, "*********** TIME INIT TX Data *******");
    // ESP_LOGE(TAG_RINA, "TIme Init: %d", time_Init_Tx);

    /*Check that DataLength is not longer than MAX_SDU_SIZE*/
    // This should not consider??? because the delimiter split into several packets??

    if (uxTotalDataLength <= MAX_SDU_SIZE)
    {
        /*Check if the Flow is active*/
        xTicksToWait = (TickType_t)50U;

        /*Request a NetworkBuffer to copy data*/

        vTaskSetTimeOutState(&xTimeOut);

        pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(uxTotalDataLength, xTicksToWait); // sizeof length DataUser packet.

        if (pxNetworkBuffer != NULL)
        {
            pvCopyDest = (void *)pxNetworkBuffer->pucEthernetBuffer;
            (void)memcpy(pvCopyDest, pvBuffer, uxTotalDataLength);

            if (xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait) == pdTRUE)
            {
                /* The entire block time has been used up. */
                xTicksToWait = (TickType_t)0;
            }
        }

        if (pxNetworkBuffer != NULL)
        {
            /*Fill the pxNetworkBuffer descriptor properly*/
            pxNetworkBuffer->xDataLength = uxTotalDataLength;
            pxNetworkBuffer->ulBoundPort = xPortId;

            xStackTxEvent.pvData = pxNetworkBuffer;

            if (xSendEventStructToIPCPTask(&xStackTxEvent, xTicksToWait) == pdPASS)
            {
                return uxTotalDataLength;
            }
            else
            {
                vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
                return 0;
            }
        }

        return 0;
    }

    return 0;
}

BaseType_t RINA_flow_close(portId_t xAppPortId)
{
    BaseType_t xResult;

    RINAStackEvent_t xDeallocateEvent;
    xDeallocateEvent.eEventType = eFlowDeallocateEvent;
    xDeallocateEvent.pvData = xAppPortId;

    // Check if the port Id is valid. If it is valid, send a event to the IPCP task
    if ((xAppPortId == NULL) || (!is_port_id_ok(xAppPortId)))
    {
        xResult = pdFALSE;
    }
    else
    {

        (void)vFlowAllocatorDeallocate(xAppPortId);
        /*if (xSendEventStructToIPCPTask(&xDeallocateEvent, (TickType_t)portMAX_DELAY) == pdFAIL)
        {
            ESP_LOGE(TAG_RINA, "RINA Deallocate Flow: failed");
            xResult = pdFALSE;
        }
        else
        {
            xResult = pdTRUE;
        }*/

        xResult = pdTRUE;
    }

    return xResult;
}

int32_t RINA_flow_read(portId_t xPortId, void *pvBuffer, size_t uxTotalDataLength)
{

    BaseType_t xPacketCount;
    const void *pvCopySource; // to copy data from networkBuffer to pvBuffer
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    flowAllocateHandle_t *pxFlowHandle;
    TickType_t xRemainingTime = (TickType_t)10;
    BaseType_t xTimed = pdFALSE;
    TimeOut_t xTimeOut;
    int32_t lDataLength;
    EventBits_t xEventBits = (EventBits_t)0;
    size_t uxPayloadLength;
    int32_t timer_delta;

    ESP_LOGD(TAG_RINA, "RINA_FLOW_READ Called");
    timer_delta = esp_timer_get_time();
    ESP_LOGD(TAG_RINA, "Time Called: %d", timer_delta);

    /*Validate if the flow is valid, if the xPortId is on status CONNECTED*/
    if (RINA_flowStatus(xPortId) != 1)
    {
        ESP_LOGE(TAG_RINA, "There is not a flow allocated for that port Id");
        return 0;
    }
    else
    {
        /*Looking for the flow handle associated to that xPortId*/
        pxFlowHandle = pxFAFindFlowHandle(xPortId);
        timer_delta = esp_timer_get_time();
        ESP_LOGD(TAG_RINA, "Time Init check packet in the list: %d", timer_delta);

        /*Check if there is a packet in the list*/
        /*Maybe this will be changed. Instead, it could use the RINA queues*/
        xPacketCount = (BaseType_t)listCURRENT_LIST_LENGTH(&(pxFlowHandle->xListWaitingPackets));

        ESP_LOGE(TAG_RINA, "Numbers of packet in the queue: %d", xPacketCount);
        timer_delta = esp_timer_get_time();
        ESP_LOGE(TAG_RINA, "Time finit check packet in the list: %d", timer_delta);

        /*Listening to packets in the queue. It blocks until the RINA stack has a packet
        The RINA stack sends a message by setting the eFLOW_RECEIVE bits*/
        while (xPacketCount == 0)
        {
            timer_delta = esp_timer_get_time();
            ESP_LOGE(TAG_RINA, "Time init check blocking: %d", timer_delta);
            if (xTimed == pdFALSE)
            {
                /* Check to see if the flow is non blocking on the first
                 * iteration.  */

                xRemainingTime = pxFlowHandle->xReceiveBlockTime;
                ESP_LOGE(TAG_RINA, "xRemainingTime: %d", xRemainingTime);

                if (xRemainingTime == (TickType_t)0)
                {

                    /*check for the interrupt flag. */
                    ESP_LOGE(TAG_RINA, "xRemainingTime: %d", xRemainingTime);

                    break;
                }

                /* To ensure this part only executes once. */
                xTimed = pdTRUE;

                timer_delta = esp_timer_get_time();
                ESP_LOGE(TAG_RINA, "Time finit check blocking: %d", timer_delta);

                /* Fetch the current time. */
                vTaskSetTimeOutState(&xTimeOut);
            }

            ESP_LOGD(TAG_RINA, "waiting...");
            timer_delta = esp_timer_get_time();
            ESP_LOGD(TAG_RINA, "Time init waiting: %d", timer_delta);

            /* Wait for arrival of data.  While waiting, the IPCP-task may set the
             * 'eFLOW_RECEIVE' bit in 'xEventGroup', if it receives data for this
             * flow, thus unblocking this API call. */
            xEventBits = xEventGroupWaitBits(pxFlowHandle->xEventGroup, ((EventBits_t)eFLOW_RECEIVE),
                                             pdTRUE /*xClearOnExit*/, pdFALSE /*xWaitAllBits*/, xRemainingTime);

            timer_delta = esp_timer_get_time();
            ESP_LOGD(TAG_RINA, "Time finit waiting: %d", timer_delta);

            /*
                        {
                            (void)xEventBits;
                        }
            */
            xPacketCount = (BaseType_t)listCURRENT_LIST_LENGTH(&(pxFlowHandle->xListWaitingPackets));

            if (xPacketCount != 0)
            {
                break;
            }

            xTaskCheckForTimeOut(&xTimeOut, &xRemainingTime);
            ESP_LOGD(TAG_RINA, "xRemainingTime: %d", xRemainingTime);
            /* Has the timeout been reached ? */
            if (xTaskCheckForTimeOut(&xTimeOut, &xRemainingTime) != pdFALSE)
            {
                ESP_LOGE(TAG_RINA, "xRemainingTime: %d", xRemainingTime);
                break;
            }

            timer_delta = esp_timer_get_time();
            ESP_LOGD(TAG_RINA, "Time finit waiting: %d", timer_delta);
        } /* End while */

        timer_delta = esp_timer_get_time();
        ESP_LOGD(TAG_RINA, "Time init ckeck packet: %d", timer_delta);

        /*There are packets in the queue.*/
        if (xPacketCount != 0)
        {

            taskENTER_CRITICAL(&mux);
            {
                /* The owner of the list item is the network buffer. */
                /* Get the first packet, and removed from the list*/
                pxNetworkBuffer = ((NetworkBufferDescriptor_t *)listGET_OWNER_OF_HEAD_ENTRY(&(pxFlowHandle->xListWaitingPackets)));
                (void)uxListRemove(&(pxNetworkBuffer->xBufferListItem));
            }
            taskEXIT_CRITICAL(&mux);

            lDataLength = (int32_t)pxNetworkBuffer->xDataLength;
            ESP_LOGD(TAG_RINA, "Reading %d bytes", lDataLength);

            // vPrintBytes((void *)pxNetworkBuffer->pucDataBuffer, lDataLength);

            /*Copy the SDU into the User buffer*/
            (void)memcpy(pvBuffer, (const void *)pxNetworkBuffer->pucDataBuffer, (size_t)lDataLength);

            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        }

        /*if not packets after a whilem then return error time out*/
        else
        {
            ESP_LOGE(TAG_RINA, "Error Timeout");
            lDataLength = -1;
        }
        timer_delta = esp_timer_get_time();
        ESP_LOGD(TAG_RINA, "Time finit ckeck packet: %d", timer_delta);
    }

    ESP_LOGD(TAG_RINA, "Returning to main");

    return lDataLength;
}