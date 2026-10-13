/**
 * # Event queues
 *
 * Event queue and dispatch system
 *
 * Provides event buffering, handler registration, and propagation control.
 */

#ifndef LDK_EVENT_QUEUE_H
#define LDK_EVENT_QUEUE_H

#include <ldk_common.h>
#include <ldk_event.h>

#ifndef LDK_EVENT_QUEUE_SIZE
/** @group modules.eventqueue.lifecycle */
#define LDK_EVENT_QUEUE_SIZE 255
#endif

#ifndef LDK_EVENT_HANDLERS_MAX
/** @group modules.eventqueue.lifecycle */
#define LDK_EVENT_HANDLERS_MAX 32
#endif

/**
 * @brief Callback handling an engine event.
 * @param event Event to inspect or receive.
 * @param optional Optional used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.eventqueue.lifecycle
 */
typedef bool (*LDKEventHandler)(const LDKEvent* event, void* optional);

#ifdef __cplusplus
extern "C" {
#endif

/** @group modules.eventqueue.lifecycle */
typedef struct
{
  LDKEventHandler callbacks[LDK_EVENT_HANDLERS_MAX];
  LDKEventType    mask[LDK_EVENT_HANDLERS_MAX];
  void*           data[LDK_EVENT_QUEUE_SIZE];
  LDKEvent        events[LDK_EVENT_QUEUE_SIZE];
  u32             numEvents;
  u32             numHandlers;
} LDKEventQueue;

/**
 * @brief Initialize the event queue.
 * @param queue Queue used by this operation.
 * @return True if the operation succeeds; false otherwise.
 * @group modules.eventqueue.lifecycle
 */
LDK_API bool ldk_event_queue_initialize(LDKEventQueue* queue);
/**
 * @brief Shut down the event queue.
 * @param queue Queue used by this operation.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_queue_terminate(LDKEventQueue* queue);
/**
 * @brief Push the event onto its stack or queue.
 * @param queue Queue used by this operation.
 * @param event Event to inspect or receive.
 * @group modules.eventqueue.publishing
 */
LDK_API void ldk_event_push(LDKEventQueue* queue, LDKEvent* event);
/**
 * @brief Register a callback for engine events.
 * @param queue Queue used by this operation.
 * @param handler Handler called for the corresponding event or diagnostic.
 * @param mask Mask used by this operation.
 * @param optional Optional used by this operation.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_handler_add(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask, void* optional);
/**
 * @brief Remove an engine event callback.
 * @param queue Queue used by this operation.
 * @param handler Handler called for the corresponding event or diagnostic.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_handler_remove(LDKEventQueue* queue, LDKEventHandler handler);
/**
 * @brief Read the event mask associated with a handler.
 * @param queue Queue used by this operation.
 * @param handler Handler called for the corresponding event or diagnostic.
 * @return The requested value.
 * @group modules.eventqueue.consuming
 */
LDK_API LDKEventType ldk_event_handler_mask_get(LDKEventQueue* queue, LDKEventHandler handler);
/**
 * @brief Change the event mask associated with a handler.
 * @param queue Queue used by this operation.
 * @param handler Handler called for the corresponding event or diagnostic.
 * @param mask Mask used by this operation.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_handler_mask_set(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask);
/**
 * @brief Clear the state of the event queue.
 * @param queue Queue used by this operation.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_queue_clear(LDKEventQueue* queue);
/**
 * @brief Broadcast an event to subscribers of its type.
 * @param queue Queue used by this operation.
 * @group modules.eventqueue.lifecycle
 */
LDK_API void ldk_event_queue_broadcast(LDKEventQueue* queue);

#ifdef __cplusplus
}
#endif

#endif //LDK_EVENT_QUEUE_H

