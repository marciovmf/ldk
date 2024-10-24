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
} s_msgQueue = { 0 };


void ldkEventPush(LDKEvent* event)
{
  uint32 index = s_msgQueue.numEvents++;
  if (index >= LDK_EVENT_QUEUE_SIZE)
  {
    ldkLogError("Failed to push event to the event queue because the maximum number of events (%d) was reached.", LDK_EVENT_QUEUE_SIZE);
    return;
  }
  memcpy((void*) &s_msgQueue.events[index], event, sizeof(LDKEvent));
}

void ldkEventQueueCleanup(void)
{
  s_msgQueue.numEvents = 0;
}

void ldkEventHandlerAdd(LDKEventHandler handler, LDKEventType mask, void *data)
{
  int32 index = s_msgQueue.numHandlers++;
  if (index >= LDK_EVENT_HANDLERS_MAX)
  {
    ldkLogError("Failed to register event handler because the maximum number of handelrs (%d) was reached.", LDK_EVENT_HANDLERS_MAX);
    return;
  }

  s_msgQueue.callbacks[index] = handler;
  s_msgQueue.data[index]      = data;
  s_msgQueue.mask[index]      = mask;
}

void ldkEventHandlerRemove(LDKEventHandler handler)
{
  const int32 numHandlers = s_msgQueue.numHandlers;
  const int32 lastHandlerindex = numHandlers - 1;

  for (int i = 0; i < numHandlers; i++)
  {
    if (s_msgQueue.callbacks[i] == handler)
    {
      if (i != lastHandlerindex)
      {
        s_msgQueue.callbacks[i] = s_msgQueue.callbacks[lastHandlerindex];
        s_msgQueue.mask[i] = s_msgQueue.mask[lastHandlerindex];
        s_msgQueue.data[i] = s_msgQueue.data[lastHandlerindex];

        s_msgQueue.callbacks[lastHandlerindex] = 0;
        s_msgQueue.mask[lastHandlerindex] = 0;
        s_msgQueue.data[lastHandlerindex] = 0;
      }
      s_msgQueue.numHandlers--;
    }
  }
}

LDKEventType ldkEventHandlerMaskGet(LDKEventHandler handler)
{
  const int32 numHandlers = s_msgQueue.numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (s_msgQueue.callbacks[i] == handler)
    {
      return s_msgQueue.mask[i];
    }
  }
  return LDK_EVENT_TYPE_NONE;
}

void ldkEventHandlerMaskSet(LDKEventHandler handler, LDKEventType mask)
{
  const int32 numHandlers = s_msgQueue.numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (s_msgQueue.callbacks[i] == handler)
    {
      s_msgQueue.mask[i] = mask;
      break;
    }
  }
}

void ldkEventQueueBroadcast(void)
{
  if (s_msgQueue.numHandlers == 0 || s_msgQueue.numEvents == 0)
    return;

  for (uint32 eventIndex = 0; eventIndex < s_msgQueue.numEvents; eventIndex++)
  {
    for (uint32 handlerIndex = 0; handlerIndex < s_msgQueue.numHandlers; handlerIndex++)
    {
      LDKEventType mask = s_msgQueue.mask[handlerIndex];
      if (mask & s_msgQueue.events[eventIndex].type)
        if (s_msgQueue.callbacks[handlerIndex](&s_msgQueue.events[eventIndex], s_msgQueue.data[handlerIndex]))
          break;
    }
  } 
  s_msgQueue.numEvents = 0;
}

