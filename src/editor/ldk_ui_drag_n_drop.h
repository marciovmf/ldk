#ifndef LDK_UI_DRAG_N_DROP_H
#define LDK_UI_DRAG_N_DROP_H

#include <ldk_common.h>
#include <module/ldk_ui.h>
#include <stdx/stdx_string.h>

#include <stdbool.h>

#define LDK_EDITOR_DRAG_N_DROP_PAYLOAD_FILE_PATH 0x46494C45u

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * Replaces the current drag-and-drop payload.
   *
   * Setting a payload only arms a possible drag. The drag becomes active
   * after the mouse moves beyond the activation threshold while the left
   * button remains pressed.
   */
  void ldk_ui_drag_n_drop_payload_set(
      u32 type, const XSmallstr *payload);

  /**
   * Updates drag activation and draws the preview icon at the current mouse
   * position once the activation threshold has been exceeded.
   *
   * The preview is non-interactive and is submitted to the late UI draw
   * buffers so it is not clipped by the source window.
   */
  void ldk_ui_drag_n_drop_preview_draw(LDKUIContext *ui, LDKUIIcon icon);

  /**
   * Copies the current drag-and-drop payload and removes it from the mailbox.
   *
   * An armed payload that never crossed the drag activation threshold is
   * removed but is not returned as a valid drop.
   *
   * @return true only when an active drag payload was available.
   */
  bool ldk_ui_drag_n_drop_payload_get_and_remove(
      u32 *out_type, XSmallstr *out_payload);

#ifdef __cplusplus
}
#endif

#endif // LDK_UI_DRAG_N_DROP_H
