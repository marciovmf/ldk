/**
 *
 * event.h
 * 
 * Functions and types for reading events or pushing events to an event queue
 *
 */

#ifndef LDK_EVENT_H
#define LDK_EVENT_H

#include "common.h"

#ifndef LDK_EVENT_QUEUE_SIZE
#define LDK_EVENT_QUEUE_SIZE 255
#endif

#ifndef LDK_EVENT_HANDLERS_MAX
#define LDK_EVENT_HANDLERS_MAX 32
#endif
typedef enum
{
  /* Event Types */
  LDK_EVENT_NONE                  = 0,
  LDK_EVENT_TYPE_GAME             = 1,
  LDK_EVENT_TYPE_WINDOW           = 1 << 1,
  LDK_EVENT_TYPE_TEXT             = 1 << 2,
  LDK_EVENT_TYPE_APPLICATION      = 1 << 3,
  LDK_EVENT_TYPE_KEYBOARD         = 1 << 4,
  LDK_EVENT_TYPE_MOUSE_MOVE       = 1 << 5,
  LDK_EVENT_TYPE_MOUSE_BUTTON     = 1 << 6,
  LDK_EVENT_TYPE_MOUSE_WHEEL      = 1 << 7,
  LDK_EVENT_TYPE_ANY              = 0xFFFF,

  /* Keyboard Event types */
  LDK_KEYBOARD_EVENT_KEY_DOWN     = 100,
  LDK_KEYBOARD_EVENT_KEY_UP       = 101,
  LDK_KEYBOARD_EVENT_KEY_HOLD     = 102,

  /* Text Event types */
  LDK_TEXT_EVENT_CHARACTER_INPUT  = 102,
  LDK_TEXT_EVENT_BACKSPACE        = 103,
  LDK_TEXT_EVENT_DEL              = 104,

  /* Mouse event types */
  LDK_MOUSE_EVENT_MOVE            = 106,
  LDK_MOUSE_EVENT_BUTTON_DOWN     = 107,
  LDK_MOUSE_EVENT_BUTTON_UP       = 108,
  LDK_MOUSE_EVENT_WHEEL_FORWARD   = 109,
  LDK_MOUSE_EVENT_WHEEL_BACKWARD  = 110,

  /* Display event types */
  LDK_WINDOW_EVENT_RESIZED       = 105,
  LDK_WINDOW_EVENT_       = 105,

} LDKEventType;

typedef struct
{
  LDKEventType type;
  uint32 character;
} LDKTextEvent;

typedef struct
{
  LDKEventType type;
  uint32 width;
  uint32 height;
} LDKDisplayEvent;


typedef struct
{
  LDKEventType type;
  uint32 keyCode; 
  bool ctrlIsDown;
  bool shiftIsDown;
  bool altIsDown;
} LDKKeyboardEvent;


typedef struct
{
  LDKEventType type;
  int cursorX;
  int cursorY;

  union
  {
    int32 wheelDelta;
    uint8 mouseButton;
  };

} LDKMouseEvent;

typedef struct
{
  LDKEventType type;
  union
  {
    LDKTextEvent         textEvent;
    LDKDisplayEvent      displayEvent;
    LDKKeyboardEvent     keyboardEvent;
    LDKMouseEvent        mouseEvent;
  };
} LDKEvent;


typedef bool (*LDKEventHandler)(LDKEvent);

LDK_API void ldk_event_push(LDKEvent* event);
LDK_API void ldk_event_handler_add(LDKEventType mask, LDKEventHandler handler);
LDK_API void ldk_event_broadcast();

#endif //LDK_EVENT_H

