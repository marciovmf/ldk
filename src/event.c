#include "ldk/event.h"
#include <memory.h>

static struct
{
  LDKEventHandler callbacks[LDK_EVENT_HANDLERS_MAX];
  LDKEventType    mask[LDK_EVENT_QUEUE_SIZE];
  LDKEvent        events[LDK_EVENT_QUEUE_SIZE];
  uint32          numEvents;
  uint32          numHandlers;
} eventQueue_ = { 0 };

void ldk_event_push(LDKEvent* event)
{
  uint32 index = eventQueue_.numEvents++;
  if (index >= LDK_EVENT_QUEUE_SIZE)
  {
    ldk_log_error("Failed to push event to the event queue because the maximum number of events (%d) was reached.", LDK_EVENT_QUEUE_SIZE);
    return;
  }
  memcpy((void*) &eventQueue_.events[index], event, sizeof(LDKEvent));
}

void ldk_event_queue_reset()
{
  eventQueue_.numEvents = 0;
}


void ldk_event_handler_add(LDKEventType mask, LDKEventHandler handler)
{
  int32 index = eventQueue_.numHandlers++;
  if (index >= LDK_EVENT_HANDLERS_MAX)
  {
    ldk_log_error("Failed to register event handler because the maximum number of handelrs (%d) was reached.", LDK_EVENT_HANDLERS_MAX);
    return;
  }

  eventQueue_.mask[index]       = mask;
  eventQueue_.callbacks[index]  = handler;
}

void ldk_event_broadcast()
{
  if (eventQueue_.numHandlers == 0 || eventQueue_.numEvents == 0)
    return;

  for (int eventIndex = 0; eventIndex < eventQueue_.numEvents; eventIndex++)
  {
    LDKEventType eventType = eventQueue_.mask[eventIndex];

    for (int handlerIndex = 0; handlerIndex < eventQueue_.numHandlers; handlerIndex++)
    {
      LDKEventType mask = eventQueue_.mask[handlerIndex];
      if (mask & eventQueue_.events[eventIndex].type)
        if ( eventQueue_.callbacks[handlerIndex]( eventQueue_.events[eventIndex] ))
          break;
    }
  } 
  eventQueue_.numEvents = 0;
}
