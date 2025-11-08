#include "alertSystem.h"
extern "C" {
	#include "UI/ui_attention_multi.h"   // standard palette + double-flash + style restore
}

// One uiac context + one auto-stop timer per category (no cross-callbacks)
static uiac_ctx_t ctxApi, ctxSeconds, ctxRashi, ctxMangala;
static lv_timer_t* stopApi = nullptr;
static lv_timer_t* stopSeconds = nullptr;
static lv_timer_t* stopRashi = nullptr;
static lv_timer_t* stopMangala = nullptr;

static void cancelStop(lv_timer_t*& t) { if (t) { lv_timer_del(t); t = nullptr; } }

static void stopCb(lv_timer_t* t) {
	// The timer's user data points at the ctx we should stop
	auto* ctx = (uiac_ctx_t*) lv_timer_get_user_data(t);
	if (ctx) uiac_stop(ctx);         // restores base colours (bg, opa, text) per helper
	lv_timer_del(t);
}

void alertInit() {
	uiac_init(&ctxApi);
	uiac_init(&ctxSeconds);
	uiac_init(&ctxRashi);
	uiac_init(&ctxMangala);
}

// Pick the actual label to flash (fall back for Api/Rashi)
static bool mapTarget(NutClass c, uiac_ctx_t*& ctx, lv_obj_t*& obj, uiac_cat_t& cat, lv_timer_t*& stopper) {
	switch (c) {
		case NutClass::Api:
			ctx = &ctxApi;    cat = UIAC_CAT_API;     stopper = stopApi;
			obj = uic_apiPanelLabel ? uic_apiPanelLabel : uic_apiPercentageValueLabel;
			return obj != nullptr;
		case NutClass::Seconds:
			ctx = &ctxSeconds; cat = UIAC_CAT_SECONDS; stopper = stopSeconds;
			obj = uic_secondsPanelLabel ? uic_secondsPanelLabel : uic_secondsPercentageValueLabel;
			return obj != nullptr;
		case NutClass::Rashi:
			ctx = &ctxRashi;   cat = UIAC_CAT_RASHI;   stopper = stopRashi;
			obj = uic_rashiPanelLabel ? uic_rashiPanelLabel : uic_rashiPercentageValueLabel;
			return obj != nullptr;
		case NutClass::Mangala:
			ctx = &ctxMangala; cat = UIAC_CAT_MANGALA; stopper = stopMangala;
			obj = uic_mangalaPanelLabel ? uic_mangalaPanelLabel : uic_mangalaPercentageValueLabel;
			return obj != nullptr;
		default:
			return false;
	}
}

void alertStopAll() {
	uiac_stop(&ctxApi);     cancelStop(stopApi);
	uiac_stop(&ctxSeconds); cancelStop(stopSeconds);
	uiac_stop(&ctxRashi);   cancelStop(stopRashi);
	uiac_stop(&ctxMangala); cancelStop(stopMangala);
}

// LVGL-thread: start helper + schedule a one-shot stop after one full cycle (~1000 ms)
void alertFlash(NutClass cls) {
	uiac_ctx_t* ctx = nullptr; lv_obj_t* obj = nullptr; uiac_cat_t cat = UIAC_CAT_API; lv_timer_t*& stopper = stopApi;
	if (!mapTarget(cls, ctx, obj, cat, stopper)) {
		Serial.printf("[ALERT] skip (no target) for cls=%d\n", (int)cls);
		return;
	}

	// Stop only this category cleanly (don’t touch others)
	cancelStop(stopper);
	uiac_stop(ctx);                // ensure previous run is fully restored

	// Kick standardized double-flash on label (palette + pattern inside helper)
	uiac_start(ctx, obj, cat);     // helper saves base colours and starts its lv_timer. :contentReference[oaicite:3]{index=3}

	// Auto-stop after one pattern cycle: 120+120+120+640 = 1000 ms (single burst), then restore base
	stopper = lv_timer_create(stopCb, 1000, ctx);

	Serial.printf("[ALERT] uiac start cls=%d obj=%p\n", (int)cls, (void*)obj);
}

/* ---------- mailbox: post from web thread, run on LVGL thread ---------- */
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool pend = false;
static volatile NutClass pendCls = NutClass::Unknown;

void alertPostFlash(NutClass cls) {
	portENTER_CRITICAL(&mux);
	pend = true; pendCls = cls;
	portEXIT_CRITICAL(&mux);
}

void alertPoll() {
	bool doIt = false; NutClass c = NutClass::Unknown;
	portENTER_CRITICAL(&mux);
	if (pend) { doIt = true; c = pendCls; pend = false; }
	portEXIT_CRITICAL(&mux);
	if (doIt && c != NutClass::Unknown) alertFlash(c);
}
