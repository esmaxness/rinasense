
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "rfifo.h"
#include "configRINA.h"
#include "configSensor.h"

#include "esp_log.h"

rfifo_t *pxRfifoCreate(void)
{
    rfifo_t *pxFifo = pvPortMalloc(sizeof(*pxFifo));

    pxFifo->xQueue = xQueueCreate(SIZE_SDU_QUEUE, sizeof(uint32_t));

    if (!pxFifo->xQueue)
    {
        vPortFree(pxFifo);
        return NULL;
    }

    return pxFifo;
}

BaseType_t xRfifoDestroy(rfifo_t *f)
{
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    vQueueDelete(f->xQueue);

    vPortFree(f);

    ESP_LOGD(TAG_QUEUE, "FIFO %pK destroyed successfully", f);

    return pdTRUE;
}

BaseType_t xRfifoPush(rfifo_t *f, const void *vItemToQueue)
{
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    /*Send to Queue don`t block if the queue is already full*/
    if (xQueueSendToBack(f->xQueue, vItemToQueue, (TickType_t)0) == 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t xRfifoPushHead(rfifo_t *f, const void *vItemToQueue)
{
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    /*Send to Queue don`t block if the queue is already full*/
    if (xQueueSendToFront(f->xQueue, vItemToQueue, (TickType_t)0) == 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

void *pxRfifoPop(rfifo_t *f)
{
    const void *vItemFromQueue;

    vItemFromQueue = pvPortMalloc(sizeof(vItemFromQueue));

    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return NULL;
    }

    /*Send to Queue don`t block if the queue is already full*/
    if (xQueueReceive(f->xQueue, &vItemFromQueue, (TickType_t)0) == 0)
    {
        return NULL;
    }

    return vItemFromQueue;
}

BaseType_t xRfifoPeek(rfifo_t *f, const void *vItemFromQueue)
{
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    /*Send to Queue don`t block if the queue is already full*/
    if (xQueuePeek(f->xQueue, vItemFromQueue, (TickType_t)0) == 0)
    {
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t xRfifoIsEmpty(rfifo_t *f)
{
    UBaseType_t uxRet;
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    /*Send to Queue don`t block if the queue is already full*/
    uxRet = uxQueueSpacesAvailable(f->xQueue);

    if (uxRet == SIZE_SDU_QUEUE)
        return pdTRUE;

    return pdFALSE;
}

UBaseType_t uxRfifoMessagesWaiting(rfifo_t *f)
{
    UBaseType_t uxRet;
    if (!f)
    {
        ESP_LOGE(TAG_QUEUE, "Bogus input parameters, can't destroy NULL");
        return pdFALSE;
    }

    /*Send to Queue don`t block if the queue is already full*/
    uxRet = uxQueueMessagesWaiting(f->xQueue);

    return uxRet;
}
