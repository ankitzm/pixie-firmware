#include <stdlib.h>
#include <string.h>

#include "firefly-address.h"
#include "firefly-db.h"
#include "firefly-decimal.h"
#include "firefly-scene.h"
#include "firefly-tx.h"

#include "panel.h"
#include "panel-tx.h"

#include "utils.h"

// DEBUG
void dumpBuffer(const char*, const uint8_t*, size_t);
#include <stdio.h>

// The distance from the top and bottom to make sure a highlighted element
// is within (unless at the end of the scrollable section)
#define MARGIN    (80)

// The width of the info box
#define WIDTH     (200)


typedef struct State {
    FfxScene scene;
    FfxNode panel;

    FfxNode info;      // container of all fields
    FfxNode box;       // background of info
    int offset;

    int nextIndex;

    FfxNode active;
    int index;

    FfxDataResult tx;
} State;

typedef enum Format {
    FormatNone    = 0,
    FormatTitle,
    FormatHeading,
    FormatValue,
    FormatAddress
} Format;

// @TODO: Separate state and UI build logic
typedef struct MenuBuilder {
    int nextIndex;
} MenuBuilder;


#define  TAG_BASE       (10)

#define DURATION_HIGHLIGHT    (300)

#define COLOR_ENTRY           (0x00444488)

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

    FfxNode target = ffx_scene_findAnchor(state->scene, TAG_BASE + index);
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

    FfxNode node = ffx_scene_findAnchor(state->scene, TAG_BASE + index);
    if (node == NULL) { return false; }

    Button *button = ffx_sceneAnchor_getData(node);
    if (button->userData == PanelTxActionReject) {
        panel_pop(PANEL_TX_REJECT);
    } else if (button->userData == PanelTxActionApprove) {
        panel_pop(PANEL_TX_APPROVE);
    } else if (button->userData == PanelTxViewNoDrill) {
        // Do nothing; cannot drill down
    } else if (button->userData) {
        if (button->userData == PanelTxViewSummary ||
          button->userData == PanelTxViewTo ||
          button->userData == PanelTxViewNetwork) {
            pushPanelTx(&state->tx, button->userData);
        }
    }

    return true;
}

#define PADDING         (15)

static void appendText(State *state, const char* text, Format format) {
    FfxTextAlign align = FfxTextAlignTop | FfxTextAlignCenter;
    FfxFont font = FfxFontLargeBold;

    color_ffxt color = COLOR_WHITE;

    switch (format) {
        case FormatTitle:
            font = FfxFontSmallBold;
            color = ffx_color_rgb(120, 120, 120);
            break;
        case FormatHeading:
            font = FfxFontSmall;
            color = ffx_color_rgb(200, 200, 200);
            break;
        case FormatValue:
            font = FfxFontMediumBold;
            break;
        default:
            break;
    }

    FfxFontMetrics metrics = ffx_scene_getFontMetrics(font);

    FfxNode label = ffx_scene_createLabel(state->scene, font, text);
    ffx_sceneGroup_appendChild(state->info, label);
    ffx_sceneLabel_setAlign(label, align);
    ffx_sceneLabel_setTextColor(label, color);
    ffx_sceneNode_setPosition(label, ffx_point(WIDTH / 2, state->offset));

    state->offset += metrics.size.height -  metrics.descent;
}

