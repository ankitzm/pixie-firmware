#ifndef __EVENTS_H__
#define __EVENTS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>

#include "firefly-cbor.h"


typedef uint16_t Keys;

typedef enum Key {
    KeyNone          = 0,
    KeyNorth         = (1 << 0),
    KeyEast          = (1 << 1),
    KeySouth         = (1 << 2),
    KeyWest          = (1 << 3),
    KeyOk            = (1 << 4),
    KeyCancel        = (1 << 5),
    KeyStart         = (1 << 6),
    KeySelect        = (1 << 7),
    KeyAll           = (0xff),
} Key;

#define KeyAll       (KeyCancel | KeyOk | KeyNorth | KeySouth)
#define KeyReset     (KeyCancel | KeyNorth)

// [ category: 4 bits ] [ specific: 4 bits ] [ filter info: 24 bits ]
typedef enum EventName {
    // Render Scene (N.B. filter by equality, bottom bits unused)
    EventNameRenderScene       = ((0x01) << 24),

    // Connect events; (N.B. filter by equality)
    EventNameRadioState        = ((0x02) << 24),

    // Keypad events; info=keys (N.B. filter by & EventNameKeys)
    EventNameKeysDown          = ((0x11) << 24),
    EventNameKeysUp            = ((0x12) << 24),
//    EventNameKeysPress       = ((0x13) << 24),  // @TODO: type repeat-rate
    EventNameKeysChanged       = ((0x14) << 24),
//    EventNameKeysCancelled   = ((0x15) << 24),
    EventNameKeys              = ((0x10) << 24),

    // Panel events; (N.B. filter by & EventNamePanel)
    EventNamePanelFocus        = ((0x21) << 24),
    //EventNamePanelBlur       = ((0x22) << 24),
    EventNamePanel             = ((0x20) << 24),

    // Message events (N.B. filter by equality, bottom bits unused)
    EventNameMessage           = ((0x42) << 24),

    // Custom event; (N.B. filter by & EventNameCustom)
    EventNameCustom            = ((0x80) << 24),

    // Mask to isolate the event type
    EventNameMask              = ((0xff) << 24),

    // Mask to isolate the event category
    EventNameCategoryMask      = ((0xf0) << 24),
} EventName;


typedef struct EventRenderSceneProps {
    uint32_t ticks;
} EventRenderSceneProps;

typedef enum EventKeysFlags {
    EventKeysFlagsNone        = 0,
    EventKeysFlagsCancelled   = (1 << 0),
} EventKeysFlags;

typedef struct EventKeysProps {
    Keys down;
    Keys changed;
    uint16_t flags;
} EventKeysProps;

typedef struct EventPanelProps {
    int id;
} EventPanelProps;

//typedef struct EventIncomingMessageProps {
//    size_t complete;
//    size_t length;
//} EventIncomingMessageProps;

typedef struct EventMessageProps {
    uint32_t id;
    const char* method;
    FfxCborCursor params;
} EventMessageProps;

typedef enum EventRadioState {
    EventRadioStateConnect = 1,
    EventRadioStateDisconnect = 2,
} EventRadioState;

typedef struct EventRadioProps {
    int id;
    EventRadioState state;
} EventRadioProps;

typedef struct EventCustomProps {
    uint8_t data[32];
} EventCustomProps;

typedef union EventPayloadProps {
    EventRenderSceneProps render;
    EventKeysProps keys;
    EventPanelProps panel;
//    EventIncomingMessageProps incoming;
    EventMessageProps message;
    EventRadioProps radio;
    EventCustomProps custom;
} EventPayloadProps;

typedef struct EventPayload {
    EventName event;
    int eventId;
    EventPayloadProps props;
} EventPayload;

typedef void (*EventCallback)(EventPayload event, void* arg);

bool panel_emitEvent(EventName eventName, EventPayloadProps props);
int panel_onEvent(EventName event, EventCallback cb, void* arg);
void panel_offEvent(int eventId);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVENTS_H__ */
