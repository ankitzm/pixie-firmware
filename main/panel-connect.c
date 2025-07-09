#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdio.h>

#include "firefly-address.h"
#include "firefly-cbor.h"
#include "firefly-ecc.h"
#include "firefly-hash.h"
#include "firefly-hollows.h"
#include "firefly-tx.h"

#include "panel-connect.h"
#include "panel-tx.h"

#include "utils.h"


#define ACCOUNT_INDEX        (0)


typedef struct State {
    FfxScene scene;
    FfxNode panel;

    FfxNode screenConnect;
    FfxNode screenDisconnect;

    uint32_t messageId;
} State;


static void replyAccounts(uint32_t messageId) {

    uint32_t t0 = ticks();

    FfxEcPrivkey privkey;
    assert(ffx_deviceTestPrivkey(&privkey, ACCOUNT_INDEX));

    FfxEcPubkey pubkey;
    ffx_ec_computePubkey(&pubkey, &privkey);

    memset(privkey.data, 0, sizeof(privkey.data));

    FfxAddress address = ffx_eth_getAddress(&pubkey);

    printf("getAccounts: dt=%ld\n", ticks() - t0);

    uint8_t replyBuffer[128] = { 0 };
    FfxCborBuilder reply = ffx_cbor_build(replyBuffer, sizeof(replyBuffer));

    ffx_cbor_appendArray(&reply, 1);
    ffx_cbor_appendData(&reply, address.data, sizeof(address.data));

    ffx_sendReply(messageId, &reply);
}

static void replySignTransaction(uint32_t messageId, FfxDataResult tx) {
    printf("Sending: @TODO %ld\n", messageId);
    ffx_tx_dump(tx);

    // Compute the transaction hash to sign
    FfxEcDigest digest;
    //uint8_t digest[FFX_KECCAK256_DIGEST_LENGTH] = { 0 };
    ffx_hash_keccak256(digest.data, tx.bytes, tx.length);

    // Get the private key
    FfxEcPrivkey privkey;
    assert(ffx_deviceTestPrivkey(&privkey, ACCOUNT_INDEX));

    // Sign the transaction
    FfxEcSignature sig;
    int32_t status = ffx_ec_signDigest(&sig, &privkey, &digest);
    printf("sig: status=%ld\n", status);

    memset(privkey.data, 0, sizeof(privkey.data));

    uint8_t replyBuffer[128];
    FfxCborBuilder reply = ffx_cbor_build(replyBuffer, sizeof(replyBuffer));

    ffx_cbor_appendMap(&reply, 3);
    ffx_cbor_appendString(&reply, "r");
    ffx_cbor_appendData(&reply, &sig.data[0], 32);
    ffx_cbor_appendString(&reply, "s");
    ffx_cbor_appendData(&reply, &sig.data[32], 32);
    ffx_cbor_appendString(&reply, "v");
    ffx_cbor_appendNumber(&reply, sig.data[64]);

    ffx_sendReply(messageId, &reply);
}

static void showDisconnect(State *state, bool animated) {
    ffx_sceneNode_setHidden(state->screenConnect, true);
    ffx_sceneNode_setHidden(state->screenDisconnect, false);
}

static void showConnect(State *state, bool animated) {
    ffx_sceneNode_setHidden(state->screenConnect, false);
    ffx_sceneNode_setHidden(state->screenDisconnect, true);
}


static void onRadio(FfxEvent event, FfxEventProps props, void* arg) {
    State *state = arg;
    if (props.radio.connected) {
        showConnect(state, true);
    } else {
        showDisconnect(state, true);
    }
}

static void onKeys(FfxEvent event, FfxEventProps props, void *_app) {
    //State *app = _app;
    if (props.keys.down & FfxKeyCancel) {
        ffx_disconnect();
        ffx_popPanel(0);
    }
}

static void onMessage(FfxEvent event, FfxEventProps props, void* arg) {
/*
    uint8_t _privateKey[32] = { 0 };
    uint8_t* privateKey = device_testPrivateKey(_privateKey, ACCOUNT_INDEX);
    assert(privateKey != NULL);
*/

    uint32_t messageId = props.message.id;
    const char* method = props.message.method;
    FfxCborCursor params = *props.message.params;
    printf("GOT MESSAGE: id=%ld, method=%s cbor=", messageId, method);
    ffx_cbor_dump(params);

    if (strcmp(method, "ffx_accounts") == 0) {
        replyAccounts(messageId);

    } else if (strcmp(method, "ffx_signTransaction") == 0) {
        size_t txBufferSize = 16 * 1024;
        uint8_t *txBuffer = malloc(txBufferSize);
        assert(txBuffer);

        FfxDataResult tx = ffx_tx_serializeUnsigned(params, txBuffer, txBufferSize);
        printf("panel-connect: ");
        ffx_tx_dump(tx);;

        uint32_t result = pushPanelTx(&tx, PanelTxViewSummary);
        printf("GOT: %ld\n", result);

        if (result == PANEL_TX_APPROVE) {
            replySignTransaction(messageId, tx);
        } else if (result == PANEL_TX_REJECT) {
            ffx_sendErrorReply(messageId, 1000, "user rejected request");
        } else {
            ffx_sendErrorReply(messageId, 42, "internal error");
        }

        free(txBuffer);

    } else {
        ffx_sendErrorReply(messageId, 1, "Unsupported operation");
    }
}

static FfxNode addText(FfxNode node, const char* text, int y, FfxFont font) {
    FfxScene scene = ffx_sceneNode_getScene(node);

    FfxNode label = ffx_scene_createLabel(scene, font, text);
    ffx_sceneGroup_appendChild(node, label);

    ffx_sceneNode_setPosition(label, ffx_point(120, y));

    ffx_sceneLabel_setAlign(label, FfxTextAlignMiddle | FfxTextAlignCenter);
    ffx_sceneLabel_setOutlineColor(label, COLOR_BLACK);

    return label;
}

static int initFunc(FfxScene scene, FfxNode panel, void* _state, void* arg) {
    State *state = _state;
    state->scene = scene;
    state->panel = panel;

    FfxNode screenDisconnect = ffx_scene_createGroup(scene);
    state->screenDisconnect = screenDisconnect;
    ffx_sceneGroup_appendChild(panel, screenDisconnect);

    FfxNode screenConnect = ffx_scene_createGroup(scene);
    state->screenConnect = screenConnect;
    ffx_sceneGroup_appendChild(panel, screenConnect);

    addText(screenDisconnect, "Listening...", 40, FfxFontLargeBold);
    addText(screenDisconnect, "Connect at:", 130, FfxFontLarge);
    addText(screenDisconnect, "firefly.box/demo", 160, FfxFontMedium);

    addText(screenConnect, "Connected!", 40, FfxFontLargeBold);
    addText(screenConnect, "Ready! Waiting", 130, FfxFontLarge);
    addText(screenConnect, "for requests...", 160, FfxFontLarge);

    ffx_onEvent(FfxEventMessage, onMessage, state);
    ffx_onEvent(FfxEventRadioState, onRadio, state);
    ffx_onEvent(FfxEventKeys, onKeys, state);

    if (ffx_isConnected()) {
        showConnect(state, false);
    } else {
        showDisconnect(state, false);
    }

    return 0;
}

int pushPanelConnect() {
    return ffx_pushPanel(initFunc, sizeof(State), FfxPanelStyleSlideLeft,
      NULL);
}