static void appendHR(State *state) {
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

static void appendTitle(State *state, const char* title) {
    state->offset += PADDING;
    appendText(state, title, FormatTitle);
    state->offset += PADDING;
    adjust(state);
}

static void appendEntry(State *state, const char* heading, const char* value,
  uint16_t userData) {

    appendHR(state);

    state->offset += 3;
    size_t top = state->offset;
    FfxNode on = appendHighlight(state, COLOR_ENTRY, userData);
    state->offset += PADDING - 3;

    appendText(state, heading, FormatHeading);
    state->offset += 5;
    appendText(state, value, FormatValue);
    state->offset += PADDING - 3;
    FfxSize boxSize = ffx_size(180, state->offset - top - 1);
    ffx_sceneBox_setSize(on, boxSize);
    //FfxPoint boxPos = ffx_sceneNode_getPosition(on);

    if (userData && userData != PanelTxViewNoDrill) {
        FfxNode caret = ffx_scene_createLabel(state->scene, FfxFontLargeBold, ">");
        ffx_sceneLabel_setTextColor(caret, 0x007777aa);
        ffx_sceneLabel_setOutlineColor(caret, COLOR_BLACK);
        ffx_sceneNode_setPosition(caret,
          ffx_point(188, top + (boxSize.height / 2) - 10));
        ffx_sceneGroup_appendChild(state->info, caret);
    }

    state->offset += 3;


    adjust(state);
}

static void appendButton(State *state, const char* label, color_ffxt color,
  uint16_t userData) {

    state->offset += 3;
    size_t top = state->offset;
    FfxNode on = appendHighlight(state, color, userData);
    state->offset += PADDING - 6;

    appendText(state, label, FormatValue);

    state->offset += PADDING - 3;
    ffx_sceneBox_setSize(on, ffx_size(180, state->offset - top - 1));
    state->offset += 3;

    adjust(state);
}

static void keyChanged(EventPayload event, void *_state) {
    State *state = _state;

    switch (event.props.keys.down) {
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

const char* HEX = "0123456789abcdef";
static size_t getHex(char *hexOut, const uint8_t* data, size_t length) {
    size_t offset = 0;
    hexOut[offset++] = '0';
    hexOut[offset++] = 'x';
    for (int i = 0; i < length; i++) {
        hexOut[offset++] = HEX[data[i] >> 4];
        hexOut[offset++] = HEX[data[i] & 0x0f];
    }
    hexOut[offset++] = 0;
    return offset;
}

static bool initViewNetwork(State *state) {
    appendTitle(state, "NETWORK");

    FfxDataResult data = ffx_tx_getChainId(state->tx);
    if (data.error) {
        printf("ERROR! Bad Value\n");
        return false;
    }

    FfxBigInt value = ffx_bigint_initBytes(data.bytes, data.length);
    if (!ffx_bigint_cmpU32(&value, 1)) {
        appendEntry(state, "NAME", "Mainnet", 0);
    } else if (!ffx_bigint_cmpU32(&value, 11155111)) {
        appendEntry(state, "NAME", "Sepolia", 0);
    }

    char str[FFX_BIGINT_STRING_LENGTH];
    size_t length = ffx_bigint_getString(&value, str);
    appendEntry(state, "CHAIN ID", str, 0);

    appendHR(state);

    adjust(state);

    return true;
}

static bool initViewTo(State *state) {
    appendTitle(state, "TO");

    appendHR(state);

    FfxDataResult data = ffx_tx_getAddress(state->tx);
    if (data.error) {
        printf("ERROR!! Bad Address\n");
        return false;
    }

    state->offset += PADDING;
    if (data.length == 0) {
        state->offset += PADDING;
        appendText(state, "contract", FormatValue);
        appendText(state, "deployment", FormatValue);

    } else if (data.length == 20) {
        FFX_INIT_ADDRESS(addr, data.bytes);
        FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);

        char line[16] = { 0 };
        line[0] = '0';
        line[1] = 'x';

        memcpy(&line[2], &address.text[2], 10);
        appendText(state, line, FormatValue);
        state->offset += 3;

        line[0] = line[1] = ' ';
        memcpy(&line[2], &address.text[12], 10);
        appendText(state, line, FormatValue);
        state->offset += 3;

        memcpy(&line[2], &address.text[22], 10);
        appendText(state, line, FormatValue);
        state->offset += 3;

        memcpy(&line[2], &address.text[32], 10);
        appendText(state, line, FormatValue);
        state->offset += 3;

        //memcpy(&line[2], &address.text[34], 8);
        //appendText(state, line, FormatValue);
        //state->offset += 3;

    } else {
        printf("ERROR!! Bad Address\n");
        return false;
    }

    state->offset += PADDING;

    appendHR(state);

    adjust(state);

    return true;
}

static bool initViewSummary(State *state) {
    appendTitle(state, "TRANSACTION");

    // Network
    {
        FfxDataResult data = ffx_tx_getChainId(state->tx);
        if (data.error) {
            printf("ERROR! Bad Value\n");
            return false;
        }

        FfxBigInt value = ffx_bigint_initBytes(data.bytes, data.length);
        const char *name = ffx_db_getNetworkName(&value);
        if (name) {
            appendEntry(state, "NETWORK", name, PanelTxViewNetwork);
        } else {
            char str[FFX_BIGINT_STRING_LENGTH];
            size_t length = ffx_bigint_getString(&value, str);
            appendEntry(state, "CHAIN ID", str, PanelTxViewNetwork);
        }
    }

    // To Address
    {
        FfxDataResult data = ffx_tx_getAddress(state->tx);
        if (data.error) {
            printf("ERROR!! Bad Address\n");
            return false;
        }

        if (data.length == 0) {
            appendEntry(state, "TO", "deploy", PanelTxViewTo);
        } else if (data.length == 20) {
            FFX_INIT_ADDRESS(addr, data.bytes);
            FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);

            // "0x01234...5678\0"
            char str[14];
            memcpy(&str[0], &address.text[0], 6);
            str[6] = '.';
            str[7] = '.';
            str[8] = '.';
            memcpy(&str[9], &address.text[38], 5);
            appendEntry(state, "TO", str, PanelTxViewTo);
        } else {
            printf("ERROR!! Bad Address\n");
            return false;
        }
    }

    // Value
    {
        FfxDataResult data = ffx_tx_getValue(state->tx);
        if (data.error) {
            printf("ERROR! Bad Value\n");
            return false;
        }

        FfxBigInt value = ffx_bigint_initBytes(data.bytes, data.length);

        // Reserve a space to indicate rounding occurred
        char str[1 + FFX_ETHER_STRING_LENGTH] = { 0 };
        FfxDecimalResult result = ffx_decimal_formatValue(&str[1], &value, (FfxDecimalFormat){
            .round = FfxDecimalRoundCeiling,
            .decimals = 18,
            .groups = 3,
            .maxDecimals = 5,
            .minDecimals = 1,
        });

        if (result.flags & FfxDecimalFlagRounded) {
            str[0] = '<';
            result.str = str;
        }

        // @TODO: Drill down into value
        //appendEntry(state, "VALUE (sETH)", result.str, PanelTxViewValue);
        appendEntry(state, "VALUE (sETH)", result.str, PanelTxViewNoDrill);
    }

    // Data
    {
        FfxDataResult data = ffx_tx_getData(state->tx);
        if (data.error) {
            printf("ERROR! Bad Data\n");
            return false;
        }

        // @TODO: Drill down into data; offer suggestions for selector

        if (data.length == 0) {
            // No data
            appendEntry(state, "DATA", "none", PanelTxViewNoDrill);

        } else if (data.length <= 5) {
            // Short data; will fit in a single entry
            char hex[20] = { 0 };
            getHex(hex, data.bytes, data.length);
            //appendEntry(state, "DATA", hex, PanelTxViewData);
            appendEntry(state, "DATA", hex, PanelTxViewNoDrill);

        } else {
            // Long data; show first 4 bytes
            char hex[20] = { 0 };
            size_t offset = getHex(hex, data.bytes, 4) - 1;
            hex[offset++] = '.';
            hex[offset++] = '.';
            hex[offset++] = '.';
            hex[offset++] = '\0';
            //appendEntry(state, "DATA", hex, PanelTxViewData);
            appendEntry(state, "DATA", hex, PanelTxViewNoDrill);
        }
    }

    appendHR(state);

    // Buttons
    appendButton(state, "REJECT", COLOR_RED, PanelTxActionReject);
    appendButton(state, "SIGN & SEND", COLOR_GREEN, PanelTxActionApprove);

    appendHR(state);

/*
    appendField(state, "FEES", FormatHeading);
    appendField(state, "< $0.10", FormatValue);
*/
    return true;
}

typedef struct InitArg {
    FfxDataResult *tx;
    PanelTxView view;
} InitArg;

static int _init(FfxScene scene, FfxNode panel, void* _state, void* _arg) {
    State *state = _state;
    state->scene = scene;
    state->panel = panel;

    // Clear selection
    state->index = -1;

    InitArg *init = _arg;
    state->tx = *(init->tx);
    printf("panel-tx: ");
    ffx_tx_dump(state->tx);;

    /*
    // DEBUG
    //const char data[] = "7228bd2c766b6640250f4a2bc7909d7dad3bc5b87efc421c3f6db58ebe7ef993a4617601666d6574686f64736666785f7369676e5472616e73616374696f6e6269640266706172616d73a967636861696e496443aa36a7686761734c696d697442520864747970654102656e6f6e6365410b6c6d6178466565506572476173432a5b6a746d61785072696f72697479466565506572476173430f42406576616c7565410162746f548ba1f109551bd432803012645ac136ddd64dba72646461746140";
    const char data[] = "a967636861696e496443aa36a7686761734c696d697442520864747970654102656e6f6e636541036c6d6178466565506572476173450d1fc40f6e746d61785072696f72697479466565506572476173430f42406576616c7565410162746f548ba1f109551bd432803012645ac136ddd64dba72646461746140";
    size_t dataLength = sizeof(data) >> 1;
    uint8_t *cbor = malloc(dataLength);
    size_t length = readBuffer(data, cbor, dataLength);
    dumpBuffer("CBOR", cbor, dataLength);
    ffx_cbor_dump(ffx_cbor_walk(cbor, dataLength));
    state->tx = ffx_tx_serializeUnsigned(ffx_cbor_walk(cbor, dataLength),
      state->data, state->length);
    ffx_tx_dump(state->tx);;
    */

    FfxNode info = ffx_scene_createGroup(scene);
    state->info = info;
    ffx_sceneGroup_appendChild(panel, info);
    ffx_sceneNode_setPosition(info, ffx_point(20, 20));

    FfxNode box = ffx_scene_createBox(scene, ffx_size(WIDTH, 400));
    state->box = box;
    ffx_sceneGroup_appendChild(info, box);

    if (init->view == PanelTxViewSummary) {
        initViewSummary(state);
    } else if (init->view == PanelTxViewTo) {
        initViewTo(state);
    } else if (init->view == PanelTxViewNetwork) {
        initViewNetwork(state);
    } else {
        printf("Not supported yet: %d\n", init->view);
        assert(0);
    }

    panel_onEvent(EventNameKeysChanged | KeyAll, keyChanged, state);

    return 0;
}

uint32_t pushPanelTx(FfxDataResult *tx, PanelTxView view) {
    InitArg init = { .tx = tx, .view = view };
    return panel_push(_init, sizeof(State), PanelStyleSlideLeft, &init);
}
