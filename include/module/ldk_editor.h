#ifndef LDK_EDITOR_H
#define LDK_EDITOR_H

#include "module/ldk_ui.h"
#include <ldk_common.h>
#include <ldk.h>
#include <module/ldk_rhi.h>
#include <module/ldk_renderer.h>
#include <module/ldk_asset_manager.h>


#ifndef LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY
#define LDK_DEFATUL_UI_INITIAL_INDEX_CAPACITY 256
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY
#define LDK_DEFATUL_UI_INITIAL_VERTEX_CAPACITY 1024
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_COMMAND_CAPACITY 32
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_WINDOW_CAPACITY 16
#endif

#ifndef LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY 
#define LDK_DEFATUL_UI_INITIAL_STACK_CAPACITY 16
#endif


typedef struct LDKEditor
{
  LDKUIContext          ui;
  LDKAssetFont          font;
  LDKUITextInputState   text_input_state;
} LDKEditor;

LDK_API bool  ldk_editor_initialize(LDKEditor* editor, LDKConfig* config);
LDK_API void  ldk_editor_update(LDKEditor* editor, i32 width, i32 height, float delta_time);
LDK_API void  ldk_editor_terminate(LDKEditor* editor);

#endif //LDK_EDITOR_H

