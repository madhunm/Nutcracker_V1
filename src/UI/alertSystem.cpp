#include "alertSystem.h"
extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include "UI/ui_attention_multi.h"	// uiac_* API
	#include <lvgl.h>
}

/* one context per label */
static uiac_ctx_t attApi;
static uiac_ctx_t attSeconds;
static uiac_ctx_t attRashi;
static uiac_ctx_t attMangala;

static lv_timer_t* stopTimer = nullptr;

void alertInit() {
	uiac_init(&attApi);
	uiac_init(&attSeconds);
	uiac_init(&attRashi);
	uiac_init(&attMangala);
}

/* map class -> (ctx, obj, cat). We use ui_*PanelLabel handles per your UI. */
static uiac_ctx_t* pickCtx(NutClass c, lv_obj_t*& obj, uiac_cat_t& cat) {
	switch (c) {
		case NutClass::Api:
			obj = ui_apiPanelLabel;     cat = UIAC_CAT_API;     return &attApi;
		case NutClass::Seconds:
			obj = ui_secondsPanelLabel; cat = UIAC_CAT_SECONDS; return &attSeconds;
		case NutClass::Rashi:
			obj = ui_rashiPanelLabel;   cat = UIAC_CAT_RASHI;   return &attRashi;
		case NutClass::Mangala:
			obj = ui_mangalaPanelLabel; cat = UIAC_CAT_MANGALA; return &attMangala;
		default:
			obj = nullptr; return nullptr;
	}
}

static void stopCb(lv_timer_t* t) {
	auto* ctx = (uiac_ctx_t*) lv_timer_get_user_data(t);
	uiac_stop(ctx);
	lv_timer_del(t);
	if (t == stopTimer) stopTimer = nullptr;
}

void alertStopAll() {
	uiac_stop(&attApi);
	uiac_stop(&attSeconds);
	uiac_stop(&attRashi);
	uiac_stop(&attMangala);
	if (stopTimer) { lv_timer_del(stopTimer); stopTimer = nullptr; }
}

void alertFlash(NutClass cls, uint32_t ms) {
	lv_obj_t* obj = nullptr; uiac_cat_t cat = UIAC_CAT_API;
	uiac_ctx_t* ctx = pickCtx(cls, obj, cat);
	if (!ctx || !obj) return;

	if (stopTimer) { lv_timer_del(stopTimer); stopTimer = nullptr; }
	uiac_start(ctx, obj, cat);			// uses the double-flash pattern by default
	stopTimer = lv_timer_create(stopCb, ms, ctx);	// auto-stop after ms
}
