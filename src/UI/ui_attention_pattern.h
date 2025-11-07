#pragma once
#include <lvgl.h>

/* Header-only color flasher with patterns (LVGL v9)
 * - Flashes by toggling text color between the label's original color and a category color.
 * - Includes a 2 Hz pattern and a "double-flash then pause" pattern (120/120/120/640).
 * - One default context for your 4 labels; ask if you need multiple groups.
 */

#ifndef UI_ATTN_MAX_OBJS
#define UI_ATTN_MAX_OBJS 8
#endif

/* --- Categories & palette (edit colors here) --- */
typedef enum {
  UI_CAT_API,
  UI_CAT_SECONDS,
  UI_CAT_RASHI,
  UI_CAT_MANGALA
} ui_attn_cat_t;

static inline lv_color_t ui_attn_color(ui_attn_cat_t cat) {
  switch (cat) {
    case UI_CAT_API:      return lv_color_hex(0xFF3B30); // red
    case UI_CAT_SECONDS:  return lv_color_hex(0x007AFF); // blue
    case UI_CAT_RASHI:    return lv_color_hex(0x34C759); // green
    case UI_CAT_MANGALA:  return lv_color_hex(0xFF9500); // orange
    default:              return lv_color_hex(0xFFFFFF);
  }
}

/* --- Pattern definition ---
 * steps[]: durations in ms, alternating ON/OFF starting with ON.
 * Example: {250,250} = 2 Hz steady blink
 *          {120,120,120,640} = double-flash then pause
 */
typedef struct {
  const uint16_t* steps;
  uint8_t         step_count; // must be >= 2
} ui_attn_pattern_t;

/* Built-in patterns */
static const uint16_t UI_ATTN_2HZ_STEPS[]      = { 250, 250 };
static const uint16_t UI_ATTN_DOUBLE_STEPS[]   = { 120, 120, 120, 640 };

static const ui_attn_pattern_t UI_ATTN_2HZ     = { UI_ATTN_2HZ_STEPS,    2 };
static const ui_attn_pattern_t UI_ATTN_DOUBLE  = { UI_ATTN_DOUBLE_STEPS, 4 };

/* --- Internal state --- */
typedef struct {
  lv_obj_t*  obj;
  lv_color_t base;  // original text color
  lv_color_t hi;    // category color
} ui_attn_item_t;

typedef struct {
  ui_attn_item_t items[UI_ATTN_MAX_OBJS];
  uint8_t        count;
  uint8_t        step_idx;       // current step in pattern
  lv_timer_t*    timer;
  const ui_attn_pattern_t* pat;
} ui_attn_ctx_t;

static inline ui_attn_ctx_t& ui_attn_ctx() {
  static ui_attn_ctx_t ctx = {};
  return ctx;
}

/* Timer callback: advance pattern and set color */
static void ui_attn_cb(lv_timer_t* t) {
  ui_attn_ctx_t* ctx = (ui_attn_ctx_t*) lv_timer_get_user_data(t); 
  if (!ctx || !ctx->pat || ctx->pat->step_count < 2) return;

  /* ON for even step indices (0,2,4...), OFF for odd */
  const bool is_on = (ctx->step_idx % 2 == 0);

  for (uint8_t i = 0; i < ctx->count; ++i) {
    if (!ctx->items[i].obj) continue;
    const lv_color_t col = is_on ? ctx->items[i].hi : ctx->items[i].base;
    lv_obj_set_style_text_color(ctx->items[i].obj, col, LV_PART_MAIN);
  }

  /* advance & schedule next duration */
  ctx->step_idx = (ctx->step_idx + 1) % ctx->pat->step_count;
  lv_timer_set_period(ctx->timer, ctx->pat->steps[ctx->step_idx]);
}

/* Start flashing a list of (obj, category) with a pattern */
static inline void ui_attn_start(const lv_obj_t* const* objs,
                                 const ui_attn_cat_t* cats,
                                 uint8_t count,
                                 const ui_attn_pattern_t* pat) {
  ui_attn_ctx_t& ctx = ui_attn_ctx();

  if (ctx.timer) { lv_timer_del(ctx.timer); ctx.timer = nullptr; }

  ctx.count = (count > UI_ATTN_MAX_OBJS) ? UI_ATTN_MAX_OBJS : count;
  ctx.pat   = pat ? pat : &UI_ATTN_2HZ;
  ctx.step_idx = 0;

  for (uint8_t i = 0; i < ctx.count; ++i) {
    ctx.items[i].obj  = (lv_obj_t*)objs[i];
    if (ctx.items[i].obj) {
      ctx.items[i].base = lv_obj_get_style_text_color(ctx.items[i].obj, LV_PART_MAIN);
      ctx.items[i].hi   = ui_attn_color(cats[i]);
      /* initialize to OFF color so first step (ON) pops */
      lv_obj_set_style_text_color(ctx.items[i].obj, ctx.items[i].base, LV_PART_MAIN);
    }
  }

  /* start timer at first step duration */
ctx.timer = lv_timer_create(ui_attn_cb, ctx.pat->steps[0], &ctx);
}

/* Convenience for your four labels */
static inline void ui_attn_start4(lv_obj_t* apiLabel,
                                  lv_obj_t* secondsLabel,
                                  lv_obj_t* rashiLabel,
                                  lv_obj_t* mangalaLabel,
                                  const ui_attn_pattern_t* pat /*= &UI_ATTN_DOUBLE*/) {
  const lv_obj_t* objs[4] = { apiLabel, secondsLabel, rashiLabel, mangalaLabel };
  const ui_attn_cat_t cats[4] = { UI_CAT_API, UI_CAT_SECONDS, UI_CAT_RASHI, UI_CAT_MANGALA };
  ui_attn_start(objs, cats, 4, pat ? pat : &UI_ATTN_DOUBLE);
}

/* Stop and restore original colors */
static inline void ui_attn_stop() {
  ui_attn_ctx_t& ctx = ui_attn_ctx();
  if (ctx.timer) { lv_timer_del(ctx.timer); ctx.timer = nullptr; }
  for (uint8_t i = 0; i < ctx.count; ++i) {
    if (ctx.items[i].obj) {
      lv_obj_set_style_text_color(ctx.items[i].obj, ctx.items[i].base, LV_PART_MAIN);
    }
  }
  ctx.count = 0;
  ctx.step_idx = 0;
}

/* Apply category color once (no flashing) */
static inline void ui_attn_apply_once(lv_obj_t* obj, ui_attn_cat_t cat) {
  if (!obj) return;
  lv_obj_set_style_text_color(obj, ui_attn_color(cat), LV_PART_MAIN);
}
