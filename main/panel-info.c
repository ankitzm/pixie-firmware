#include <stdio.h>

#include "firefly-color.h"
#include "firefly-scene.h"

#include "panel.h"
#include "panel-info.h"


#define WIDTH     (200)

#define  TAG_BASE       (10)

#define DURATION_HIGHLIGHT    (300)

#define COLOR_ENTRY           (0x00444488)


typedef struct State {
    FfxScene scene;
    FfxNode panel;

    FfxNode info;      // container of all fields
    FfxNode box;       // background of info
    int offset;

    int lastHR;

    int nextIndex;

    FfxNode active;
    int index;

    InfoSelectFunc selectFunc;

    // Child state goes here
} State;


typedef struct Button {
    int16_t userData;
} Button;

static FfxNode appendHighlight(State *state, color_ffxt color,
  int16_t userData) {

    int index = state->nextIndex++;

    FfxNode glow = ffx_scene_createBox(state->scene, ffx_size(180, 25));
    ffx_sceneBox_setColor(glow, ffx_color_setOpacity(color, 0));

    FfxNode anchor = ffx_scene_createAnchor(state->scene, TAG_BASE + index,
      sizeof(Button), glow);
    ffx_sceneNode_setPosition(anchor, ffx_point(10, state->offset));
    Button *button = ffx_sceneAnchor_getData(anchor);
    button->userData = userData;
    ffx_sceneGroup_appendChild(state->info, anchor);

    if (state->active == NULL && userData) {
        ffx_sceneBox_setColor(glow, ffx_color_setOpacity(color, OPACITY_80));
        state->active = anchor;
        state->index = index;
    }

    return glow;
}

static bool highlight(State *state, uint32_t index) {
    FfxNode target = ffx_sceneNode_findAnchor(state->panel, TAG_BASE + index);

    if (target == NULL) { return false; }

    Button *button = ffx_sceneAnchor_getData(target);

    state->index = index;

    // Turn off existing highlight @TODO: use node_find
    if (state->active) {
        FfxNode glow = ffx_sceneAnchor_getChild(state->active);
        color_ffxt color = ffx_sceneBox_getColor(glow);
        ffx_sceneNode_stopAnimations(glow, false);
        ffx_sceneBox_animateColor(glow,
          ffx_color_setOpacity(color, 0), 0, 300, FfxCurveLinear, NULL, NULL);
    }

    // Turn on highlight
    state->active = target;

    FfxNode glow = ffx_sceneAnchor_getChild(state->active);
    color_ffxt color = ffx_sceneBox_getColor(glow);
    ffx_sceneNode_stopAnimations(glow, false);

    if (button->userData) {
        ffx_sceneBox_animateColor(glow, ffx_color_setOpacity(color, OPACITY_80),
          0, 300, FfxCurveLinear, NULL, NULL);
    }

    // Scroll highlight onto screen

    if (ffx_sceneBox_getSize(state->box).height <= 200) {
        // Infobox is too small to scroll
        return true;
    };

    FfxPoint pos = ffx_sceneNode_getPosition(state->info);
    int32_t y = ffx_sceneNode_getPosition(target).y;
    int32_t h = ffx_sceneBox_getSize(glow).height;

    if (index == 0) {
        ffx_sceneNode_stopAnimations(state->info, false);
        pos.y = 20;
        ffx_sceneNode_animatePosition(state->info, pos, 0, 300,
          FfxCurveEaseOutQuad, NULL, NULL);

    } else if (index + 1 == state->nextIndex) {
        int32_t H = ffx_sceneBox_getSize(state->box).height;
        ffx_sceneNode_stopAnimations(state->info, false);
        pos.y = -(H + 20 - 240);
        ffx_sceneNode_animatePosition(state->info, pos, 0, 300,
          FfxCurveEaseOutQuad, NULL, NULL);

    } else if (y + h + pos.y > 200) {
        ffx_sceneNode_stopAnimations(state->info, false);
        pos.y = -(y + h - 200);
        ffx_sceneNode_animatePosition(state->info, pos, 0, 300,
          FfxCurveEaseOutQuad, NULL, NULL);

    } else if (y + pos.y < 40) {
        ffx_sceneNode_stopAnimations(state->info, false);
        pos.y = -(y - 40);
        ffx_sceneNode_animatePosition(state->info, pos, 0, 300,
          FfxCurveEaseOutQuad, NULL, NULL);
    }

    return true;
}

static bool selectHighlight(State *state) {
    int index = state->index;
    if (index == -1) { return false; }

    FfxNode node = ffx_sceneNode_findAnchor(state->panel, TAG_BASE + index);
    if (node == NULL) { return false; }

    Button *button = ffx_sceneAnchor_getData(node);
    state->selectFunc(&state[1], button->userData);

    return true;
}


void appendPadding(void *infoState, size_t size) {
    State *state = infoState;
    state->offset += size;
}

#define FONT_TITLE         (FfxFontSmallBold)
#define COLOR_TITLE        (ffx_color_rgb(120, 120, 120))

#define FONT_HEADING       (FfxFontSmall)
#define COLOR_HEADING      (ffx_color_rgb(200, 200, 200))

#define FONT_VALUE         (FfxFontMediumBold)
#define COLOR_VALUE        (COLOR_WHITE)

