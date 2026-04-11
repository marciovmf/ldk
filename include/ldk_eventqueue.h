/**
 * @file   ldk_eventqueue.h
 * @brief  Event queue and dispatch system
 *
 * Provides event buffering, handler registration, and propagation control.
 */

#ifndef LDK_EVENT_QUEUE_H
#define LDK_EVENT_QUEUE_H

#include <ldk_common.h>
#include <ldk_event.h>

#ifndef LDK_EVENT_QUEUE_SIZE
#define LDK_EVENT_QUEUE_SIZE 255
#endif

#ifndef LDK_EVENT_HANDLERS_MAX
#define LDK_EVENT_HANDLERS_MAX 32
#endif

typedef bool (*LDKEventHandler)(const LDKEvent* event, void* optional);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  LDKEventHandler callbacks[LDK_EVENT_HANDLERS_MAX];
  LDKEventType    mask[LDK_EVENT_HANDLERS_MAX];
  void*           data[LDK_EVENT_QUEUE_SIZE];
  LDKEvent        events[LDK_EVENT_QUEUE_SIZE];
  u32             numEvents;
  u32             numHandlers;
} LDKEventQueue;

LDK_API bool ldk_event_queue_initialize(LDKEventQueue* queue);
LDK_API void ldk_event_queue_terminate(LDKEventQueue* queue);

LDK_API void ldk_event_push(LDKEventQueue* queue, LDKEvent* event);
LDK_API void ldk_event_handler_add(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask, void* optional);
LDK_API void ldk_event_handler_remove(LDKEventQueue* queue, LDKEventHandler handler);
LDK_API LDKEventType ldk_event_handler_mask_get(LDKEventQueue* queue, LDKEventHandler handler);
LDK_API void ldk_event_handler_mask_set(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask);
LDK_API void ldk_event_queue_clear(LDKEventQueue* queue);
LDK_API void ldk_event_queue_broadcast(LDKEventQueue* queue);

#ifdef __cplusplus
}
#endif

#endif //LDK_EVENT_QUEUE_H

