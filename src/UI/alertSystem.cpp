#include "alertSystem.h"

// ===== Standardized attention pattern =====
// Burst A:  ON 160ms, OFF 120ms, ON 160ms, GAP 300ms
// Burst B:  ON 160ms, OFF 120ms, ON 160ms, END
static const uint16_t kDurMs[]  = {160, 120, 160, 300, 160, 120, 160, 0};
static const uint8_t  kState[]  = {  1,   0,   1,   0,   1,   0,   1, 0};
static const uint8_t  kPhaseMax = 7;

// Category colors during flash (WCAG-friendly)
static inline lv_color_t colorApi()     { return lv_color_hex(0x2E7D32); } // green 800
static inline lv_color_t colorSeconds() { return lv_color_hex(0x1565C0); } // blue 800
static inline lv_color_t colorRashi()   { return lv_color_hex(0xEF6C00); } // orange 800
static inline lv_color_t colorMangala() { return lv_color_hex(0xC62828); } // red 800
static inline lv_color_t colorTextOn()  { return lv_color_hex(0xFFFFFF); } // white

struct StyleSnapshot {
	lv_color_t	bgColor;
	lv_opa_t	bgOpa;
	lv_color_t	textColor;
	lv_coord_t	pad;		// restore symmetrically
	lv_coord_t	radius;
	bool		valid = false;
};

struct FlashCtx {
	lv_obj_t*	target = nullptr;	// uic_*PanelLabel (exact label)
	lv_timer_t*	timer  = nullptr;
	uint8_t		phase  = 0;
	bool		on     = false;

	StyleSnapshot orig;
	lv_color_t	onBg;
	lv_color_t	onText;
};

// One context per category
static FlashCtx ctxApi, ctxSeconds, ctxRashi, ctxMangala;

// Mailbox for cross-task posting
static portMUX_TYPE qMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool pend = false;
static volatile NutClass pendCls = NutClass::Unknown;

// ---- helpers ----
static void snapshotStyle(lv_obj_t* o, StyleSnapshot& s) {
	if (!o) { s.valid = false; return; }
	s.bgColor	= lv_obj_get_style_bg_color(o, LV_PART_MAIN);
	s.bgOpa		= lv_obj_get_style_bg_opa  (o, LV_PART_MAIN);
	s.textColor	= lv_obj_get_style_text_color(o, LV_PART_MAIN);
	lv_coord_t pL = lv_obj_get_style_pad_left  (o, LV_PART_MAIN);
	lv_coord_t pR = lv_obj_get_style_pad_right (o, LV_PART_MAIN);
	lv_coord_t pT = lv_obj_get_style_pad_top   (o, LV_PART_MAIN);
	lv_coord_t pB = lv_obj_get_style_pad_bottom(o, LV_PART_MAIN);
	s.pad		= (pL || pR || pT || pB) ? pL : 0;
	s.radius	= lv_obj_get_style_radius   (o, LV_PART_MAIN);
	s.valid		= true;
}

static void applyOn(FlashCtx* c, bool on) {
	if (!c || !c->target) return;

	if (on) {
		// Snapshot once on first ON
		if (!c->orig.valid) snapshotStyle(c->target, c->orig);

		// Apply alert colors + subtle chrome
		lv_obj_set_style_bg_color   (c->target, c->onBg,       0);
		lv_obj_set_style_bg_opa     (c->target, LV_OPA_COVER,  0);
		lv_obj_set_style_text_color (c->target, c->onText,     0);
		lv_obj_set_style_radius     (c->target, 8,             0);
		lv_obj_set_style_pad_all    (c->target, 4,             0);
	} else {
		// Restore exactly what we captured
		if (c->orig.valid) {
			lv_obj_set_style_bg_color   (c->target, c->orig.bgColor,   0);
			lv_obj_set_style_bg_opa     (c->target, c->orig.bgOpa,     0);
			lv_obj_set_style_text_color (c->target, c->orig.textColor, 0);
			lv_obj_set_style_radius     (c->target, c->orig.radius,    0);
			lv_obj_set_style_pad_all    (c->target, c->orig.pad,       0);
		} else {
			lv_obj_set_style_bg_opa     (c->target, LV_OPA_TRANSP,     0);
		}
	}
	lv_obj_invalidate(c->target);
}

