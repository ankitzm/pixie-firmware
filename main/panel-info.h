#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define PADDING         (15)


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

/*
typedef enum Format {
    FormatNone    = 0,
    FormatTitle,
    FormatHeading,
    FormatValue,
    FormatAddress
} Format;
*/

#define PANEL_TX_REJECT        (0)
#define PANEL_TX_APPROVE       (1)


void appendTitle(void *infoState, const char* title);

void appendEntry(void *infoState, const char* heading, const char* value,
  uint16_t userData);

void beginEntry(void *infoState);
void appendText(void *infoState, const char* text, FfxFont font,
  color_ffxt color);
void endEntry(void *infoState);

void appendButton(void *infoState, const char* label, color_ffxt color,
  uint16_t userData);

void appendHR(void *infoState);
void appendPadding(void *infoState, size_t size);


typedef int (*InfoInitFunc)(void *infoState, void *state, void *arg);
typedef void (*InfoSelectFunc)(void *state, uint16_t userData);

uint32_t pushPanelInfo(InfoInitFunc initFunc, size_t stateSize,
  InfoSelectFunc selectFunc, void *arg);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANEL_INFO_H__ */
