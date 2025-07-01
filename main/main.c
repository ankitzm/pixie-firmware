
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device-info.h"
#include "events-private.h"
#include "task-ble.h"
#include "task-io.h"

#include "utils.h"

#include "panel-connect.h"
#include "panel-menu.h"
#include "panel-null.h"
#include "panel-space.h"
#include "panel-test.h"
#include "panel-tx.h"
//#include "./panel-keyboard.h"
//#include "./panel-game.h"


// This is populated with a signature after signing
__attribute__((used)) const char code_signature[] =
  "<FFX-SIGNATURE>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</FFX-SIGNATURE>";


// This is added from the CMakeLists.txt
#ifndef GIT_COMMIT
#define GIT_COMMIT ("unknown")
#endif


void taskAppFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;
    vTaskSetApplicationTaskTag(NULL, (void*) NULL);

    *ready = 1;

    // Start the App Process; this is started in the main task, so
    // has high-priority. Don't doddle.
    // @TODO: should we start a short-lived low-priority task to start this?
    //pushPanelSpace(NULL);
    //pushPanelNull(pushPanelSpace);
    //pushPanelKeyboard(NULL);
    //pushPanelGame(NULL);

    //uint32_t result = pushPanelTest(NULL);
    //uint32_t result = pushPanelConnect();
    uint32_t result = pushPanelMenu(NULL);
    printf("[main] Root Panel returned status %ld\n", result);

    while (1) { delay(10000); }
}

void taskPrimeFunc(void* pvParameter) {
    uint32_t *ready = (uint32_t*)pvParameter;
    vTaskSetApplicationTaskTag(NULL, (void*) NULL);

    {
        FfxEcPrivkey privkey0;
        device_testPrivateKey(&privkey0, 0);
    }

    *ready = 1;
/*
    uint8_t challenge[32] = { 0 };
    challenge[31] = 42;
    challenge[30] = 41;
    challenge[29] = 40;

    DeviceAttestation attest = { 0 };
    DeviceStatus status = device_attest(challenge, &attest);

    printf("ATTEST: v=%d model=%d, serial=%d\n", attest.version,
      (int)attest.modelNumber, (int)attest.serialNumber);
    dumpBuffer("  nonce:      ", attest.nonce, sizeof(attest.nonce));
    dumpBuffer("  challenge:  ", attest.challenge, sizeof(attest.challenge));
    dumpBuffer("  pukbkeyN:    ", attest.pubkeyN, sizeof(attest.pubkeyN));
    dumpBuffer("  attestProof: ", attest.attestProof,
      sizeof(attest.attestProof));
    dumpBuffer("  signature:   ", attest.signature, sizeof(attest.signature));
*/
    // Farewell...
    vTaskDelete(NULL);
}

void app_main() {
    printf("GIT Commit: %s\n", GIT_COMMIT);

    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    TaskHandle_t taskAppHandle = NULL;
    TaskHandle_t taskBleHandle = NULL;
    TaskHandle_t taskIoHandle = NULL;

    // Load NVS and eFuse provision data
    {
        DeviceStatus status = device_init();
        printf("[main] device initialized: status=%d serial=%ld model=%ld\n",
          status, device_serialNumber(), device_modelNumber());
    }

    // Initialie the events
    events_init();

    // Start the IO task (handles the display, LEDs and keypad) [priority: 5]
    {
        // Pointer passed to taskIoFunc to notify us when IO is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskIoFunc, "io",
          8 * 1024, &ready, PRIORITY_IO, &taskIoHandle, 0);
        assert(status && taskIoHandle != NULL);

        // Wait for the IO task to complete setup
        while (!ready) { delay(1); }
        printf("[main] IO task ready\n");
    }

    // Start the Message task (handles BLE messages) [priority: 6]
    {
        // Pointer passed to taskReplFunc to notify us when REPL is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskBleFunc, "ble",
          4 * 1024, &ready, PRIORITY_BLE, &taskBleHandle, 0);
        assert(status && taskBleHandle != NULL);

        // Wait for the REPL task to complete setup
        while (!ready) { delay(1); }
        printf("[main] BLE task ready\n");
    }

    // Start app process [priority: 3];
    {
        // Pointer passed to taskAppFunc to notify us when APP is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskAppFunc, "app",
          4 * 1024, &ready, PRIORITY_APP, &taskAppHandle, 0);
        assert(status && taskAppHandle != NULL);

        // Wait for the IO task to complete setup
        while (!ready) { delay(1); }
        printf("[main] APP ready\n");
    }

    // Start prime process [priority: 2];
    {
        TaskHandle_t taskPrimeHandle = NULL;

        // Pointer passed to taskAppFunc to notify us when APP is ready
        uint32_t ready = 0;

        BaseType_t status = xTaskCreatePinnedToCore(&taskPrimeFunc, "prime",
          8 * 1024, &ready, PRIORITY_PRIME, &taskPrimeHandle, 0);
        assert(status && taskPrimeHandle != NULL);

        // Wait for the prime task to complete setup
        while (!ready) { delay(1); }
        printf("[main] PRIME task done\n");
    }

    while (1) {
        printf("[main] ticks=%ld high-water: main=%d io=%d, ble=%d app=%d, freq=%ld\n",
            ticks(),
            uxTaskGetStackHighWaterMark(NULL),
            uxTaskGetStackHighWaterMark(taskIoHandle),
            uxTaskGetStackHighWaterMark(taskBleHandle),
            uxTaskGetStackHighWaterMark(taskAppHandle),
            portTICK_PERIOD_MS);
        delay(60000);
    }
}