void appendText(void *infoState, const char* text, FfxFont font,
  color_ffxt color) {

    State *state = infoState;

    FfxFontMetrics metrics = ffx_scene_getFontMetrics(font);

    FfxNode label = ffx_scene_createLabel(state->scene, font, text);
    ffx_sceneGroup_appendChild(state->info, label);
    ffx_sceneLabel_setAlign(label, FfxTextAlignTop | FfxTextAlignCenter);
    ffx_sceneLabel_setTextColor(label, color);
    ffx_sceneNode_setPosition(label, ffx_point(WIDTH / 2, state->offset));

    state->offset += metrics.size.height -  metrics.descent;
}

void appendHR(void *_state) {
    State *state = _state;

    // Don't add an HR if we already have one
    if (state->lastHR + 1 == state->offset) { return; }
    state->lastHR = state->offset;

    FfxNode hr = ffx_scene_createBox(state->scene, ffx_size(WIDTH - 30, 1));
    ffx_sceneBox_setColor(hr, ffx_color_rgb(92, 168, 199));
    ffx_sceneNode_setPosition(hr, ffx_point(15, state->offset));
    ffx_sceneGroup_appendChild(state->info, hr);
    state->offset += 1;
}

static void adjust(State *state) {
    FfxSize size = ffx_sceneBox_getSize(state->box);
    size.height = state->offset + PADDING;
    ffx_sceneBox_setSize(state->box, size);
}

void appendTitle(void *_state, const char* title) {
    State *state = _state;

    state->offset += PADDING;
    appendText(state, title, FONT_TITLE, COLOR_TITLE);
    state->offset += PADDING;

    appendHR(_state);

}

void appendEntry(void *_state, const char* heading, const char* value,
  uint16_t userData) {

    State *state = _state;

    appendHR(state);

    state->offset += 3;
    size_t top = state->offset;
    FfxNode on = appendHighlight(state, COLOR_ENTRY, userData);

    state->offset += 10;
    appendText(state, heading, FONT_HEADING, COLOR_HEADING);

    state->offset += 8;
    appendText(state, value, FONT_VALUE, COLOR_VALUE);

    state->offset += 12;

    // Resize the highlight to fill the entry
    FfxSize boxSize = ffx_size(180, state->offset - top - 1);
    ffx_sceneBox_setSize(on, boxSize);

    if (userData && userData != PanelTxViewNoDrill) {
        FfxNode caret = ffx_scene_createLabel(state->scene, FfxFontLargeBold, ">");
        ffx_sceneLabel_setTextColor(caret, 0x007777aa);
        ffx_sceneLabel_setOutlineColor(caret, COLOR_BLACK);
        ffx_sceneNode_setPosition(caret,
          ffx_point(188, top + (boxSize.height / 2) - 10));
        ffx_sceneGroup_appendChild(state->info, caret);
    }

    state->offset += 3;

    appendHR(state);
}

void appendButton(void *_state, const char* label, color_ffxt color,
  uint16_t userData) {

    State *state = _state;

    state->offset += 3;
    size_t top = state->offset;
    FfxNode on = appendHighlight(state, color, userData);
    state->offset += PADDING - 6;

    appendText(state, label, FONT_VALUE, COLOR_VALUE);

    state->offset += PADDING - 3;
    ffx_sceneBox_setSize(on, ffx_size(180, state->offset - top - 1));
    state->offset += 3;
}

static void keyUp(EventPayload event, void *_state) {
    State *state = _state;

    switch ((~event.props.keys.down) & event.props.keys.changed) {
        case KeySouth:
            if (state->index + 1 < state->nextIndex) {
                highlight(state, state->index + 1);
            }
            break;

        case KeyNorth:
            if (state->index > 0) {
                highlight(state, state->index - 1);
            }
            break;

        case KeyOk:
            selectHighlight(state);
            break;

        case KeyCancel:
            panel_pop(PANEL_TX_REJECT);
            break;
    }
}

typedef struct InitArg {
    InfoInitFunc initFunc;
    InfoSelectFunc selectFunc;
    void *arg;
} InitArg;

static int _init(FfxScene scene, FfxNode panel, void* _state, void* _arg) {
    State *state = _state;
    state->scene = scene;
    state->panel = panel;

    // Clear selection
    state->index = -1;
    state->lastHR = -1;

    FfxNode info = ffx_scene_createGroup(scene);
    state->info = info;
    ffx_sceneGroup_appendChild(panel, info);
    ffx_sceneNode_setPosition(info, ffx_point(20, 20));

    FfxNode box = ffx_scene_createBox(scene, ffx_size(WIDTH, 400));
    state->box = box;
    ffx_sceneGroup_appendChild(info, box);

    InitArg *init = _arg;
    state->selectFunc = init->selectFunc;

    init->initFunc(state, &state[1], init->arg);

    appendHR(state);

    adjust(state);

    panel_onEvent(EventNameKeysUp | KeyAll, keyUp, state);

    return 0;
}

uint32_t pushPanelInfo(InfoInitFunc initFunc, size_t stateSize,
  InfoSelectFunc selectFunc, void *arg) {

    InitArg init = {
        .initFunc = initFunc,
        .selectFunc = selectFunc,
        .arg = arg
    };

    return panel_push(_init, sizeof(State) + stateSize, PanelStyleSlideLeft,
      &init);
}
