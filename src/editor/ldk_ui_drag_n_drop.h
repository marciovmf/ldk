#ifndef LDK_UI_DRAG_N_DROP_H
#define LDK_UI_DRAG_N_DROP_H

#include <ldk_common.h>
#include <stdx/stdx_string.h>

#include <stdbool.h>

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
