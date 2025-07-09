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

#define FONT_VALUE         (FfxFontMediumBold)
#define COLOR_VALUE        (COLOR_WHITE)

typedef struct State {
    FfxDataResult tx;
} State;


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

static bool initViewNetwork(void *infoState, State *state) {
    appendTitle(infoState, "NETWORK");

    FfxDataResult data = ffx_tx_getChainId(state->tx);
    if (data.error) {
        printf("ERROR! Bad Value\n");
        return false;
    }

    FfxBigInt value = ffx_bigint_initBytes(data.bytes, data.length);
    if (!ffx_bigint_cmpU32(&value, 1)) {
        appendEntry(infoState, "NAME", "Mainnet", PanelTxViewNoDrill);
    } else if (!ffx_bigint_cmpU32(&value, 11155111)) {
        appendEntry(infoState, "NAME", "Sepolia", PanelTxViewNoDrill);
    }

    char str[FFX_BIGINT_STRING_LENGTH];
    size_t length = ffx_bigint_getString(&value, str);
    appendEntry(infoState, "CHAIN ID", str, PanelTxViewNoDrill);

    appendHR(infoState);

    // Buttons
    appendButton(infoState, "BACK", COLOR_BLUE, PanelTxActionApprove);

    return true;
}

static bool initViewTo(void *infoState, State *state) {
    appendTitle(infoState, "TO");

    FfxDataResult data = ffx_tx_getAddress(state->tx);
    if (data.error) {
        printf("ERROR!! Bad Address\n");
        return false;
    }

    appendPadding(infoState, PADDING);

    if (data.length == 0) {
        appendText(infoState, "contract", FONT_VALUE, COLOR_VALUE);
        appendText(infoState, "deployment", FONT_VALUE, COLOR_VALUE);

    } else if (data.length == 20) {
        FFX_INIT_ADDRESS(addr, data.bytes);
        FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);

        char line[16] = { 0 };
        line[0] = '0';
        line[1] = 'x';

        memcpy(&line[2], &address.text[2], 10);
        appendText(infoState, line, FONT_VALUE, COLOR_VALUE);
        appendPadding(infoState, 3);

        line[0] = line[1] = ' ';
        memcpy(&line[2], &address.text[12], 10);
        appendText(infoState, line, FONT_VALUE, COLOR_VALUE);
        appendPadding(infoState, 3);

        memcpy(&line[2], &address.text[22], 10);
        appendText(infoState, line, FONT_VALUE, COLOR_VALUE);
        appendPadding(infoState, 3);

        memcpy(&line[2], &address.text[32], 10);
        appendText(infoState, line, FONT_VALUE, COLOR_VALUE);
        appendPadding(infoState, 3);

    } else {
        printf("ERROR!! Bad Address\n");
        return false;
    }

    appendPadding(infoState, PADDING);

    return true;
}

static bool initViewSummary(void *infoState, State *state) {
    appendTitle(infoState, "TRANSACTION");

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
            appendEntry(infoState, "NETWORK", name, PanelTxViewNetwork);
        } else {
            char str[FFX_BIGINT_STRING_LENGTH];
            size_t length = ffx_bigint_getString(&value, str);
            appendEntry(infoState, "CHAIN ID", str, PanelTxViewNetwork);
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
            appendEntry(infoState, "TO", "deploy", PanelTxViewTo);
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
            appendEntry(infoState, "TO", str, PanelTxViewTo);
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
        appendEntry(infoState, "VALUE (sETH)", result.str, PanelTxViewNoDrill);
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
            appendEntry(infoState, "DATA", "none", PanelTxViewNoDrill);

        } else if (data.length <= 5) {
            // Short data; will fit in a single entry
            char hex[20] = { 0 };
            getHex(hex, data.bytes, data.length);
            //appendEntry(state, "DATA", hex, PanelTxViewData);
            appendEntry(infoState, "DATA", hex, PanelTxViewNoDrill);

        } else {
            // Long data; show first 4 bytes
            char hex[20] = { 0 };
            size_t offset = getHex(hex, data.bytes, 4) - 1;
            hex[offset++] = '.';
            hex[offset++] = '.';
            hex[offset++] = '.';
            hex[offset++] = '\0';
            //appendEntry(state, "DATA", hex, PanelTxViewData);
            appendEntry(infoState, "DATA", hex, PanelTxViewNoDrill);
        }
    }

    appendHR(infoState);

    // Buttons
    appendButton(infoState, "REJECT", COLOR_RED, PanelTxActionReject);
    appendButton(infoState, "SIGN & SEND", COLOR_GREEN, PanelTxActionApprove);

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

static int _initFunc(void *infoState, void *_state, void *_arg) {
    State *state = _state;

    InitArg *init = _arg;
    state->tx = *(init->tx);
    printf("panel-tx: ");
    ffx_tx_dump(state->tx);;

    if (init->view == PanelTxViewSummary) {
        initViewSummary(infoState, state);
    } else if (init->view == PanelTxViewTo) {
        initViewTo(infoState, state);
    } else if (init->view == PanelTxViewNetwork) {
        initViewNetwork(infoState, state);
    } else {
        printf("Not supported yet: %d\n", init->view);
        assert(0);
    }

    return 0;
}

void _selectFunc(void *_state, uint16_t userData) {

    if (userData == PanelTxActionReject) {
        panel_pop(PANEL_TX_REJECT);
    } else if (userData == PanelTxActionApprove) {
        panel_pop(PANEL_TX_APPROVE);
    } else if (userData == PanelTxViewNoDrill) {
        // Do nothing; cannot drill down
    } else if (userData) {
        State *state = _state;

        if (userData == PanelTxViewSummary || userData == PanelTxViewTo ||
          userData == PanelTxViewNetwork) {
            pushPanelTx(&state->tx, userData);
        }
    }
}

uint32_t pushPanelTx(FfxDataResult *tx, PanelTxView view) {
    InitArg init = { .tx = tx, .view = view };
    return pushPanelInfo(_initFunc, sizeof(State), _selectFunc, &init);
}

