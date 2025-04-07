#include "panel.h"
#include "panel-null.h"


typedef struct State {
    NullInit init;
} State;

static void focus(EventPayload event, void *_state) {
    State *state = _state;
    if (state->init == NULL) { return; }

    state->init();
    state->init = NULL;
}

static int _init(FfxScene scene, FfxNode panel, void* panelState, void* arg) {
    State *state = panelState;
    state->init = arg;

    // When we focus we will push that panel
    panel_onEvent(EventNamePanelFocus, focus, state);

    return 0;
}

void pushPanelNull(NullInit init) {
    panel_push(_init, sizeof(State), PanelStyleInstant, init);
}