static void tickPhase(lv_timer_t* t) {
	auto* c = (FlashCtx*) lv_timer_get_user_data(t);
	if (!c || !c->target) { lv_timer_del(t); return; }

	bool wantOn = kState[c->phase] != 0;
	if (wantOn != c->on) {
		c->on = wantOn;
		applyOn(c, c->on);
	}

	// End?
	if (kDurMs[c->phase + 1] == 0) {
		applyOn(c, false);		// ensure fully restored
		lv_timer_del(c->timer);
		c->timer = nullptr;
		c->phase = 0;
		c->on    = false;
		c->orig.valid = false;	// resnapshot next time
		return;
	}

	// Next phase
	c->phase++;
	lv_timer_set_period(t, kDurMs[c->phase]);
}

static FlashCtx* mapCtx(NutClass cls) {
	switch (cls) {
		case NutClass::Api:     return &ctxApi;
		case NutClass::Seconds: return &ctxSeconds;
		case NutClass::Rashi:   return &ctxRashi;
		case NutClass::Mangala: return &ctxMangala;
		default: return nullptr;
	}
}

static void bindTargetAndColors(FlashCtx* c, NutClass cls) {
	// exact labels you asked for
	switch (cls) {
		case NutClass::Api:
			c->target = uic_apiPanelLabel;
			c->onBg   = colorApi();     c->onText = colorTextOn();
			break;
		case NutClass::Seconds:
			c->target = uic_secondsPanelLabel;
			c->onBg   = colorSeconds(); c->onText = colorTextOn();
			break;
		case NutClass::Rashi:
			c->target = uic_rashiPanelLabel;
			c->onBg   = colorRashi();   c->onText = colorTextOn();
			break;
		case NutClass::Mangala:
			c->target = uic_mangalaPanelLabel;
			c->onBg   = colorMangala(); c->onText = colorTextOn();
			break;
		default:
			c->target = nullptr;
			break;
	}
}

static void stopOne(FlashCtx& c) {
	if (c.timer) { lv_timer_del(c.timer); c.timer = nullptr; }
	if (c.target) applyOn(&c, false);
	c.phase = 0; c.on = false; c.orig.valid = false;
}

static void stopAll() {
	stopOne(ctxApi); stopOne(ctxSeconds); stopOne(ctxRashi); stopOne(ctxMangala);
}

// ===== public API =====
void alertInit() {
	// nothing to do; contexts configured per flash
}

void alertStopAll() { stopAll(); }

void alertFlash(NutClass cls, uint32_t /*ms*/) {
	FlashCtx* c = mapCtx(cls);
	if (!c) return;

	// Rebind target (uic_*PanelLabel) & colors each time (in case UI reloaded)
	bindTargetAndColors(c, cls);
	if (!c->target) {
		Serial.printf("[ALERT] skip (no target label) for cls=%d\n", (int)cls);
		return;
	}

	// single visible flash at a time
	stopAll();

	// Start pattern
	c->phase = 0; c->on = false; c->orig.valid = false;
	if (c->timer) lv_timer_del(c->timer);
	c->timer = lv_timer_create(tickPhase, kDurMs[c->phase], c);

	// Kick first state immediately (so user sees it now, not after 160 ms)
	tickPhase(c->timer);

	Serial.printf("[ALERT] start cls=%d obj=%p\n", (int)cls, (void*)c->target);
}

// from other tasks (e.g. web server thread)
void alertPostFlash(NutClass cls, uint32_t /*ms*/) {
	portENTER_CRITICAL(&qMux);
	pend = true; pendCls = cls;
	portEXIT_CRITICAL(&qMux);
}

// LVGL-thread
void alertPoll() {
	bool doIt = false; NutClass c = NutClass::Unknown;
	portENTER_CRITICAL(&qMux);
	if (pend) { doIt = true; c = pendCls; pend = false; }
	portEXIT_CRITICAL(&qMux);
	if (doIt && c != NutClass::Unknown) alertFlash(c, 0);
}
