#include "alertSystem.h"

// Per-category flash context
struct FlashCtx {
	lv_style_t			style;
	bool				styleInited = false;
	lv_obj_t*			target = nullptr;
	lv_timer_t*			timer = nullptr;
	uint16_t			ticksLeft = 0;
	uint16_t			periodMs = 0;
	bool				on = false;
	lv_color_t			hiColor;
};

// 4 contexts (Api, Seconds, Rashi, Mangala)
static FlashCtx ctxApi, ctxSeconds, ctxRashi, ctxMangala;

// Map NutClass -> target label + context + color
static bool pickCtx(NutClass c, FlashCtx*& ctx) {
	switch (c) {
		case NutClass::Api:
			ctx = &ctxApi;
			ctx->target = uic_apiPanelLabel ? uic_apiPanelLabel : uic_apiPercentageValueLabel;
			ctx->hiColor = lv_color_hex(0x4CAF50);	// green
			return ctx->target != nullptr;

		case NutClass::Seconds:
			ctx = &ctxSeconds;
			ctx->target = uic_secondsPanelLabel ? uic_secondsPanelLabel : uic_secondsPercentageValueLabel;
			ctx->hiColor = lv_color_hex(0x1976D2);	// blue
			return ctx->target != nullptr;

		case NutClass::Rashi:
			ctx = &ctxRashi;
			ctx->target = uic_rashiPanelLabel ? uic_rashiPanelLabel : uic_rashiPercentageValueLabel;
			ctx->hiColor = lv_color_hex(0xFF9800);	// orange
			return ctx->target != nullptr;

		case NutClass::Mangala:
			ctx = &ctxMangala;
			ctx->target = uic_mangalaPanelLabel ? uic_mangalaPanelLabel : uic_mangalaPercentageValueLabel;
			ctx->hiColor = lv_color_hex(0xD32F2F);	// red
			return ctx->target != nullptr;

		default:
			ctx = nullptr;
			return false;
	}
}

static void ensureStyle(FlashCtx* c) {
	if (c->styleInited) return;
	lv_style_init(&c->style);
	lv_style_set_bg_color(&c->style, c->hiColor);
	lv_style_set_bg_opa(&c->style, LV_OPA_90);	// mostly solid
	lv_style_set_radius(&c->style, 6);
	c->styleInited = true;
}

// Timer toggler
static void flashTick(lv_timer_t* t) {
	auto* c = (FlashCtx*) lv_timer_get_user_data(t);
	if (!c || !c->target) { lv_timer_del(t); return; }

	c->on = !c->on;
	if (c->on) {
		lv_obj_add_style(c->target, &c->style, LV_PART_MAIN);
	} else {
		lv_obj_remove_style(c->target, &c->style, LV_PART_MAIN);
	}
	if (c->target) lv_obj_invalidate(c->target);

	if (c->ticksLeft) {
		c->ticksLeft--;
	} else {
		// ensure off & stop
		lv_obj_remove_style(c->target, &c->style, LV_PART_MAIN);
		if (c->target) lv_obj_invalidate(c->target);
		lv_timer_del(c->timer);
		c->timer = nullptr;
		c->on = false;
	}
}

void alertInit() {
	// lazy styles; nothing to do now
}

void alertStopAll() {
	auto stop = [](FlashCtx& c){
		if (c.timer) { lv_timer_del(c.timer); c.timer = nullptr; }
		if (c.target && c.styleInited) lv_obj_remove_style(c.target, &c.style, LV_PART_MAIN);
		c.on = false;
	};
	stop(ctxApi);
	stop(ctxSeconds);
	stop(ctxRashi);
	stop(ctxMangala);
}

void alertFlash(NutClass cls, uint32_t ms) {
	FlashCtx* c = nullptr;
	if (!pickCtx(cls, c) || !c || !c->target) {
		Serial.printf("[ALERT] skip (no target) for cls=%d\n", (int)cls);
		return;
	}

	// configure period/ticks (double-blink-ish)
	const uint16_t period = 150;					// ms per toggle
	uint16_t toggles = (ms / period);
	if (toggles < 2) toggles = 2;

	// cancel ongoing
	if (c->timer) { lv_timer_del(c->timer); c->timer = nullptr; }
	if (c->styleInited) lv_obj_remove_style(c->target, &c->style, LV_PART_MAIN);
	c->on = false;

	ensureStyle(c);
	c->periodMs = period;
	c->ticksLeft = toggles;
	c->timer = lv_timer_create(flashTick, period, c);

	// immediate first "on"
	c->on = true;
	lv_obj_add_style(c->target, &c->style, LV_PART_MAIN);
	lv_obj_invalidate(c->target);

	Serial.printf("[ALERT] flash %d on obj=%p ticks=%u period=%u\n", (int)cls, (void*)c->target, (unsigned)c->ticksLeft, (unsigned)c->periodMs);
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
