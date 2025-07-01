#ifndef __DEVICE_INFO_H__
#define __DEVICE_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdint.h>

#include "firefly-ecc.h"


#define VERSION              (0x00000101)

#define MANUFACTURER_NAME    ("Firefly")
#define DEVICE_NAME          ("Firefly Pixie")


typedef enum DeviceStatus {
    DeviceStatusOk              = 0,

    // Initialization errors
    DeviceStatusFailed          = -1,
    DeviceStatusNotInitialized  = -10,
    DeviceStatusMissingEfuse    = -40,
    DeviceStatusMissingNvs      = -41,
    DeviceStatusOutOfMemory     = -50,

    // Operation errors
    DeviceStatusTruncated       = 10,
} DeviceStatus;

//typedef DeviceCapability {
//    DeviceCapabilityDPad        = (1 << 0),
//} DeviceCapability;

uint32_t device_modelNumber();
uint32_t device_serialNumber();

//uint32_t device_capabilities();

DeviceStatus device_init();

DeviceStatus device_getModelName(char *output, size_t length);

DeviceStatus device_canAttest();

#define CHALLENGE_LENGTH    (32)

typedef struct DeviceAttestation {
    // Version; should always be 1 (currently)
    uint8_t version;

    // A random nonce selected by the device during signing
    uint8_t nonce[16];

    // A challenge provided by the party requesting attestation
    uint8_t challenge[CHALLENGE_LENGTH];

    // Device info
    uint32_t modelNumber;
    uint32_t serialNumber;

    // The device RSA public key (e=65537)
    uint8_t pubkeyN[384];

    // Signature provided during provisioning that the given
    // device information has been authenticated by Firefly
    uint8_t attestProof[64];

    // The RSA attestion (signature) from this device
    uint8_t signature[384];
} DeviceAttestation;

/**
 *  Response:
 *   - device info [4 bytes]
 *   - P_device.address [20 bytes]
 *   - P_firefly.sign(device info ++ P_device.address) [64 bytes]
 *   - P_device.sign(device info ++ timestamp ++ challenge) [64 bytes]
 *
 *  Verify:
 *   - ecrecover(deviceInfo ++ P_device.address) => P_firefly.address
 *   - ecrecover(deviceInfo ++ timestamp ++ challenge) => P_device.address
 *
 *  Note:
 *   - all signatures are EIP-2098 compressed signatures
 */
DeviceStatus device_attest(uint8_t *challenge, DeviceAttestation *attest);

/**
 *  This privste key is ONLY for testing purposes. It should NEVER
 *  be used for anything meaningful.
 *
 *  It is derived from the device encrypted attestation and signature
 *  using the device attestation key.
 *
 *  The [[device_init]] must have been successfully called for this
 *  to return a valid private key, and will return NULL otherwise.
 *
 *  The %%data%% buffer must be at least [[FFX_PRIVKEY_LENGTH]] long
 *  and will contain the private key after calling. It should be zero-ed
 *  before returned to the system and general cryptographic protections
 *  should be applied to it.
 */
bool device_testPrivateKey(FfxEcPrivkey *privkey, uint32_t account);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DEVICE_INFO_H__ */
