#ifndef __PANEL_NULL_H__
#define __PANEL_NULL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 *  The Null Panel is useful during testing when testing a panel that
 *  can pop itself. Normally, popping the top-level panel will crash
 *  the device, so a Null Panel (which will not ever pop itself) can
 *  be added first, and automatically trigger its sub-panel.
 */

#include "panel.h"

typedef void (*NullInit)();

void pushPanelNull(NullInit init);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_NULL_H__ */
