#ifndef __TASK_BLE_H__
#define __TASK_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>


// 16kb
#define MAX_MESSAGE_SIZE        (1 << 14)

void taskBleFunc(void* pvParameter);

//bool panel_isMessageEnabled();
//void panel_enableMessage(bool enable);

//size_t panel_copyMessage(uint32_t messageId, uint8_t *output);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TASK_BLE_H__ */
