#pragma once
#include <lvgl.h>

/* Per-label color alert (LVGL v9)
 * - Double-flash pattern only: 120,120,120,640 ms (ON/OFF/ON/PAUSE)
 * - Changes BG + text color during ON step
 * - Restores original BG color/opa + text color when stopped
 * - Multi-context: one context per label/group
 * - No font size or opacity changes
 */

#ifndef UIAC_MAX_OBJS
#define UIAC_MAX_OBJS 1   // single-label contexts; bump if you group items
#endif

/* ===== Categories ===== */
typedef enum {
  UIAC_CAT_API,
  UIAC_CAT_SECONDS,
  UIAC_CAT_RASHI,
  UIAC_CAT_MANGALA
} uiac_cat_t;

/* ===== Distinct palette (dark, high-contrast) ===== */
static inline lv_color_t uiac_bg(uiac_cat_t cat) {
  switch (cat) {
    case UIAC_CAT_API:     return lv_color_hex(0xC62828); // red
    case UIAC_CAT_SECONDS: return lv_color_hex(0x1565C0); // blue
    case UIAC_CAT_RASHI:   return lv_color_hex(0x2E7D32); // green
    case UIAC_CAT_MANGALA: return lv_color_hex(0xEF6C00); // orange
    default:               return lv_color_hex(0x000000);
  }
}
static inline lv_color_t uiac_text_on_bg(uiac_cat_t /*cat*/) {
  return lv_color_white();     // white text on those dark backgrounds
}

/* ===== Double-flash pattern (ms) =====
 * Steps alternate ON/OFF starting with ON.
 */
static const uint16_t UIAC_DOUBLE_STEPS[] = { 120, 120, 120, 640 };

/* ===== Storage ===== */
typedef struct {
  lv_obj_t*  obj;

  // base (restore) style
  lv_color_t base_bg;
  lv_opa_t   base_bg_opa;
  lv_color_t base_text;

  // highlight style
  lv_color_t hi_bg;
  lv_color_t hi_text;
} uiac_item_t;

typedef struct {
  uiac_item_t item;           // one object per context
  uint8_t     step_idx;       // 0..3
  lv_timer_t* timer;
} uiac_ctx_t;

/* ===== Internals ===== */
static inline void _uiac_paint(uiac_ctx_t* ctx, bool on) {
  uiac_item_t* it = &ctx->item;
  if (!it->obj) return;

  if (on) {
    lv_obj_set_style_bg_color(it->obj, it->hi_bg,   LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (it->obj, LV_OPA_COVER, LV_PART_MAIN); // ensure bg shows
    lv_obj_set_style_text_color(it->obj, it->hi_text, LV_PART_MAIN);
  } else {
    lv_obj_set_style_bg_color(it->obj, it->base_bg,    LV_PART_MAIN);
    lv_obj_set_style_bg_opa  (it->obj, it->base_bg_opa,LV_PART_MAIN);
    lv_obj_set_style_text_color(it->obj, it->base_text, LV_PART_MAIN);
  }
}

static void _uiac_cb(lv_timer_t* t) {
  auto* ctx = (uiac_ctx_t*) lv_timer_get_user_data(t);   // v9-safe
  if (!ctx) return;

  const bool is_on = (ctx->step_idx % 2) == 0;           // 0/2 ON, 1/3 OFF
  _uiac_paint(ctx, is_on);

  ctx->step_idx = (ctx->step_idx + 1) & 3;               // wrap 0..3
  lv_timer_set_period(t, UIAC_DOUBLE_STEPS[ctx->step_idx]);
}

static void _uiac_on_deleted(lv_event_t* e) {
  auto* ctx  = (uiac_ctx_t*) lv_event_get_user_data(e);
  auto* dead = (lv_obj_t*) lv_event_get_target(e);
  if (!ctx || ctx->item.obj != dead) return;
  if (ctx->timer) { lv_timer_del(ctx->timer); ctx->timer = nullptr; }
  ctx->item.obj = nullptr;
}

/* ===== Public API ===== */
static inline void uiac_init(uiac_ctx_t* ctx) {
  ctx->item.obj = nullptr;
  ctx->step_idx = 0;
  ctx->timer    = nullptr;
}

/* Start a color alert on one label */
static inline void uiac_start(uiac_ctx_t* ctx, lv_obj_t* obj, uiac_cat_t cat) {
  if (!ctx || !obj) return;

  // stop any running timer
  if (ctx->timer) { lv_timer_del(ctx->timer); ctx->timer = nullptr; }

  // snapshot base style
  ctx->item.obj        = obj;
  ctx->item.base_bg    = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
  ctx->item.base_bg_opa= lv_obj_get_style_bg_opa  (obj, LV_PART_MAIN);
  ctx->item.base_text  = lv_obj_get_style_text_color(obj, LV_PART_MAIN);

  // set highlight colors
  ctx->item.hi_bg   = uiac_bg(cat);
  ctx->item.hi_text = uiac_text_on_bg(cat);

  // start from OFF so the first ON pops
  ctx->step_idx = 0;
  _uiac_paint(ctx, false);

  // auto-stop if the object is deleted
  lv_obj_add_event_cb(obj, _uiac_on_deleted, LV_EVENT_DELETE, ctx);

  // kick the timer with the first step duration
  ctx->timer = lv_timer_create(_uiac_cb, UIAC_DOUBLE_STEPS[0], ctx);
}

/* Stop and restore base colors */
static inline void uiac_stop(uiac_ctx_t* ctx) {
  if (!ctx) return;
  if (ctx->timer) { lv_timer_del(ctx->timer); ctx->timer = nullptr; }
  _uiac_paint(ctx, false);        // restore once
  ctx->item.obj = nullptr;
  ctx->step_idx = 0;
}

/* Apply category colors once (no flashing) */
static inline void uiac_apply_once(lv_obj_t* obj, uiac_cat_t cat) {
  if (!obj) return;
  lv_obj_set_style_bg_color (obj, uiac_bg(cat), LV_PART_MAIN);
  lv_obj_set_style_bg_opa   (obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(obj, uiac_text_on_bg(cat), LV_PART_MAIN);
}
