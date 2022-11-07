#include <stdint.h>
#include <limits.h>

//#include "portability/port.h"

#include "num_mgr.h"
#include "bit_array.h"

NumMgr_t *pxNumMgrCreate(size_t numCnt)
{
    NumMgr_t *nm;

    /* UINT_MAX is our error code. */
    if (numCnt == UINT_MAX)
        return NULL;

    nm = pvPortMalloc(sizeof(NumMgr_t));
    if (!nm)
        return NULL;

    memset(nm, 0, sizeof(NumMgr_t));

    if (!xNumMgrInit(nm, numCnt))
    {
        vPortFree(nm);
        return NULL;
    }

    return nm;
}

BaseType_t xNumMgrInit(NumMgr_t *im, size_t numCnt)
{
    im->numCnt = numCnt + 1;
    im->lastAllocated = 0;

    im->ba = pxBitArrayAlloc(im->numCnt);
    if (!im->ba)
        return pdFALSE;

    return pdTRUE;
}

void vNumMgrFini(NumMgr_t *im)
{
    configASSERT(im != NULL);
    configASSERT(im->ba != NULL);

    vBitArrayFree(im->ba);
}

void vNumMgrDestroy(NumMgr_t *im)
{
    configASSERT(im != NULL);

    vPortFree(im);
}

uint32_t ulNumMgrAllocate(NumMgr_t *im)
{
    uint32_t p;

    configASSERT(im != NULL);

    p = im->lastAllocated + 1;

    /* Loops from the next to last allocated port, until we reach the last
     * allocated port again, wrapping over MAX_PORT_ID. */
    for (;; p++)
    {

        /* We return UINT_MAX if we overflow. */
        if (p == im->lastAllocated)
            return UINT_MAX;

        if (p == im->numCnt)
            p = 1;

        if (!xBitArrayGetBit(im->ba, p))
        {
            vBitArraySetBit(im->ba, p);
            im->lastAllocated = p;
            break;
        }
    }

    return p;
}

BaseType_t xNumMgrRelease(NumMgr_t *im, uint32_t n)
{
    configASSERT(im != NULL);

    if (!xBitArrayGetBit(im->ba, n))
        return pdFALSE;
    else
    {
        vBitArrayClearBit(im->ba, n);
        return pdTRUE;
    }
}

BaseType_t xNumMgrIsAllocated(NumMgr_t *im, uint32_t n)
{
    configASSERT(im != NULL);

    return xBitArrayGetBit(im->ba, n);
}