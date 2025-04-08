
#include "./utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t ticks() {
    return xTaskGetTickCount();
}

void delay(uint32_t duration) {
    vTaskDelay((duration + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);
}

char* taskName() {
    TaskStatus_t xTaskDetails;
    vTaskGetInfo(NULL, &xTaskDetails, pdFALSE, eInvalid);
    return xTaskDetails.pcTaskName;
}

void dumpBuffer(char *header, uint8_t *buffer, size_t length) {
    printf("%s 0x", header);
    for (int i = 0; i < length; i++) {
        //if ((i % 16) == 0) { printf("\n    "); }
        printf("%02x", buffer[i]);
        //if ((i % 4) == 3) { printf("  "); }
    }
    printf(" (length=%d)\n", length);
}
