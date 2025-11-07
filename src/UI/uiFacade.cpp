#include "uiFacade.h"

// ---------- small helpers ----------
static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", v);
	lv_label_set_text(lbl, buf);
}

// ---------- public basics ----------
void uiFacadeInit() {
	// no-op: resume/unknown modals handle their own tap events
}

void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala) {
	setLabelInt(uic_apiPercentageValueLabel, api);
	setLabelInt(uic_secondsPercentageValueLabel, seconds);
	setLabelInt(uic_rashiPercentageValueLabel, rashi);
	setLabelInt(uic_mangalaPercentageValueLabel, mangala);
}

void uiFacadeSetBatchResult(bool pass) {
	lv_obj_t* lbl = uic_batchResult ? uic_batchResult : ui_batchResult;
	if (!lbl) return;
	lv_label_set_text(lbl, pass ? "PASS" : "FAIL");
	lv_obj_set_style_text_color(lbl,
		pass ? lv_color_hex(0x00C853) : lv_color_hex(0xD32F2F),
		LV_PART_MAIN);
}

void uiFacadeClearBatchResult() {
	lv_obj_t* lbl = uic_batchResult ? uic_batchResult : ui_batchResult;
	if (!lbl) return;
	lv_label_set_text(lbl, "");
}

// =====================================================================
//							RESUME PROMPT (single/double tap)
// =====================================================================
static lv_obj_t* resumeOverlay = nullptr;
static lv_obj_t* resumeCard = nullptr;
static lv_obj_t* resumeStats = nullptr;
static ResumeDecisionCb resumeCb = nullptr;

static uint8_t resumeTapCount = 0;
static lv_timer_t* resumeTapTimer = nullptr;
static const uint32_t RESUME_TAP_WINDOW_MS = 300;

static void resumeFinish(bool yes) {
	if (resumeCb) resumeCb(yes);
	if (resumeOverlay) lv_obj_del(resumeOverlay);
	resumeOverlay = resumeCard = resumeStats = nullptr;
	resumeCb = nullptr;
	resumeTapCount = 0;
	if (resumeTapTimer) { lv_timer_del(resumeTapTimer); resumeTapTimer = nullptr; }
}

static void resumeTapTimerCb(lv_timer_t* t) {
	LV_UNUSED(t);
	// single tap expired -> YES
	resumeFinish(true);
}

static void resumeOnClicked(lv_event_t* e) {
	LV_UNUSED(e);
	if (!resumeOverlay) return;
	resumeTapCount++;
	if (resumeTapCount == 1) {
		if (resumeTapTimer) lv_timer_del(resumeTapTimer);
		resumeTapTimer = lv_timer_create(resumeTapTimerCb, RESUME_TAP_WINDOW_MS, nullptr);
	} else if (resumeTapCount == 2) {
		if (resumeTapTimer) { lv_timer_del(resumeTapTimer); resumeTapTimer = nullptr; }
		resumeFinish(false);	// double tap => Start New (No)
	}
}

