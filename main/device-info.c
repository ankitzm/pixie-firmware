#include <stdio.h>
#include <string.h>

#include "esp_ds.h"
#include "esp_efuse.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "nvs_flash.h"

#include "firefly-address.h"
#include "firefly-bip32.h"
#include "firefly-ecc.h"
#include "firefly-hash.h"

#include "device-info.h"
#include "utils.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_memory_utils.h"

#define DEVICE_INFO_BLOCK   (EFUSE_BLK3)
#define ATTEST_SLOT         (2)
#define ATTEST_KEY_BLOCK    (EFUSE_BLK_KEY2)
#define ATTEST_HMAC_KEY     (HMAC_KEY2)


static uint32_t modelNumber = 0;
static uint32_t serialNumber = 0;

static DeviceStatus status = DeviceStatusNotInitialized;

static uint8_t attestProof[64] = { 0 };
static uint8_t pubkeyN[384] = { 0 };
esp_ds_data_t *cipherdata = NULL;

static StaticSemaphore_t privkey0LockBuffer;
static SemaphoreHandle_t privkey0Lock = NULL;

static void reverseBytes(uint8_t *data, size_t length) {
    for (int i = 0; i < length / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[length - 1 - i];
        data[length - 1 - i] = tmp;
    }
}

uint32_t device_modelNumber() { return modelNumber; }
uint32_t device_serialNumber() { return serialNumber; }

DeviceStatus device_canAttest() { return (status == DeviceStatusOk); }

DeviceStatus device_getModelName(char *output, size_t length) {
    if (length == 0) { return DeviceStatusTruncated; }

    if (status == DeviceStatusNotInitialized) {
        int l = snprintf(output, length, "[uninitialized]");
        if (l >= length) { return DeviceStatusTruncated; }
        return status;
    }

    if (status != DeviceStatusOk) {
        int l = snprintf(output, length, "[failed]");
        if (l >= length) { return DeviceStatusTruncated; }
        return status;
    }

    if ((modelNumber >> 8) == 1) {
        int l = snprintf(output, length, "Firefly Pixie (DevKit rev.%ld)",
          modelNumber & 0xff);
        if (l >= length) { return DeviceStatusTruncated; }
        return status;
    }

    int l = snprintf(output, length, "Unknown model: 0x%lx", modelNumber);
    if (l >= length) { return DeviceStatusTruncated; }
    return status;
}

DeviceStatus device_init() {
    // Already loaded or failed to laod
    if (status == DeviceStatusOk || status != DeviceStatusNotInitialized) {
        return status;
    }

    // Semphore we use to provide async access to the account 0 test key
    privkey0Lock = xSemaphoreCreateBinaryStatic(&privkey0LockBuffer);
    xSemaphoreGive(privkey0Lock);

    // Initialize the elliptic curve library, randomizing the points to
    // mitigate side-channel attacks.
    uint8_t tweak[32];
    esp_fill_random(tweak, sizeof(tweak));
    ffx_ec_init(tweak);
    memset(tweak, 0, sizeof(tweak));

    // Read eFuse info
    uint32_t version = esp_efuse_read_reg(EFUSE_BLK3, 0);
    uint32_t _modelNumber = esp_efuse_read_reg(EFUSE_BLK3, 1);
    uint32_t _serialNumber = esp_efuse_read_reg(EFUSE_BLK3, 2);

    // Invalid eFuse info
    if (version != 0x00000001 || _modelNumber == 0 || _serialNumber == 0) {
        status = DeviceStatusMissingEfuse;
        return status;
    }

    // Open the NVS partition
    nvs_handle_t nvs;
    {
        int ret = nvs_flash_init_partition("attest");
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            status = DeviceStatusMissingNvs;
            return status;
        }

        ret = nvs_open_from_partition("attest", "secure", NVS_READONLY, &nvs);
        if (ret) {
            status = DeviceStatusMissingNvs;
            return status;
        }
    }

    // Load the cipherdata
    {
        size_t olen = sizeof(esp_ds_data_t);
        cipherdata = heap_caps_malloc(sizeof(esp_ds_data_t), MALLOC_CAP_DMA);
        memset(cipherdata, 0, sizeof(esp_ds_data_t));
        int ret = nvs_get_blob(nvs, "cipherdata", cipherdata, &olen);

        if (ret || olen != sizeof(esp_ds_data_t)) {
            free(cipherdata);
            cipherdata = NULL;
            status = DeviceStatusMissingNvs;
            return status;
        }
    }

    // Load the attest proof
    {
        size_t olen = 64;
        int ret = nvs_get_blob(nvs, "attest", attestProof, &olen);
        if (ret || olen != 64) {
            status = DeviceStatusMissingNvs;
            return status;
        }
    }

    // Load the RSA public key
    {
        size_t olen = 384;
        int ret = nvs_get_blob(nvs, "pubkey-n", pubkeyN, &olen);
        if (ret || olen != 384) {
            status = DeviceStatusMissingNvs;
            return status;
        }
    }

    serialNumber = _serialNumber;
    modelNumber = _modelNumber;

    status = DeviceStatusOk;
    return status;
}

