

#ifndef _COMMON_R_FIFO
#define _COMMON_R_FIFO

#ifdef __cplusplus
extern "C"
{
#endif

#include "freertos/queue.h"

    typedef struct xQUEUE_FIFO
    {
        QueueHandle_t xQueue;

    } rfifo_t;

    rfifo_t *prvShimCreateQueue(void);
    BaseType_t xRfifoIsEmpty(rfifo_t *f);
    BaseType_t xRfifoPeek(rfifo_t *f, const void *vItemFromQueue);

    BaseType_t xRfifoPushHead(rfifo_t *f, const void *vItemToQueue);
    BaseType_t xRfifoPush(rfifo_t *f, const void *vItemToQueue);

    void *pxRfifoPop(rfifo_t *f);

    BaseType_t xRfifoDestroy(rfifo_t *f);
    rfifo_t *pxRfifoCreate(void);
    UBaseType_t uxRfifoMessagesWaiting(rfifo_t *f);

#ifdef __cplusplus
}
#endif

#endif