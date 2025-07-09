#ifndef __PANEL_H__
#define __PANEL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>

#include "firefly-cbor.h"
#include "firefly-color.h"
#include "firefly-scene.h"

#include "events.h"


typedef enum PanelStyle {
    PanelStyleInstant = 0,
    PanelStyleCoverUp,
//    PanelStyleSlideOver,
    PanelStyleSlideLeft, // SlideFromRight?
//    PanelStyleCoverFromRight,
//    PanelStyleSlideSlideOut,
} PanelStyle;


///////////////////////////////
// Panel Managment API

typedef int (*PanelInitFunc)(FfxScene scene, FfxNode node, void* state,
  void* arg);

uint32_t panel_push(PanelInitFunc initFunc, size_t stateSize,
  PanelStyle style, void* arg);

// Deletes the current panel task, returning control to previous panel in stack
void panel_pop(uint32_t status);


///////////////////////////////
// Pixel API

typedef union FfxPixelParam {
    uint32_t u32;
    color_ffxt color;
    int32_t i32;
} FfxPixelParam;
/*
typedef enum FfxPixelStyle {
    // p1 = pixel, p2 = color
    FfxPixelStyleConstant = 0,

    // p1 = pixel, p2 = color, p3 = period
    FfxPixelStyleThrob,
    FfxPixelStyleBlink,
    FfxPixelStyleBlinkAlt,

    FfxPixelStyleDance,
    FfxPixelStyleDripDown,
    FfxPixelStyleDripUp,
} FfxPixelStyle;
*/
// void panel_setPixels(uint32_t pixelMask, FfxPixelStyle style, ...);

void panel_setPixel(uint32_t pixel, color_ffxt color);


///////////////////////////////
// Message API

bool panel_acceptMessage(uint32_t id, FfxCborCursor *params);
bool panel_sendErrorReply(uint32_t id, uint32_t code, char *message);
bool panel_sendReply(uint32_t id, FfxCborBuilder *result);

bool panel_isRadioConnected();
void panel_disconnect();

// @TODO: Remvoe this and automatically register messages
//        on message events
bool panel_isMessageEnabled();
void panel_enableMessage(bool enable);


///////////////////////////////
// Debugging

void panel_enableRemoteLog(bool enable);
bool pannel_isRemoteLog();

void panel_log(const char* message);
void panel_logFormat(const char* message, ...);
void panel_logData(uint8_t *data, size_t length);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_H__ */
