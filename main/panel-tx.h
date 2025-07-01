
#ifndef __PANEL_TX_H__
#define __PANEL_TX_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "firefly-tx.h"


typedef enum PanelTxView {
    PanelTxViewSummary    = 0,

    PanelTxViewNoDrill    = 0x01,

    PanelTxViewData       = 0x10,
    PanelTxViewFees       = 0x11,
    PanelTxViewNetwork    = 0x12,
    PanelTxViewTo         = 0x13,
    PanelTxViewValue      = 0x14,

    // Used internally
    PanelTxActionReject   = 0x80,
    PanelTxActionApprove  = 0x81,
} PanelTxView;


#define PANEL_TX_REJECT        (0)
#define PANEL_TX_APPROVE       (1)

uint32_t pushPanelTx(FfxDataResult *tx, PanelTxView view);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_TX_H__ */
