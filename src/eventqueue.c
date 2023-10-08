#include "ldk/eventqueue.h"
#include <memory.h>
#include <string.h>

static struct
{
  LDKEventHandler callbacks[LDK_EVENT_HANDLERS_MAX];
  LDKEventType    mask[LDK_EVENT_QUEUE_SIZE];
  LDKEvent        events[LDK_EVENT_QUEUE_SIZE];
  void*           data[LDK_EVENT_QUEUE_SIZE];
  uint32          numEvents;
  uint32          numHandlers;
} internal = { 0 };

void ldkEventPush(LDKEvent* event)
{
  uint32 index = internal.numEvents++;
  if (index >= LDK_EVENT_QUEUE_SIZE)
  {
    ldkLogError("Failed to push event to the event queue because the maximum number of events (%d) was reached.", LDK_EVENT_QUEUE_SIZE);
    return;
  }
  memcpy((void*) &internal.events[index], event, sizeof(LDKEvent));
}

void ldkEventCleanup()
{
  internal.numEvents = 0;
}

void ldkEventHandlerAdd(LDKEventHandler handler, LDKEventType mask, void *data)
{
  int32 index = internal.numHandlers++;
  if (index >= LDK_EVENT_HANDLERS_MAX)
  {
    ldkLogError("Failed to register event handler because the maximum number of handelrs (%d) was reached.", LDK_EVENT_HANDLERS_MAX);
    return;
  }

  internal.callbacks[index] = handler;
  internal.data[index]      = data;
  internal.mask[index]      = mask;
}

void ldkEventHandlerRemove(LDKEventHandler handler)
{
  const int32 numHandlers = internal.numHandlers;
  const int32 lastHandlerindex = numHandlers - 1;

  for (int i = 0; i < numHandlers; i++)
  {
    if (internal.callbacks[i] == handler)
    {
      if (i != lastHandlerindex)
      {
        internal.callbacks[i] = internal.callbacks[lastHandlerindex];
        internal.mask[i] = internal.mask[lastHandlerindex];
        internal.data[i] = internal.data[lastHandlerindex];

        internal.callbacks[lastHandlerindex] = 0;
        internal.mask[lastHandlerindex] = 0;
        internal.data[lastHandlerindex] = 0;
      }
      internal.numHandlers--;
    }
  }
}

LDKEventType ldkEventHandlerMaskGet(LDKEventHandler handler)
{
  const int32 numHandlers = internal.numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (internal.callbacks[i] == handler)
    {
      return internal.mask[i];
    }
  }
  return LDK_EVENT_NONE;
}

void ldkEventHandlerMaskSet(LDKEventHandler handler, LDKEventType mask)
{
  const int32 numHandlers = internal.numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (internal.callbacks[i] == handler)
    {
      internal.mask[i] = mask;
      break;
    }
  }
}

void ldkEventQueueBroadcast()
{
  if (internal.numHandlers == 0 || internal.numEvents == 0)
    return;

  for (uint32 eventIndex = 0; eventIndex < internal.numEvents; eventIndex++)
  {
    for (uint32 handlerIndex = 0; handlerIndex < internal.numHandlers; handlerIndex++)
    {
      LDKEventType mask = internal.mask[handlerIndex];
      if (mask & internal.events[eventIndex].type)
        if (internal.callbacks[handlerIndex](&internal.events[eventIndex], internal.data[handlerIndex]))
          break;
    }
  } 
  internal.numEvents = 0;
}
