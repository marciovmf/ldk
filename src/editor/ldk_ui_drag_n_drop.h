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
   * The payload is only data. Its presence does not imply that a drag is
   * currently active, and this module does not participate in UI interaction,
   * hit testing, mouse capture, or drop-target resolution.
   */
  void ldk_ui_drag_n_drop_payload_set(
      u32 type, const XSmallstr *payload);

  /**
   * Draws the drag-and-drop preview icon at the current mouse position.
   *
   * The preview is non-interactive and is submitted to the late UI draw
   * buffers so it is not clipped by the source window.
   */
  void ldk_ui_drag_n_drop_preview_draw(LDKUIContext *ui, LDKUIIcon icon);

  /**
   * Copies the current drag-and-drop payload and removes it from the mailbox.
   *
   * @return true when a payload was available, false otherwise.
   */
  bool ldk_ui_drag_n_drop_payload_get_and_remove(
      u32 *out_type, XSmallstr *out_payload);

#ifdef __cplusplus
}
#endif

#endif // LDK_UI_DRAG_N_DROP_H