// The PKCS#1 v1.5 prefix to place before a 32-byte payload:
// [ [ algorithm: sha-256, NULL ], PAYLOAD ]
const char PKCS_PREFIX[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65,
    0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};


// The top bit of nonce should be:
// - 0 for extenal API usage
// - 1 for less-insecure purposes used internally
static DeviceStatus _device_attest(uint8_t *challenge, uint8_t *nonce,
  DeviceAttestation *attest) {

    if (status) { return status; }

    attest->version = 1;
    memcpy(attest->nonce, nonce, 16);
    memcpy(attest->challenge, challenge, 32);
    memcpy(&attest->pubkeyN, pubkeyN, sizeof(pubkeyN));
    memcpy(&attest->attestProof, attestProof, sizeof(attestProof));


    // Compute the data to sign
    uint8_t attestation[1 + 16 + 32] = { 0 };
    {
        size_t offset = 0;

        // Version
        attestation[offset++] = 1;

        // Nonce
        memcpy(&attestation[offset], nonce, 16);
        offset += 16;

        // Challenge
        memcpy(&attestation[offset], attest->challenge, 32);
        offset += 32;
    }


    // Compute the PKCS#1 v1.5 digest
    uint8_t hash[384] = { 0 };
    {
        size_t offset = 0;

        // PKCS#1 v1.5 header
        hash[offset++] = 0x00;
        hash[offset++] = 0x01;

        // Padding
        // - 2 bytes : pre-padding header [ 0x00, 0x01 ]
        // - 1 byte  : post-padding tail [ 0x00 ]
        // - 19 bytes: ASN.1/DER prefix
        // - 32 bytes: hash
        int padding = sizeof(hash) - 2 - 1 - sizeof(PKCS_PREFIX) - 32;
        memset(&hash[offset], 0xff, padding);
        offset += padding;

        hash[offset++] = 0x00;

        memcpy(&hash[offset], PKCS_PREFIX, sizeof(PKCS_PREFIX));
        offset += sizeof(PKCS_PREFIX);

        FfxSha256Context ctx;
        ffx_hash_initSha256(&ctx);
        ffx_hash_updateSha256(&ctx, attestation, sizeof(attestation));
        ffx_hash_finalSha256(&ctx, &hash[offset]);
        offset += 32;

        reverseBytes(hash, sizeof(hash));
    }

    uint8_t sig[384] = { 0 };

    // Sync version
    int ret = esp_ds_sign(hash, cipherdata, ATTEST_HMAC_KEY, sig);
    if (ret) { return DeviceStatusFailed; }

    /*

    // Async version
    printf("AA1: %ld %d\n", ticks(), uxTaskPriorityGet(NULL));
    esp_ds_context_t *signCtx = NULL;
    int ret = esp_ds_start_sign(hash, cipherdata, ATTEST_HMAC_KEY, &signCtx);
    if (ret) {
        printf("RET1: %d\n", ret);
        return DeviceStatusFailed;
    }
    printf("AA2: %ld\n", ticks());
    while(esp_ds_is_busy()) { delay(10); }
    printf("AA3: %ld\n", ticks());
    ret = esp_ds_finish_sign(sig, signCtx);
    if (ret) {
        printf("RET2: %d\n", ret);
        return DeviceStatusFailed;
    }
    printf("AA4: %ld\n", ticks());
    */

    reverseBytes(sig, sizeof(sig));

    memcpy(attest->signature, sig, sizeof(sig));

    return DeviceStatusOk;
}


