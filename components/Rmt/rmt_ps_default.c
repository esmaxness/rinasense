

#include "Rmt.h"
#include "rmt_ps_default.h"
#include "rfifo.h"
#include "pci.h"
#include "configSensor.h"
#include "configRINA.h"

#include "esp_log.h"

rmtQueue_t *pxRmtQueueCreate(portId_t xPortId)
{
    rmtQueue_t *pxTmp;

    pxTmp = pvPortMalloc(sizeof(*pxTmp));
    if (!pxTmp)
        return NULL;

    pxTmp->pxDtQueue = pxRfifoCreate();
    if (!pxTmp->pxDtQueue)
    {
        vPortFree(pxTmp);
        return NULL;
    }

    pxTmp->pxMgtQueue = pxRfifoCreate();
    if (!pxTmp->pxMgtQueue)
    {
        xRfifoDestroy(pxTmp->pxDtQueue);
        vPortFree(pxTmp);
        return NULL;
    }

    pxTmp->xPortId = xPortId;

    return pxTmp;
}

BaseType_t xRmtQueueDestroy(rmtQueue_t *pxQueue)
{
    if (!pxQueue)
    {
        ESP_LOGE(TAG_QUEUE, "No RMT Key-queue to destroy...");
        return pdFALSE;
    }

    if (pxQueue->pxDtQueue)
        xRfifoDestroy(pxQueue->pxDtQueue);

    if (pxQueue->pxMgtQueue)
        xRfifoDestroy(pxQueue->pxMgtQueue);

    vPortFree(pxQueue);

    return pdTRUE;
}

UBaseType_t uxRmtEnqueueDfltPolicy(struct rmtN1Port_t *pxN1_port,
                                   struct du_t *pxDu,
                                   BaseType_t xMustEnqueue)
{
    rmtQueue_t *pxQueue;
    pduType_t xPduType;

    if (!pxN1_port || !pxDu)
    {
        ESP_LOGE(TAG_QUEUE, "Wrong input parameters");
        return RMT_PS_ENQ_ERR;
    }

    pxQueue = pxN1_port->pxRmtPsQueues;
    if (!pxQueue)
    {
        ESP_LOGE(TAG_QUEUE, "Could not find queue for n1_port %u",
                 pxN1_port->xPortId);
        xDuDestroy(pxDu);
        return RMT_PS_ENQ_ERR;
    }

    xPduType = pxDu->pxPci->xType;
    if (xPduType == PDU_TYPE_MGMT)
    {
        if (!xMustEnqueue && xRfifoIsEmpty(pxQueue->pxMgtQueue))
        {
            return RMT_PS_ENQ_SEND;
        }

        (void)xRfifoPush(pxQueue->pxMgtQueue, (void *)&pxDu);
        return RMT_PS_ENQ_SCHED;
    }

    if (!xMustEnqueue && xRfifoIsEmpty(pxQueue->pxDtQueue))
        return RMT_PS_ENQ_SEND;

    if (uxRfifoMessagesWaiting(pxQueue->pxDtQueue) == SIZE_SDU_QUEUE)
    {
        xDuDestroy(pxDu);
        return RMT_PS_ENQ_DROP;
    }

    (void)xRfifoPush(pxQueue->pxDtQueue, (void *)&pxDu);
    return RMT_PS_ENQ_SCHED;
}

struct du_t *pxRmtDequeueDfltPolicy(struct rmtN1Port_t *pxN1Port)
{
    rmtQueue_t *pxQueue;
    struct du_t *ret_du;

    if (!pxN1Port)
    {
        ESP_LOGE(TAG_QUEUE, "Wrong input parameters");
        return NULL;
    }

    pxQueue = pxN1Port->pxRmtPsQueues;
    if (!pxQueue)
    {
        ESP_LOGE(TAG_QUEUE, "Could not find queue for n1_port %u",
                 pxN1Port->xPortId);
        return NULL;
    }

    if (!xRfifoIsEmpty(pxQueue->pxMgtQueue))
        ret_du = (struct du_t *)pxRfifoPop(pxQueue->pxMgtQueue);
    else
        ret_du = (struct du_t *)pxRfifoPop(pxQueue->pxDtQueue);

    if (!ret_du)
    {
        ESP_LOGD(TAG_QUEUE, "Queue is Empty");
        return NULL;
    }

    return ret_du;
}