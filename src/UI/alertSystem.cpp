#include "alertSystem.h"
extern "C" {
	#include "UI/ui_attention_multi.h"	// standardized colors + double-flash + restore
}

// One uiac context + one auto-stop timer per category
static uiac_ctx_t  ctxApi, ctxSeconds, ctxRashi, ctxMangala;
static lv_timer_t* stopApi     = nullptr;
static lv_timer_t* stopSeconds = nullptr;
static lv_timer_t* stopRashi   = nullptr;
static lv_timer_t* stopMangala = nullptr;

static inline void cancelStop(lv_timer_t** p) {
	if (p && *p) { lv_timer_del(*p); *p = nullptr; }
}

// IMPORTANT: clear the correct global stop* after the timer runs
static void clearStopByCtx(uiac_ctx_t* ctx) {
	if      (ctx == &ctxApi)     stopApi     = nullptr;
	else if (ctx == &ctxSeconds) stopSeconds = nullptr;
	else if (ctx == &ctxRashi)   stopRashi   = nullptr;
	else if (ctx == &ctxMangala) stopMangala = nullptr;
}

static void stopCb(lv_timer_t* t) {
	auto* ctx = (uiac_ctx_t*) lv_timer_get_user_data(t);
	if (ctx) {
		uiac_stop(ctx);			// helper restores base bg/opa/text
		clearStopByCtx(ctx);	// avoid dangling pointer on next flash
	}
	lv_timer_del(t);
}

void alertInit() {
	uiac_init(&ctxApi);
	uiac_init(&ctxSeconds);
	uiac_init(&ctxRashi);
	uiac_init(&ctxMangala);
}

// Pick the actual label to flash (fall back for Api/Rashi) + return address of the right stop pointer
static bool mapTarget(
	NutClass c,
	uiac_ctx_t** outCtx,
	lv_obj_t** outObj,
	uiac_cat_t* outCat,
	lv_timer_t*** outStopPtr   // <-- pointer-to-pointer to the correct global stopX
) {
	switch (c) {
		case NutClass::Api:
			*outCtx    = &ctxApi;
			*outObj    = uic_apiPanelLabel ? uic_apiPanelLabel : uic_apiPercentageValueLabel;
			*outCat    = UIAC_CAT_API;
			*outStopPtr = &stopApi;
			return *outObj != nullptr;

		case NutClass::Seconds:
			*outCtx    = &ctxSeconds;
			*outObj    = uic_secondsPanelLabel ? uic_secondsPanelLabel : uic_secondsPercentageValueLabel;
			*outCat    = UIAC_CAT_SECONDS;
			*outStopPtr = &stopSeconds;
			return *outObj != nullptr;

		case NutClass::Rashi:
			*outCtx    = &ctxRashi;
			*outObj    = uic_rashiPanelLabel ? uic_rashiPanelLabel : uic_rashiPercentageValueLabel;
			*outCat    = UIAC_CAT_RASHI;
			*outStopPtr = &stopRashi;
			return *outObj != nullptr;

		case NutClass::Mangala:
			*outCtx    = &ctxMangala;
			*outObj    = uic_mangalaPanelLabel ? uic_mangalaPanelLabel : uic_mangalaPercentageValueLabel;
			*outCat    = UIAC_CAT_MANGALA;
			*outStopPtr = &stopMangala;
			return *outObj != nullptr;

		default:
			return false;
	}
}

void alertStopAll() {
	cancelStop(&stopApi);     uiac_stop(&ctxApi);
	cancelStop(&stopSeconds); uiac_stop(&ctxSeconds);
	cancelStop(&stopRashi);   uiac_stop(&ctxRashi);
	cancelStop(&stopMangala); uiac_stop(&ctxMangala);
}

// LVGL-thread: start standardized double-flash, auto-stop after one cycle (~1000 ms)
void alertFlash(NutClass cls) {
	uiac_ctx_t* ctx = nullptr; lv_obj_t* obj = nullptr; uiac_cat_t cat = UIAC_CAT_API;
	lv_timer_t** stopPtr = nullptr;
	if (!mapTarget(cls, &ctx, &obj, &cat, &stopPtr)) {
		Serial.printf("[ALERT] skip (no target) for cls=%d\n", (int)cls);
		return;
	}

	// Cancel any pending stop for THIS category only, safely
	cancelStop(stopPtr);

	// Ensure previous run of THIS ctx is fully restored
	uiac_stop(ctx);

	// Start helper's standardized pattern on label (internally starts its own LVGL timer)
	uiac_start(ctx, obj, cat);

	// Schedule one-shot stop for THIS category only: 120+120+120+640 = 1000 ms
	*stopPtr = lv_timer_create(stopCb, 1000, ctx);

	Serial.printf("[ALERT] uiac start cls=%d obj=%p stop=%p\n", (int)cls, (void*)obj, (void*)*stopPtr);
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
