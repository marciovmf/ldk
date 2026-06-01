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

#define LDK_UI_DEFAULT_CONTROL_HEIGHT 22.0f

  typedef uint32_t LDKUIId;
  typedef uintptr_t LDKUITextureHandle;
  typedef LDKUITextureHandle (*LDKUIGetFontPageTextureFn)(void* user, LDKFontInstance* font, u32 page_index);
  typedef u32 LDKUIColor;
  typedef LDKPointf LDKUIPoint;
  typedef LDKSizef LDKUISize;
  typedef LDKRectf LDKUIRect;

  typedef struct LDKUIWindow LDKUIWindow;
  typedef struct LDKUILayout LDKUILayout;
  typedef struct LDKUIHitCandidate LDKUIHitCandidate;
  typedef struct LDKUIDebugRect LDKUIDebugRect;
  typedef struct LDKUIScrollContentCache LDKUIScrollContentCache;
  typedef struct LDKUIAutoWindowCache LDKUIAutoWindowCache;
  typedef struct LDKUIContext LDKUIContext;

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
    LDK_UI_COLOR_SEPARATOR,
    LDK_UI_COLOR_COUNT,
  } LDKUIColorSlot;

  typedef struct LDKUITheme
  {
    rgba32 colors[LDK_UI_COLOR_COUNT];
    float control_border_size;
    float window_border_size;
    float window_interaction_border_size;
    float slider_track_height;
    float slider_thumb_width;
  } LDKUITheme;

  typedef struct LDKUIVertex
  {
    float x;
    float y;
    float u;
    float v;
    rgba32 color;
  } LDKUIVertex;

  typedef struct LDKUIDrawCmd
  {
    LDKUITextureHandle texture;
    LDKUIRect clip_rect;
    u32 index_offset;
    u32 index_count;
  } LDKUIDrawCmd;

  X_ARRAY_TYPE_NAMED(LDKUIId, ldk_ui_id);
  X_ARRAY_TYPE_NAMED(LDKUIVertex, ldk_ui_vertex);
  X_ARRAY_TYPE_NAMED(uint32_t, ldk_ui_u32);
  X_ARRAY_TYPE_NAMED(LDKUIDrawCmd, ldk_ui_draw_cmd);
  X_ARRAY_TYPE_NAMED(bool, ldk_ui_bool);

  typedef enum LDKUIThemeType
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

  typedef struct LDKUIIcon
  {
    uintptr_t texture; // raw texture resource
    LDKUIRect uv;
    LDKUISize size;
  } LDKUIIcon;

  typedef struct LDKUIConfig
  {
    u32 frame_arena_size;
    u32 initial_vertex_capacity;
    u32 initial_index_capacity;
    u32 initial_command_capacity;
    u32 initial_window_capacity;
    u32 initial_id_stack_capacity;
    LDKFontInstance* font;
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
    LDK_UI_ITEM_WINDOW = 1,
    LDK_UI_ITEM_LABEL = 2,
    LDK_UI_ITEM_BUTTON = 3,
    LDK_UI_ITEM_SLIDER = 4,
    LDK_UI_ITEM_SCROLLVIEW = 5,
    LDK_UI_ITEM_LAYOUT_VERTICAL = 6,
    LDK_UI_ITEM_LAYOUT_HORIZONTAL = 7,
    LDK_UI_ITEM_TEXT_BOX = 8,
    LDK_UI_ITEM_IMAGE = 9,
    LDK_UI_ITEM_ICON_BUTTON = 10,
  } LDKUIItemType;

  typedef enum LDKUISizeMode
  {
    LDK_UI_SIZE_PIXELS = 0,
    LDK_UI_SIZE_PERCENT = 1,
    LDK_UI_SIZE_FILL = 2,
    LDK_UI_SIZE_AUTO = 3,
  } LDKUISizeMode;

  typedef struct LDKUILayoutSize
  {
    LDKUISizeMode mode;
    float value;
  } LDKUILayoutSize;

  typedef enum LDKUIWindowFlags
  {
    LDK_UI_WINDOW_NONE       = 0,
    LDK_UI_WINDOW_TITLE_BAR  = 1 << 0,
    LDK_UI_WINDOW_DRAGGABLE  = 1 << 1,
    LDK_UI_WINDOW_RESIZABLE  = 1 << 2,
    LDK_UI_WINDOW_BORDER     = 1 << 3,
    LDK_UI_WINDOW_NO_BG      = 1 << 4,
    LDK_UI_WINDOW_TOOL       = LDK_UI_WINDOW_DRAGGABLE | LDK_UI_WINDOW_RESIZABLE | LDK_UI_WINDOW_BORDER | LDK_UI_WINDOW_TITLE_BAR
  } LDKUIWindowFlags;

  typedef enum LDKUIControlVisualState
  {
    LDK_UI_CONTROL_VISUAL_STATE_IDLE = 0,
    LDK_UI_CONTROL_VISUAL_STATE_HOVERED = 1,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE = 2,
    LDK_UI_CONTROL_VISUAL_STATE_ACTIVE_HOVERED = 3,
    LDK_UI_CONTROL_VISUAL_STATE_DISABLED = 4,
  } LDKUIControlVisualState;

  typedef enum LDKUIScrollFlags
  {
    LDK_UI_SCROLL_VERTICAL   = 1 << 0,
    LDK_UI_SCROLL_HORIZONTAL = 1 << 1,
    LDK_UI_SCROLL_IF_NEEDED  = 1 << 2,
    LDK_UI_SCROLL_BOTH =
      LDK_UI_SCROLL_VERTICAL |
      LDK_UI_SCROLL_HORIZONTAL
  } LDKUIScrollFlags;

  typedef struct LDKUITextInputState
  {
    u32 codepoints[LDK_UI_INPUT_CODEPOINTS_CAPACITY];
    u32 codepoint_count;
  } LDKUITextInputState;

  typedef struct LDKUIMark
  {
    LDKUILayout* layout;
    u32 measure_index;
  } LDKUIMark;

  typedef struct LDKUIMeasureEntry
  {
    LDKUILayout* layout;
    LDKUIRect rect;
  } LDKUIMeasureEntry;


  typedef enum LDKUIDebugRectType
  {
    LDK_UI_DEBUG_RECT_ITEM = 0,
    LDK_UI_DEBUG_RECT_LAYOUT = 1,
  } LDKUIDebugRectType;

  struct LDKUIHitCandidate
  {
    LDKUIId window_id;
    LDKUIId item_id;
    LDKUIRect rect;
    LDKUIRect clip_rect;
    u32 order;
  };

  struct LDKUIDebugRect
  {
    LDKUIId window_id;
    LDKUIId item_id;
    LDKUIDebugRectType type;
    LDKUIRect rect;
    LDKUIRect clip_rect;
  };

  struct LDKUIScrollContentCache
  {
    LDKUIId id;
    LDKUIRect content_rect;
    LDKUIPoint scroll;
    u32 last_frame_touched;
    bool has_scroll;
  };

  struct LDKUIAutoWindowCache
  {
    LDKUIId id;
    LDKUISize size;
    LDKUISize min_size;
    u32 measure_index;
    u32 last_frame_touched;
    bool active;
  };

  struct LDKUILayout
  {
    LDKUIId id;
    LDKUILayoutDirection direction;
    LDKUIRect rect;
    LDKUIRect content_rect;
    LDKUIRect bounding_rect;
    LDKUIPoint cursor;
    float spacing;
    float padding;
    float content_used_right;
    float content_used_bottom;
    u32 item_count;
    bool is_scrollview;
    LDKUIId scrollview_id;
    LDKUIPoint scroll;
    LDKUIScrollFlags scroll_flags;
    LDKUIRect scroll_view_rect;
    LDKUIRect scroll_clip_rect;
    LDKUIRect scrollbar_clip_rect;
    LDKUILayout* parent;

    LDKUIRect vertical_scrollbar_track_rect;
    LDKUIRect vertical_scrollbar_thumb_rect;
    LDKUIRect horizontal_scrollbar_track_rect;
    LDKUIRect horizontal_scrollbar_thumb_rect;
    bool has_vertical_scrollbar;
    bool has_horizontal_scrollbar;
    bool has_measure_entry;
    u32 measure_entry_index;
  };

  struct LDKUIWindow
  {
    LDKUIId id;
    char title[64];
    LDKUIRect rect;
    LDKUIRect title_bar_rect;
    LDKUIRect content_rect;
    u32 flags;
    i32 z_order;
    u32 last_frame_seen;
    LDKUIId focused_id;
    XArray_ldk_ui_vertex* vertices;
    XArray_ldk_ui_u32* indices;
    XArray_ldk_ui_draw_cmd* commands;
  };

  typedef enum LDKUITextBoxResult
  {
    LDK_UI_TEXT_BOX_NONE = 0,
    LDK_UI_TEXT_BOX_CHANGED = 1 << 0,
    LDK_UI_TEXT_BOX_COMMITTED = 1 << 1,
    LDK_UI_TEXT_BOX_CANCELED = 1 << 2,
  } LDKUITextBoxResult;


  X_ARRAY_TYPE_NAMED(LDKUIWindow, ldk_ui_window);
  X_ARRAY_TYPE_NAMED(LDKUIHitCandidate, ldk_ui_hit_candidate);
  X_ARRAY_TYPE_NAMED(LDKUIDebugRect, ldk_ui_debug_rect);
  X_ARRAY_TYPE_NAMED(LDKUIScrollContentCache, ldk_ui_scroll_content_cache);
  X_ARRAY_TYPE_NAMED(LDKUIAutoWindowCache, ldk_ui_auto_window_cache);
  X_ARRAY_TYPE_NAMED(LDKUIMeasureEntry, ldk_ui_measure_entry);

  struct LDKUIContext
  {
    XArena* frame_arena;
    LDKFontInstance* font;
    void* font_file;
    void* font_texture_user;
    LDKUIGetFontPageTextureFn get_font_page_texture;

    LDKMouseState const* mouse;
    LDKKeyboardState const* keyboard;
    LDKUITextInputState const* input_text;

    LDKUIWindow* current_window;
    LDKUILayout* current_layout;

    XArray_ldk_ui_window* windows;
    XArray_ldk_ui_id* id_stack;
    XArray_ldk_ui_vertex* vertices;
    XArray_ldk_ui_u32* indices;
    XArray_ldk_ui_draw_cmd* commands;
    XArray_ldk_ui_bool* disabled_stack;
    XArray_ldk_ui_hit_candidate* hit_candidates;
    XArray_ldk_ui_debug_rect* debug_rects;
    XArray_ldk_ui_scroll_content_cache* scroll_content_cache;
    XArray_ldk_ui_auto_window_cache* auto_window_cache;
    XArray_ldk_ui_id* auto_window_stack;
    XArray_ldk_ui_measure_entry* measure_entries;

    LDKUITheme theme;
    LDKUIRenderData render_data;
    LDKUIRect viewport;
    LDKUIRect last_rect;
    LDKUIRect last_bounding_rect;

    LDKUIId hovered_window_id;
    LDKUIId focused_window_id;
    LDKUIId hot_window_id;
    LDKUIId hot_id;
    LDKUIId next_hot_window_id;
    LDKUIId next_hot_id;
    LDKUIId active_window_id;
    LDKUIId active_id;
    LDKUIId focused_id;
    LDKUIId last_id;
    LDKUIId dragging_window_id;
    LDKUIId resizing_window_id;
    LDKUIId text_box_id;

    // popuup
    LDKUIId open_popup_id;
    LDKUIId current_popup_id;
    u32 popup_open_frame_index;

    // Textbox
    u32 text_cursor;
    u32 text_select_start;
    u32 text_select_end;

    u32 resizing_window_edges;
    u32 hit_order;
    u32 last_measure_entry_index;

    bool next_disabled;
    bool next_focus;
    bool debug_draw;
    bool has_next_width;
    bool has_next_height;
    bool mouse_wheel_consumed;
    LDKUILayoutSize next_width;
    LDKUILayoutSize next_height;
    u32 root_item_count;
    u32 frame_index;
    i32 next_z_order;
    float drag_x;
    float drag_y;
    float resize_start_cursor_x;
    float resize_start_cursor_y;
    LDKUIRect resize_start_rect;

    float scrollbar_drag_offset_x; 
    float scrollbar_drag_offset_y; 
    LDKCursorType cursor_type;
  };

  LDK_API bool ldk_ui_initialize(LDKUIContext* ctx, LDKUIConfig const* config);
  LDK_API void ldk_ui_terminate(LDKUIContext* ctx);

  LDK_API void ldk_ui_set_theme(LDKUIContext* ctx, LDKUIThemeType type, LDKUITheme* custom);

  LDK_API void ldk_ui_begin_frame(LDKUIContext* ctx, LDKMouseState const* mouse, LDKKeyboardState const* keyboard, LDKUITextInputState const* text_input, LDKUIRect viewport);
  LDK_API void ldk_ui_end_frame(LDKUIContext* ctx);
  LDK_API LDKUIRenderData const* ldk_ui_get_render_data(LDKUIContext const* ctx);

  //----------------------------------------------------------
  // IDs
  //----------------------------------------------------------
  LDK_API void ldk_ui_push_id_u32(LDKUIContext* ctx, uint32_t value); // Pushes a u32 ID
  LDK_API void ldk_ui_push_id_ptr(LDKUIContext* ctx, void const* value); // Pushes pointer ID
  LDK_API void ldk_ui_push_id_cstr(LDKUIContext* ctx, char const* value); // Pushes a string ID
  LDK_API void ldk_ui_pop_id(LDKUIContext* ctx);  // pops an ID from the ID stack

  //----------------------------------------------------------
  // Size spec
  //----------------------------------------------------------
  LDK_API LDKUILayoutSize ldk_ui_px(float value); // Specify a size in pixelsx
  LDK_API LDKUILayoutSize ldk_ui_percent(float value); // Specify a size in percent of the parent
  LDK_API LDKUILayoutSize ldk_ui_fill(void);  // specify a size equals to 100% of the parent size

  //----------------------------------------------------------
  // Size Hinting
  //----------------------------------------------------------
  LDK_API void ldk_ui_set_next_width(LDKUIContext* ctx, LDKUILayoutSize width);
  LDK_API void ldk_ui_set_next_height(LDKUIContext* ctx, LDKUILayoutSize height);
  LDK_API void ldk_ui_set_next_size(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height);

  //----------------------------------------------------------
  // Size Querying
  //----------------------------------------------------------
  LDK_API LDKUIRect ldk_ui_last_rect(LDKUIContext* ctx);
  LDK_API LDKUIRect ldk_ui_last_bounding_rect(LDKUIContext* ctx);
  LDK_API LDKUIMark ldk_ui_mark(LDKUIContext* ctx);
  LDK_API LDKUIRect ldk_ui_measure_from(LDKUIContext* ctx, LDKUIMark mark);

  //----------------------------------------------------------
  // Disabling controls
  //----------------------------------------------------------
  LDK_API void ldk_ui_set_next_disabled(LDKUIContext* ctx, bool disabled);
  LDK_API void ldk_ui_begin_disabled(LDKUIContext* ctx, bool disabled);
  LDK_API void ldk_ui_end_disabled(LDKUIContext* ctx);

  //----------------------------------------------------------
  // Windows
  //----------------------------------------------------------
  LDK_API LDKUIRect ldk_ui_begin_window_fixed(LDKUIContext* ctx, char const* title, LDKUIRect rect, u32 flags);
  LDK_API LDKUIRect ldk_ui_begin_window(LDKUIContext* ctx, char const* title, LDKUIRect rect, u32 flags);
  LDK_API void ldk_ui_end_window(LDKUIContext* ctx);

  //----------------------------------------------------------
  // Popup
  //----------------------------------------------------------
  LDK_API void ldk_ui_open_popup(LDKUIContext* ctx, char const* id);
  LDK_API void ldk_ui_close_current_popup(LDKUIContext* ctx);
  LDK_API bool ldk_ui_is_popup_open(LDKUIContext* ctx, char const* id);
  LDK_API bool ldk_ui_begin_popup_fixed(LDKUIContext* ctx, char const* id, LDKUIRect rect);
  LDK_API bool ldk_ui_begin_popup(LDKUIContext* ctx, char const* id, LDKUIRect rect);
  LDK_API void ldk_ui_end_popup(LDKUIContext* ctx);


  //----------------------------------------------------------
  // Layout
  //----------------------------------------------------------
  LDK_API void ldk_ui_begin_vertical(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height);
  LDK_API void ldk_ui_end_vertical(LDKUIContext* ctx);
  LDK_API void ldk_ui_begin_horizontal(LDKUIContext* ctx, LDKUILayoutSize width, LDKUILayoutSize height);
  LDK_API void ldk_ui_end_horizontal(LDKUIContext* ctx);

  //----------------------------------------------------------
  // Scrollview
  //----------------------------------------------------------
  LDK_API LDKUIPoint ldk_ui_begin_scrollview(LDKUIContext* ctx, LDKUIPoint scroll_pos, LDKUIRect content_rect, LDKUIScrollFlags flags);
  LDK_API void ldk_ui_end_scrollview(LDKUIContext* ctx);

  //----------------------------------------------------------
  // Widgets
  //----------------------------------------------------------
  LDK_API void ldk_ui_image(LDKUIContext* ctx, LDKUITextureHandle texture, LDKUIRect uv, LDKUISize size);
  LDK_API void ldk_ui_icon(LDKUIContext* ctx, LDKUIIcon icon);
  LDK_API bool ldk_ui_icon_button(LDKUIContext* ctx, LDKUIIcon icon);
  LDK_API void ldk_ui_label(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_button(LDKUIContext* ctx, char const* text);
  LDK_API bool ldk_ui_button_flat(LDKUIContext* ctx, char const* text);
  LDK_API float ldk_ui_slider(LDKUIContext* ctx, float value, float min_value, float max_value);
  LDK_API u32 ldk_ui_text_box(LDKUIContext* ctx, char* buffer, u32 buffer_size);
  LDK_API void ldk_ui_horizontal_line(LDKUIContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
