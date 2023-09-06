/**
 *
 * eventqueue.h
 * 
 * Provides an event queue that can be subscribed for event notifications
 *
 */

#ifndef LDK_EVENT_QUEUE_H
#define LDK_EVENT_QUEUE_H

#include "common.h"

#ifndef LDK_EVENT_QUEUE_SIZE
#define LDK_EVENT_QUEUE_SIZE 255
#endif

#ifndef LDK_EVENT_HANDLERS_MAX
#define LDK_EVENT_HANDLERS_MAX 32
#endif

typedef bool (*LDKEventHandler)(const LDKEvent* event, void* optional);

LDK_API void ldkEventPush(LDKEvent* event);
LDK_API void ldkEventHandlerAdd(LDKEventHandler handler, LDKEventType mask, void* optional);
LDK_API void ldkEventHandlerRemove(LDKEventHandler handler);
LDK_API LDKEventType ldkEventHandlerMaskGet(LDKEventHandler handler);
LDK_API void ldkEventHandlerMaskSet(LDKEventHandler handler, LDKEventType mask);
LDK_API void ldkEventQueueCleanup();
LDK_API void ldkEventQueueBroadcast();

#endif //LDK_EVENT_QUEUE_H