DeviceStatus device_attest(uint8_t *challenge, DeviceAttestation *attest) {

    // Create a random nonce
    uint8_t nonce[16];
    esp_fill_random(nonce, 16);

    // We reserve the top bit being set for internal usage so that
    // the external API cannot expose internal values.
    nonce[0] &= 0x7f;

    return _device_attest(challenge, nonce, attest);
}


static bool _device_testPrivateKey(FfxEcPrivkey *privkey, uint32_t account) {
    if (status || cipherdata == NULL || account > 0x7fffffff) { return false; }

    uint8_t digest[32];
    ffx_hash_keccak256(digest, (uint8_t*)cipherdata, sizeof(esp_ds_data_t));
    //taskYIELD();
    delay(1);

    // Used for the various purpose:
    //   - nonce (16 bytes)
    //   - entropy (16 bytes)
    //   - seed (64 bytes)
    uint8_t tmp[64] = { 0 };

    // tmp = nonce

    // The nonce must be stable to ensure the same key is generated
    // every time, and the top bit set so only this internal API
    // can access a given signature. (tmp is still filled with 0's)
    tmp[0] |= 0x80;

    DeviceAttestation attest = { 0 };
    if (_device_attest(digest, tmp, &attest)) { return false; }
    //taskYIELD();
    delay(1);

    // tmp = entropy

    ffx_hash_keccak256(tmp, attest.signature, 384);

    FfxMnemonic mnemonic = { 0 };
    if (!ffx_mnemonic_initEntropy(&mnemonic, tmp, 16)) { return false; }
    //taskYIELD();
    delay(1);

    static bool showMnemonic = false;
    if (showMnemonic) {
        showMnemonic = false;

        printf("Test Mnemonic: ");
        for (int i = 0; i < mnemonic.wordCount; i++) {
            printf("%s ", ffx_mnemonic_getWord(&mnemonic, i));
        }
        printf("\n");
    }

    // tmp = seed

    if (!ffx_mnemonic_getSeed(&mnemonic, "", tmp)) { return false; }
    //taskYIELD();
    delay(1);

    FfxHDNode node = { 0 };
    if (!ffx_hdnode_initSeed(&node, tmp)) { return false; }
    //taskYIELD();
    delay(1);

    // Derive: m/44'/60'/${ index }'/0/0

    if (!ffx_hdnode_deriveChild(&node, 44 | FfxHDNodeHardened)) { return false; }
    //taskYIELD();
    delay(1);

    if (!ffx_hdnode_deriveChild(&node, 60 | FfxHDNodeHardened)) { return false; }
    //taskYIELD();
    delay(1);

    if (!ffx_hdnode_deriveChild(&node, account | FfxHDNodeHardened)) { return false; }
    //taskYIELD();
    delay(1);

    if (!ffx_hdnode_deriveChild(&node, 0)) { return false; }
    //taskYIELD();
    delay(1);

    if (!ffx_hdnode_deriveChild(&node, 0)) { return false; }
    //taskYIELD();
    delay(1);

    // tmp = privkey

    if (!ffx_hdnode_getPrivkey(&node, privkey)) { return false; }

    {
        FfxEcPubkey pubkey = { 0 };
        if(!ffx_ec_computePubkey(&pubkey, privkey)) { return false; }

        FfxAddress addr = ffx_eth_getAddress(&pubkey);
        FfxChecksumAddress address = ffx_eth_checksumAddress(&addr);
        printf("Address (test account #0): %s\n", address.text);
    }

    memset(tmp, 0, sizeof(tmp));

    return true;
}


bool device_testPrivateKey(FfxEcPrivkey *privkey, uint32_t account) {

    if (account == 0) {
        static bool privkey0Ready = false;
        static FfxEcPrivkey privkey0 = { 0 };

        xSemaphoreTake(privkey0Lock, portMAX_DELAY);

        if (!privkey0Ready) {
            int t0 = ticks();

            bool status = _device_testPrivateKey(&privkey0, account);
            if (!status) {
                xSemaphoreGive(privkey0Lock);
                return false;
            }

            privkey0Ready = 1;

            t0 = ticks() - t0;
            printf("Computed Account 0: %d.%ds\n", t0 / 1000, t0 % 1000);
        }

        xSemaphoreGive(privkey0Lock);

        *privkey = privkey0;

        return true;
    }

    return _device_testPrivateKey(privkey, account);
}
