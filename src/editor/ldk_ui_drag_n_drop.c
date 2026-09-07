#include "ldk_ui_drag_n_drop.h"

#include <string.h>

typedef struct LDKUIDragNDropPayload
{
  u32 type;
  XSmallstr data;
  bool valid;
} LDKUIDragNDropPayload;

static LDKUIDragNDropPayload s_drag_n_drop_payload;

void ldk_ui_drag_n_drop_payload_set(u32 type, const XSmallstr *payload)
{
  if (payload == NULL)
  {
    return;
  }

  s_drag_n_drop_payload.type = type;
  s_drag_n_drop_payload.data = *payload;
  s_drag_n_drop_payload.valid = true;
}

bool ldk_ui_drag_n_drop_payload_get_and_remove(
    u32 *out_type, XSmallstr *out_payload)
{
  if (!s_drag_n_drop_payload.valid)
  {
    return false;
  }

  if (out_type != NULL)
  {
    *out_type = s_drag_n_drop_payload.type;
  }

  if (out_payload != NULL)
  {
    *out_payload = s_drag_n_drop_payload.data;
  }

  memset(&s_drag_n_drop_payload, 0, sizeof(s_drag_n_drop_payload));
  return true;
}
