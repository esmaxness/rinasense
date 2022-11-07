#ifndef RMT_PS_DEFAULT_H
#define RMT_PS_DEFAULT_H

#include "Rmt.h"
#include "rfifo.h"
#include "configSensor.h"
#include "configRINA.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* rmt_enqueue_policy return values */
#define RMT_PS_ENQ_SEND 0  /* PDU can be transmitted by the RMT */
#define RMT_PS_ENQ_SCHED 1 /* PDU enqueued and RMT needs to schedule */
#define RMT_PS_ENQ_ERR 2   /* Error */
#define RMT_PS_ENQ_DROP 3  /* PDU dropped due to queue full occupation */

    struct rmtN1Port_t;

    typedef struct xRMT_QUEUE
    {
        rfifo_t *pxDtQueue;
        rfifo_t *pxMgtQueue;
        portId_t xPortId;
    } rmtQueue_t;

    UBaseType_t uxRmtEnqueueDfltPolicy(struct rmtN1Port_t *pxN1_port,
                                       struct du_t *pxDu,
                                       BaseType_t xMustEnqueue);

    struct du_t *pxRmtDequeueDfltPolicy(struct rmtN1Port_t *pxN1Port);
    BaseType_t xRmtQueueDestroy(rmtQueue_t *pxQueue);
    rmtQueue_t *pxRmtQueueCreate(portId_t xPortId);

#ifdef __cplusplus
}
#endif

#endif