void uiFacadeShowResumePrompt(int api, int seconds, int rashi, int mangala, ResumeDecisionCb cb) {
	// clean previous
	uiFacadeHideResumePrompt();
	resumeCb = cb;

	// overlay (captures taps)
	resumeOverlay = lv_obj_create(ui_Screen1);
	lv_obj_remove_style_all(resumeOverlay);
	lv_obj_set_size(resumeOverlay, LV_PCT(100), LV_PCT(100));
	lv_obj_set_style_bg_opa(resumeOverlay, LV_OPA_60, 0);
	lv_obj_set_style_bg_color(resumeOverlay, lv_color_hex(0x000000), 0);
	lv_obj_add_event_cb(resumeOverlay, resumeOnClicked, LV_EVENT_CLICKED, nullptr);

	// card
	resumeCard = lv_obj_create(resumeOverlay);
	lv_obj_set_size(resumeCard, LV_PCT(82), LV_SIZE_CONTENT);
	lv_obj_set_style_pad_all(resumeCard, 12, 0);
	lv_obj_set_style_radius(resumeCard, 10, 0);
	lv_obj_align(resumeCard, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t* title = lv_label_create(resumeCard);
	lv_label_set_text(title, "Resume previous session?");
	lv_obj_set_style_text_font(title, lv_theme_get_font_large(title), 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

	char stats[128];
	snprintf(stats, sizeof(stats), "Api %d%%  •  Seconds %d%%  •  Rashi %d%%  •  Mangala %d%%",
	         api, seconds, rashi, mangala);
	resumeStats = lv_label_create(resumeCard);
	lv_label_set_text(resumeStats, stats);
	lv_obj_align(resumeStats, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t* hint = lv_label_create(resumeCard);
	lv_label_set_text(hint, "Tap once to Resume • Tap twice to Start New");
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void uiFacadeHideResumePrompt() {
	if (resumeOverlay) lv_obj_del(resumeOverlay);
	resumeOverlay = resumeCard = resumeStats = nullptr;
	resumeCb = nullptr;
	resumeTapCount = 0;
	if (resumeTapTimer) { lv_timer_del(resumeTapTimer); resumeTapTimer = nullptr; }
}

// =====================================================================
//							UNKNOWN PROMPT
// =====================================================================
static lv_obj_t* unknownOverlay = nullptr;
static UnknownChoiceCb unknownCb = nullptr;

static void unknownPick(lv_event_t* e) {
	if (!unknownOverlay) return;
	lv_obj_t* btn = (lv_obj_t*) lv_event_get_target(e); // v9 returns void*
	NutClass chosen = NutClass::Unknown;

	// first child is the label
	lv_obj_t* lbl = lv_obj_get_child(btn, 0);
	const char* txt = lbl ? lv_label_get_text(lbl) : "";

	if      (txt && strcmp(txt, "Api") == 0)      chosen = NutClass::Api;
	else if (txt && strcmp(txt, "Seconds") == 0)  chosen = NutClass::Seconds;
	else if (txt && strcmp(txt, "Rashi") == 0)    chosen = NutClass::Rashi;
	else if (txt && strcmp(txt, "Mangala") == 0)  chosen = NutClass::Mangala;

	if (unknownCb) unknownCb(chosen);
	lv_obj_del(unknownOverlay);
	unknownOverlay = nullptr;
	unknownCb = nullptr;
}

void uiFacadeShowUnknownPrompt(UnknownChoiceCb cb) {
	uiFacadeHideUnknownPrompt();
	unknownCb = cb;

	unknownOverlay = lv_obj_create(ui_Screen1);
	lv_obj_remove_style_all(unknownOverlay);
	lv_obj_set_size(unknownOverlay, LV_PCT(100), LV_PCT(100));
	lv_obj_set_style_bg_opa(unknownOverlay, LV_OPA_60, 0);
	lv_obj_set_style_bg_color(unknownOverlay, lv_color_hex(0x000000), 0);

	lv_obj_t* card = lv_obj_create(unknownOverlay);
	lv_obj_set_size(card, LV_PCT(90), LV_SIZE_CONTENT);
	lv_obj_set_style_pad_all(card, 12, 0);
	lv_obj_set_style_radius(card, 10, 0);
	lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);

	lv_obj_t* title = lv_label_create(card);
	lv_label_set_text(title, "Select category for this nut");
	lv_obj_set_style_text_font(title, lv_theme_get_font_large(title), 0);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

	lv_obj_t* grid = lv_obj_create(card);
	lv_obj_remove_style_all(grid);
	lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_style_pad_gap(grid, 8, 0);
	lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);

	auto makeBtn = [&](const char* label){
		lv_obj_t* b = lv_button_create(grid);
		lv_obj_set_size(b, LV_PCT(48), LV_SIZE_CONTENT);
		lv_obj_add_event_cb(b, unknownPick, LV_EVENT_CLICKED, nullptr);
		lv_obj_t* l = lv_label_create(b);
		lv_label_set_text(l, label);
	};
	makeBtn("Api");
	makeBtn("Seconds");
	makeBtn("Rashi");
	makeBtn("Mangala");
}

void uiFacadeHideUnknownPrompt() {
	if (unknownOverlay) lv_obj_del(unknownOverlay);
	unknownOverlay = nullptr;
	unknownCb = nullptr;
}
