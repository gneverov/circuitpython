// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "FreeRTOS.h"


void *pvPortMalloc(size_t xWantedSize) {
    void *pvReturn = NULL;
    pvReturn = malloc(xWantedSize);
    configASSERT(((intptr_t)pvReturn & portBYTE_ALIGNMENT_MASK) == 0);

    #if (configUSE_MALLOC_FAILED_HOOK == 1)
    {
        if (pvReturn == NULL) {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/

void vPortFree(void *pv) {
    free(pv);
}
/*-----------------------------------------------------------*/

size_t xPortGetFreeHeapSize(void) {
    struct mallinfo local_mallinfo = mallinfo();
    return local_mallinfo.fordblks;
}
/*-----------------------------------------------------------*/

void *pvPortCalloc(size_t xNum, size_t xSize) {
    void *pvReturn = NULL;
    pvReturn = calloc(xNum, xSize);
    configASSERT(((intptr_t)pvReturn & portBYTE_ALIGNMENT_MASK) == 0);

    #if (configUSE_MALLOC_FAILED_HOOK == 1)
    {
        if (pvReturn == NULL) {
            vApplicationMallocFailedHook();
        }
    }
    #endif

    return pvReturn;
}
/*-----------------------------------------------------------*/
