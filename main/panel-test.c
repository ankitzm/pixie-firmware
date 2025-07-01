
#include <stdio.h>

#include "firefly-scene.h"

#include "panel.h"


typedef struct State {
    FfxScene scene;
    FfxNode node;
} State;


static void keyChanged(EventPayload event, void *_app) {
    State *app = _app;

    switch(event.props.keys.down) {
        case KeyOk:
            printf("OK\n");
            break;
        case KeyCancel:
            printf("Cancel\n");
            break;
        case KeyNorth:
            printf("North\n");
            break;
        case KeySouth:
            printf("South\n");
            break;
        default:
            return;
    }
}

static int init(FfxScene scene, FfxNode node, void *_app, void *arg) {
    State *app = _app;
    app->scene = scene;
    app->node = node;

    FfxNode box = ffx_scene_createBox(scene, ffx_size(200, 180));
    ffx_sceneBox_setColor(box, RGBA_DARKER75);
    ffx_sceneGroup_appendChild(node, box);
    ffx_sceneNode_setPosition(box, (FfxPoint){ .x = 20, .y = 30 });

    FfxNode qr = ffx_scene_createQR(scene, "HTTPS://WWW.RICMOO.COM",
      FfxQRCorrectionLow);
    ffx_sceneGroup_appendChild(node, qr);
    ffx_sceneNode_setPosition(qr, ffx_point(50, 50));
    ffx_sceneQR_setModuleSize(qr, 4);

    panel_onEvent(EventNameKeysChanged | KeyAll, keyChanged, app);

    return 0;
}

uint32_t pushPanelTest(void *arg) {
    return panel_push(init, sizeof(State), PanelStyleCoverUp, arg);
}
