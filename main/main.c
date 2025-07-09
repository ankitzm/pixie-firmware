
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "firefly-demos.h"
#include "firefly-hollows.h"

#include "utils.h"

//#include "panel-connect.h"
#include "panel-menu.h"
//#include "panel-null.h"
#include "panel-space.h"
//#include "panel-test.h"
//#include "panel-tx.h"
//#include "./panel-keyboard.h"
//#include "./panel-game.h"


// This is populated with a signature after signing
//__attribute__((used)) const char code_signature[] =
//  "<FFX-SIGNATURE>xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx</FFX-SIGNATURE>";


// This is added from the CMakeLists.txt
#ifndef GIT_COMMIT
#define GIT_COMMIT ("unknown")
#endif


void app_main() {
    vTaskSetApplicationTaskTag( NULL, (void*)NULL);

    FFX_LOG("GIT Commit: %s\n", GIT_COMMIT);

    ffx_init(ffx_demo_backgroundPixies, NULL);
    //ffx_init(NULL, NULL);
    //ffx_demo_pushPanelTest(NULL);
    //pushPanelSpace();
    pushPanelMenu();

    while (1) {
        /*
        printf("[main] ticks=%ld high-water: main=%d io=%d, ble=%d app=%d, freq=%ld\n",
            ticks(),
            uxTaskGetStackHighWaterMark(NULL),
            uxTaskGetStackHighWaterMark(taskIoHandle),
            uxTaskGetStackHighWaterMark(taskBleHandle),
            uxTaskGetStackHighWaterMark(taskAppHandle),
            portTICK_PERIOD_MS);
        */
        printf("HERE\n");
        delay(60000);
    }
}





