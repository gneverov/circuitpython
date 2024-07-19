#include "FreeRTOS.h"
#include "task.h"


void vApplicationIdleHook(void) {
};

void vApplicationTickHook(void) {
};

void vApplicationStackOverflowHook(TaskHandle_t Task, char *pcTaskName) {
    panic("stack overflow (not the helpful kind) for %s\n", *pcTaskName);
}

void vApplicationMallocFailedHook(void) {
    panic("Malloc Failed\n");
};
