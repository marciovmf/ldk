#include <ldk.h>
#include <module/ldk_eventqueue.h>
#include <stdx/stdx_log.h>
#include <memory.h>

bool ldk_event_queue_initialize(LDKEventQueue* queue) {
  (void) queue;
  return true;
}

void ldk_event_queue_terminate(LDKEventQueue* queue) { (void) queue; }

void ldk_event_push(LDKEventQueue* queue, LDKEvent* event)
{
  u32 index = queue->numEvents++;
  if (index >= LDK_EVENT_QUEUE_SIZE)
  {
    ldk_log_error("Failed to push event to the event queue because the maximum number of events (%d) was reached.", LDK_EVENT_QUEUE_SIZE);
    return;
  }
  memcpy((void*) &queue->events[index], event, sizeof(LDKEvent));
}

void ldk_event_queue_clear(LDKEventQueue* queue)
{
  queue->numEvents = 0;
}

void ldk_event_handler_add(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask, void *data)
{
  i32 index = queue->numHandlers++;
  if (index >= LDK_EVENT_HANDLERS_MAX)
  {
    ldk_log_error("Failed to register event handler because the maximum number of handelrs (%d) was reached.", LDK_EVENT_HANDLERS_MAX);
    return;
  }

  queue->callbacks[index] = handler;
  queue->data[index]      = data;
  queue->mask[index]      = mask;
}

void ldk_event_handler_remove(LDKEventQueue* queue, LDKEventHandler handler)
{
  const i32 numHandlers = queue->numHandlers;
  const i32 lastHandlerindex = numHandlers - 1;

  for (int i = 0; i < numHandlers; i++)
  {
    if (queue->callbacks[i] == handler)
    {
      if (i != lastHandlerindex)
      {
        queue->callbacks[i] = queue->callbacks[lastHandlerindex];
        queue->mask[i] = queue->mask[lastHandlerindex];
        queue->data[i] = queue->data[lastHandlerindex];

        queue->callbacks[lastHandlerindex] = 0;
        queue->mask[lastHandlerindex] = 0;
        queue->data[lastHandlerindex] = 0;
      }
      queue->numHandlers--;
    }
  }
}

LDKEventType ldk_event_handler_mask_get(LDKEventQueue* queue, LDKEventHandler handler)
{
  const i32 numHandlers = queue->numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (queue->callbacks[i] == handler)
    {
      return queue->mask[i];
    }
  }
  return LDK_EVENT_TYPE_NONE;
}

void ldk_event_handler_mask_set(LDKEventQueue* queue, LDKEventHandler handler, LDKEventType mask)
{
  const i32 numHandlers = queue->numHandlers;
  for (int i = 0; i < numHandlers; i++)
  {
    if (queue->callbacks[i] == handler)
    {
      queue->mask[i] = mask;
      break;
    }
  }
}

void ldk_event_queue_broadcast(LDKEventQueue* queue)
{
  if (queue->numHandlers == 0 || queue->numEvents == 0)
    return;

  for (u32 eventIndex = 0; eventIndex < queue->numEvents; eventIndex++)
  {
    for (u32 handlerIndex = 0; handlerIndex < queue->numHandlers; handlerIndex++)
    {
      LDKEventType mask = queue->mask[handlerIndex];
      if (mask & queue->events[eventIndex].type)
        if (queue->callbacks[handlerIndex](&queue->events[eventIndex], queue->data[handlerIndex]))
          break;
    }
  } 
  queue->numEvents = 0;
}

