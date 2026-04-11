/**
 * @file   ldk_event.h
 * @brief  Event definitions and types
 *
 * Declares engine and OS-level event structures and enums.
 */

#ifndef LDK_EVENT_H
#define LDK_EVENT_H

#include <ldk_common.h>

//
// Events
//

typedef enum
{
  /* Event Types */
  LDK_EVENT_TYPE_NONE             = 0,
  LDK_EVENT_TYPE_GAME             = 1,
  LDK_EVENT_TYPE_WINDOW           = 1 << 1,
  LDK_EVENT_TYPE_TEXT             = 1 << 2,
  LDK_EVENT_TYPE_APPLICATION      = 1 << 3,
  LDK_EVENT_TYPE_KEYBOARD         = 1 << 4,
  LDK_EVENT_TYPE_MOUSE_MOVE       = 1 << 5,
  LDK_EVENT_TYPE_MOUSE_BUTTON     = 1 << 6,
  LDK_EVENT_TYPE_MOUSE_WHEEL      = 1 << 7,
  LDK_EVENT_TYPE_FRAME_BEFORE     = 1 << 8,
  LDK_EVENT_TYPE_FRAME_AFTER      = 1 << 9,
  LDK_EVENT_TYPE_ANY              = 0xFFFFFFFF,

  /* Keyboard Event types */
  LDK_KEYBOARD_EVENT_KEY_DOWN     = 1,
  LDK_KEYBOARD_EVENT_KEY_UP       = 2,
  LDK_KEYBOARD_EVENT_KEY_HOLD     = 3,

  /* Text Event types */
  LDK_TEXT_EVENT_CHARACTER_INPUT  = 4,
  LDK_TEXT_EVENT_BACKSPACE        = 5,
  LDK_TEXT_EVENT_DEL              = 6,

  /* Mouse event types */
  LDK_MOUSE_EVENT_MOVE            = 7,
  LDK_MOUSE_EVENT_BUTTON_DOWN     = 8,
  LDK_MOUSE_EVENT_BUTTON_UP       = 9,
  LDK_MOUSE_EVENT_WHEEL_FORWARD   = 10,
  LDK_MOUSE_EVENT_WHEEL_BACKWARD  = 11,

  /* Window event types */
  LDK_WINDOW_EVENT_RESIZED        = 14,
  LDK_WINDOW_EVENT_CLOSE          = 15,
  LDK_WINDOW_EVENT_ACTIVATE       = 16,
  LDK_WINDOW_EVENT_DEACTIVATE     = 17,
  LDK_WINDOW_EVENT_MINIMIZED      = 18,
  LDK_WINDOW_EVENT_MAXIMIZED      = 19,

} LDKEventType;

// LDKTextEvent
typedef struct
{
  LDKEventType type;
  u32 character;
} LDKTextEvent;

// LDKWindowEvent
typedef struct
{
  LDKEventType type;
  u32 width;
  u32 height;
} LDKWindowEvent;

// LDKKeyboardEvent
typedef struct
{
  LDKEventType type;
  u32 keyCode; 
  bool ctrlIsDown;
  bool shiftIsDown;
  bool altIsDown;
} LDKKeyboardEvent;

// LDKMouseEvent
typedef struct
{
  LDKEventType type;
  i32 cursorX;
  i32 cursorY;
  i32 xRel;
  i32 yRel;

  union
  {
    i32 wheelDelta;
    i32 mouseButton;
  };

} LDKMouseEvent;

// LDKFrameEvent
typedef struct
{
  u64 ticks;
  float deltaTime;
} LDKFrameEvent;

// LDKEvent
typedef struct
{
  LDKWindow window;
  LDKEventType type;
  union
  {
    LDKTextEvent        textEvent;
    LDKWindowEvent      windowEvent;
    LDKKeyboardEvent    keyboardEvent;
    LDKMouseEvent       mouseEvent;
    LDKFrameEvent       frameEvent;
  };
} LDKEvent;

#endif // LDK_EVENT_H

