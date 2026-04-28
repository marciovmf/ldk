/**
 * @file ldk_ui.h
 *
 * Immediate-mode UI system.
 *
 * Widgets are declared every frame and arranged using layout containers.
 * Windows and panels use explicit positioning; everything else is layout-driven.
 *
 * IDs are generated internally from the UI structure and ID stack.
 *
 * The UI stores only transient interaction state (hover, active, focus).
 * Persistent data (e.g. values) belongs to the caller and is passed each frame.
 *
 * Layout is resolved at the end of the frame. A small cache is kept to
 * support interaction between frames.
 */

#ifndef LDK_UI_H
#define LDK_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ldk_common.h>
#include <ldk_os.h>
#include <ldk_ttf.h>
#include <stdx/stdx_array.h>
#include <stdx/stdx_arena.h>

#include <stdbool.h>
#include <stdint.h>

#ifndef LDK_UI_INPUT_CODEPOINTS_CAPACITY
#define LDK_UI_INPUT_CODEPOINTS_CAPACITY 16
#endif

  typedef uint32_t LDKUIId;
  typedef uintptr_t LDKUITextureHandle;
  typedef LDKUITextureHandle (*LDKUIGetFontPageTextureFn)(void* user, LDKFontInstance* font, u32 page_index);
  typedef u32 LDKUIColor;
  typedef struct LDKUIItem LDKUIItem;
  typedef struct LDKUILayoutNode LDKUILayoutNode;
  typedef struct LDKUIWindow LDKUIWindow;
  typedef struct LDKUIContext LDKUIContext;
  typedef LDKPointf LDKUIPoint;
  typedef LDKSizef LDKUISize;
  typedef LDKRectf LDKUIRect;

  typedef enum LDKUIColorSlot
  {
    LDK_UI_COLOR_TEXT,
    LDK_UI_COLOR_TEXT_DISABLED,
    LDK_UI_COLOR_WINDOW_BG,
    LDK_UI_COLOR_PANEL_BG,
    LDK_UI_COLOR_CONTROL_BG,
    LDK_UI_COLOR_CONTROL_BG_HOVERED,
    LDK_UI_COLOR_CONTROL_BG_ACTIVE,
    LDK_UI_COLOR_CONTROL_BG_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT,
    LDK_UI_COLOR_CONTROL_TEXT_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT_ACTIVE,
    LDK_UI_COLOR_CONTROL_TEXT_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_TEXT_DISABLED,
    LDK_UI_COLOR_CONTROL_BORDER,
    LDK_UI_COLOR_CONTROL_BORDER_HOVERED,
    LDK_UI_COLOR_CONTROL_BORDER_ACTIVE,
    LDK_UI_COLOR_CONTROL_BORDER_ACTIVE_HOVERED,
    LDK_UI_COLOR_CONTROL_BORDER_DISABLED,
    LDK_UI_COLOR_BORDER,
    LDK_UI_COLOR_FOCUS,
    LDK_UI_COLOR_SLIDER_BAR_TRACK,
    LDK_UI_COLOR_SLIDER_BAR_TRACK_HOVERED,
    LDK_UI_COLOR_SLIDER_BAR_TRACK_ACTIVE,
    LDK_UI_COLOR_SLIDER_BAR_FILL,
    LDK_UI_COLOR_SLIDER_TRACK,
    LDK_UI_COLOR_SLIDER_TRACK_HOVERED,
    LDK_UI_COLOR_SLIDER_TRACK_ACTIVE,
    LDK_UI_COLOR_SLIDER_FILL,
    LDK_UI_COLOR_SLIDER_THUMB,
    LDK_UI_COLOR_SLIDER_THUMB_HOVERED,
    LDK_UI_COLOR_SLIDER_THUMB_ACTIVE,
    LDK_UI_COLOR_TITLE_BAR,
    LDK_UI_COLOR_TITLE_BAR_FOCUSED,
    LDK_UI_COLOR_SCROLLBAR_TRACK,
    LDK_UI_COLOR_SCROLLBAR_THUMB,
    LDK_UI_COLOR_SCROLLBAR_THUMB_HOVERED,
    LDK_UI_COLOR_SCROLLBAR_THUMB_ACTIVE,
    LDK_UI_COLOR_COUNT,
  } LDKUIColorSlot;

  typedef enum LDKUIScrollFlags
  {
    LDK_UI_SCROLL_VERTICAL   = 1 << 0,
    LDK_UI_SCROLL_HORIZONTAL = 1 << 1,
    LDK_UI_SCROLL_BOTH =
      LDK_UI_SCROLL_VERTICAL |
      LDK_UI_SCROLL_HORIZONTAL
  } LDKUIScrollFlags;

  typedef struct LDKUITheme
  {
    LDKUIColor colors[LDK_UI_COLOR_COUNT];
    float control_border_size;
    float window_border_size;
    float window_interaction_border_size;
    float slider_bar_track_height;
    float slider_track_height;
    float slider_thumb_width;
  } LDKUITheme;

  typedef struct LDKUIVertex
  {
    float x;
    float y;
    float u;
    float v;
    u32 color;
  } LDKUIVertex;

  typedef struct LDKUIDrawCmd
  {
    LDKUITextureHandle texture;
    LDKUIRect clip_rect;
    u32 index_offset;
    u32 index_count;
  } LDKUIDrawCmd;

  typedef struct LDKUIWidgetState
  {
    LDKUIId id;
    u32 last_frame_touched;
    LDKUIRect rect;
    LDKUIRect clip_rect;
    LDKUISize content_size;
  } LDKUIWidgetState;

  struct LDKUIWindow
  {
    LDKUIId id;
    char title[64];
    LDKUIRect title_bar_rect;
    LDKUIRect content_rect;
    LDKUILayoutNode* root_layout;
  };

  X_ARRAY_TYPE_NAMED(LDKUIWindow, ldk_ui_window);
  X_ARRAY_TYPE_NAMED(LDKUIId, ldk_ui_id);
  X_ARRAY_TYPE_NAMED(LDKUIVertex, ldk_ui_vertex);
  X_ARRAY_TYPE_NAMED(uint32_t, ldk_ui_u32);
  X_ARRAY_TYPE_NAMED(LDKUIDrawCmd, ldk_ui_draw_cmd);
  X_ARRAY_TYPE_NAMED(LDKUIItem*, ldk_ui_item_ptr);
  X_ARRAY_TYPE_NAMED(LDKUIWidgetState, ldk_ui_widget_state);

  typedef enum 
  {
    LDK_UI_THEME_CUSTOM = 0,
    LDK_UI_THEME_DEFAULT_LIGHT = 1,
    LDK_UI_THEME_DEFAULT_DARK = 2,
  } LDKUIThemeType;

  typedef struct LDKUIRenderData
  {
    LDKUIVertex const* vertices;
    u32 vertex_count;
    u32 const* indices;
    u32 index_count;
    LDKUIDrawCmd const* commands;
    uint32_t command_count;
  } LDKUIRenderData;

  typedef struct LDKUIConfig
  {
    u32 frame_arena_size;
    u32 initial_vertex_capacity;
    u32 initial_index_capacity;
    u32 initial_command_capacity;
    u32 initial_window_capacity;
    u32 initial_id_stack_capacity;
    const char* font;
    void* font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;
    LDKUIThemeType theme;
    u32 font_size;
  } LDKUIConfig;

  typedef enum LDKUILayoutDirection
  {
    LDK_UI_LAYOUT_VERTICAL = 0,
    LDK_UI_LAYOUT_HORIZONTAL = 1
  } LDKUILayoutDirection;

  typedef enum LDKUIItemType
  {
    LDK_UI_ITEM_SPACER = 0,
    LDK_UI_ITEM_LABEL = 1,
    LDK_UI_ITEM_BUTTON = 2,
    LDK_UI_ITEM_TOGGLE_BUTTON = 3,
    LDK_UI_ITEM_SLIDER_BAR = 4,
    LDK_UI_ITEM_SLIDER = 5,
    LDK_UI_ITEM_LAYOUT = 6,
    LDK_UI_ITEM_COLOR_VIEW = 7,
    LDK_UI_ITEM_SCROLL_AREA = 8,
    LDK_UI_ITEM_TEXT_BOX = 9,
  } LDKUIItemType;

  typedef enum LDKUITextBoxResult
  {
    LDK_UI_TEXT_BOX_NONE      = 0,
    LDK_UI_TEXT_BOX_CHANGED   = 1 << 0,
    LDK_UI_TEXT_BOX_COMMITTED = 1 << 1,
    LDK_UI_TEXT_BOX_CANCELED  = 1 << 2,
  } LDKUITextBoxResult;

  typedef struct LDKUINextLayout
  {
    bool has_width;
    bool has_height;
    bool has_min_width;
    bool has_min_height;
    bool has_expand_width;
    bool has_expand_height;
    float width;
    float height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    bool has_max_width;
    bool has_max_height;
    bool expand_width;
    bool expand_height;
  } LDKUINextLayout;

  struct LDKUIItem
  {
    LDKUIId id;
    LDKUIItem* next_sibling;
    LDKUIItemType type;
    LDKUIRect rect;
    float preferred_width;
    float preferred_height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    bool has_max_width;
    bool has_max_height;
    bool expand_width;
    bool expand_height;
    bool clicked;
    bool changed;
    char const* text;
    union
    {
      struct
      {
        bool value;
      } toggle_button;
      struct
      {
        float value;
        float min_value;
        float max_value;
      } slider;
      struct
      {
        LDKUILayoutNode* node;
      } layout;
      struct
      {
        LDKUIColor color;
        char label[32];
      } color_view;
      struct
      {
        u32 cursor;
        u32 select_start;
        u32 select_end;
        bool focused;
      } text_box;
      struct
      {
        LDKUILayoutNode* node;
        LDKUIPoint scroll;
        LDKUIScrollFlags flags;

        bool has_vertical_scrollbar;
        bool has_horizontal_scrollbar;

        LDKUIRect vertical_track_rect;
        LDKUIRect vertical_thumb_rect;

        LDKUIRect horizontal_track_rect;
        LDKUIRect horizontal_thumb_rect;
      } scroll_area;
    } data;
  };

  struct LDKUILayoutNode
  {
    LDKUIId id;
    u32 child_count;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    float spacing;
    float padding;
    LDKUIItem* first_item;
    LDKUIItem* last_item;
    LDKUILayoutNode* parent;
    LDKUIWindow* window;
  };

  typedef struct LDKUITextInputState
  {
    u32 codepoints[LDK_UI_INPUT_CODEPOINTS_CAPACITY];
    u32 codepoint_count;
  } LDKUITextInputState;

  struct LDKUIContext
  {
    XArena* frame_arena;
    LDKFontInstance* font;    //TODO: Move this to theme
    void* font_file;          // this is a XFile*
    void* font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;
    // User owned, passed in every frame
    LDKMouseState const* mouse;
    LDKKeyboardState const* keyboard;
    LDKUITextInputState const* input_text;
    // UI owned
    LDKUIWindow* hovered_window;
    LDKUIWindow* focused_window;
    LDKUIWindow* current_window;
    LDKUILayoutNode* current_layout;
    LDKUINextLayout next_layout;
    // Rendering
    XArray_ldk_ui_window* windows;
    XArray_ldk_ui_id* id_stack;
    XArray_ldk_ui_vertex* vertices;
    XArray_ldk_ui_u32* indices;
    XArray_ldk_ui_draw_cmd* commands;
    XArray_ldk_ui_widget_state* widget_states;
    // Theme and state
    LDKUITheme theme;
    LDKUIRenderData render_data;
    LDKUIRect viewport;
    LDKUIId hot_id;
    LDKUIId active_id;
    LDKUIId focused_id;
    LDKUIId last_id;
    LDKUIId text_box_id;
    LDKUIId dragging_item;
    u32 root_item_count;
    u32 frame_index;
    u32 text_cursor;
    u32 text_select_start;
    u32 text_select_end;
    float drag_x;
    float drag_y;
  };


  // lifecycle
  LDK_API bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config);
  LDK_API void ldk_ui_terminate(LDKUIContext* ctx);

  // Theme
  LDK_API void ldk_ui_set_theme(LDKUIContext* ctx, LDKUIThemeType type, LDKUITheme* custom);

  // frame
  LDK_API void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUITextInputState const* text_input, LDKUIRect viewport);
  LDK_API void ldk_ui_end_frame(LDKUIContext* ctx);
  LDK_API LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx);

  // id stack
  LDK_API void ldk_ui_push_id_u32(LDKUIContext* ctx, uint32_t value);
  LDK_API void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value);
  LDK_API void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value);
  LDK_API void ldk_ui_pop_id(LDKUIContext* ctx);

  // next-item layout hints
  LDK_API void ldk_ui_set_next_width(LDKUIContext* ctx, float width);
  LDK_API void ldk_ui_set_next_height(LDKUIContext* ctx, float height);
  LDK_API void ldk_ui_set_next_min_width(LDKUIContext* ctx, float width);
  LDK_API void ldk_ui_set_next_min_height (LDKUIContext* ctx, float height);

  LDK_API void ldk_ui_set_next_preferred_width(LDKUIContext* ctx, float width);
  LDK_API void ldk_ui_set_next_preferred_height(LDKUIContext* ctx, float height);
  LDK_API void ldk_ui_set_next_preferred_size(LDKUIContext* ctx, float width, float height);

  LDK_API void ldk_ui_set_next_fixed_width(LDKUIContext* ui, float width);
  LDK_API void ldk_ui_set_next_fixed_height(LDKUIContext* ui, float height);
  LDK_API void ldk_ui_set_next_fixed_size(LDKUIContext* ui, float width, float height);

  LDK_API void ldk_ui_set_next_expand_width(LDKUIContext* ctx, bool expand);
  LDK_API void ldk_ui_set_next_expand_height(LDKUIContext* ctx, bool expand);

  // layout
  LDK_API void ldk_ui_begin_vertical(LDKUIContext* ctx);
  LDK_API void ldk_ui_end_vertical(LDKUIContext* ctx);
  LDK_API void ldk_ui_begin_horizontal(LDKUIContext* ctx);
  LDK_API void ldk_ui_end_horizontal(LDKUIContext* ctx);

  // Pane
  LDK_API bool ldk_ui_begin_pane(LDKUIContext* ctx, LDKUIRect rect);
  LDK_API void ldk_ui_end_pane(LDKUIContext* ctx);

  // windows
  LDK_API LDKUIRect ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect);
  LDK_API void ldk_ui_end_window(LDKUIContext* ctx);

  // widgets
  LDK_API void ldk_ui_label(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_button(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_toggle_button(LDKUIContext* ctx, char const* text, bool value);
  LDK_API float ldk_ui_slider_bar(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value);
  LDK_API float ldk_ui_slider(LDKUIContext* ctx, char const* text, float value, float min_value, float max_value);
  LDK_API void ldk_ui_spacer(LDKUIContext* ctx);

  LDK_API void ldk_ui_color_view(LDKUIContext* ctx, LDKUIColor color);

  LDK_API LDKUIPoint ldk_ui_begin_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll, LDKUIScrollFlags flags);

  LDK_API void ldk_ui_end_scroll_area(LDKUIContext* ctx);

  LDK_API LDKUIPoint ldk_ui_begin_vertical_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll);
  LDK_API void ldk_ui_end_vertical_scroll_area(LDKUIContext* ctx);

  LDK_API LDKUIPoint ldk_ui_begin_horizontal_scroll_area(LDKUIContext* ctx, LDKUIPoint scroll);
  LDK_API void ldk_ui_end_horizontal_scroll_area(LDKUIContext* ctx);

  LDK_API u32 ldk_ui_text_box(LDKUIContext* ctx, char* buffer, u32 buffer_size);

#ifdef __cplusplus
}
#endif

#endif // LDK_UI_H
