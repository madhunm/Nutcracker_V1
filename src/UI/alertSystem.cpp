#include "alertSystem.h"

static uiac_ctx_t attApi, attSeconds, attRashi, attMangala;
static lv_timer_t* stopTimer = nullptr;

static bool pickTarget(NutClass c, uiac_ctx_t*& ctx, lv_obj_t*& obj, uiac_cat_t& cat) {
	switch (c) {
		case NutClass::Api:
			ctx = &attApi;
			obj = uic_apiPanelLabel ? uic_apiPanelLabel : uic_apiPercentageValueLabel;
			cat = UIAC_CAT_API;     return true;
		case NutClass::Seconds:
			ctx = &attSeconds;
			obj = uic_secondsPanelLabel ? uic_secondsPanelLabel : uic_secondsPercentageValueLabel;
			cat = UIAC_CAT_SECONDS; return true;
		case NutClass::Rashi:
			ctx = &attRashi;
			obj = uic_rashiPanelLabel ? uic_rashiPanelLabel : uic_rashiPercentageValueLabel;
			cat = UIAC_CAT_RASHI;   return true;
		case NutClass::Mangala:
			ctx = &attMangala;
			obj = uic_mangalaPanelLabel ? uic_mangalaPanelLabel : uic_mangalaPercentageValueLabel;
			cat = UIAC_CAT_MANGALA; return true;
		default:
			return false;
	}
}

void alertInit() {
	uiac_init(&attApi);
	uiac_init(&attSeconds);
	uiac_init(&attRashi);
	uiac_init(&attMangala);
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
	uiac_ctx_t* ctx = nullptr; lv_obj_t* obj = nullptr; uiac_cat_t cat = UIAC_CAT_API;
	if (!pickTarget(cls, ctx, obj, cat) || !obj || !ctx) return;
	if (stopTimer) { lv_timer_del(stopTimer); stopTimer = nullptr; }
	uiac_start(ctx, obj, cat);
	stopTimer = lv_timer_create(stopCb, ms, ctx);
}

/* ---------- mailbox so HTTP thread can request a flash ---------- */
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool pend = false;
static volatile NutClass pendCls = NutClass::Unknown;
static volatile uint32_t pendMs = 0;

void alertPostFlash(NutClass cls, uint32_t ms) {
	portENTER_CRITICAL(&mux);
	pend = true; pendCls = cls; pendMs = ms;
	portEXIT_CRITICAL(&mux);
}

void alertPoll() {
	bool doIt = false; NutClass c = NutClass::Unknown; uint32_t ms = 0;
	portENTER_CRITICAL(&mux);
	if (pend) { doIt = true; c = pendCls; ms = pendMs; pend = false; }
	portEXIT_CRITICAL(&mux);
	if (doIt) alertFlash(c, ms);
}
