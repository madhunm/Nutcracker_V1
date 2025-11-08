#include "uiFacade.h"

static inline int clamp0_100(int v) { if (v < 0) return 0; if (v > 100) return 100; return v; }

static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8]; snprintf(buf, sizeof(buf), "%d", clamp0_100(v));
	lv_label_set_text(lbl, buf);
}

// ---- init ----
void uiFacadeInit() {
	// Force the uic_ screen to be the active one (you asked for uic_* labels)
	if (uic_Screen1) lv_screen_load(uic_Screen1);

	Serial.printf("[UI] uic screen=%p  api=%p sec=%p rashi=%p mangala=%p batch=%p\n",
		(void*)uic_Screen1,
		(void*)uic_apiPercentageValueLabel,
		(void*)uic_secondsPercentageValueLabel,
		(void*)uic_rashiPercentageValueLabel,
		(void*)uic_mangalaPercentageValueLabel,
		(void*)uic_batchResult
	);
}

// ---- immediate setters (LVGL thread) ----
void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala) {
	setLabelInt(uic_apiPercentageValueLabel,     api);
	setLabelInt(uic_secondsPercentageValueLabel, seconds);
	setLabelInt(uic_rashiPercentageValueLabel,   rashi);
	setLabelInt(uic_mangalaPercentageValueLabel, mangala);

	if (uic_apiPercentageValueLabel)     lv_obj_invalidate(uic_apiPercentageValueLabel);
	if (uic_secondsPercentageValueLabel) lv_obj_invalidate(uic_secondsPercentageValueLabel);
	if (uic_rashiPercentageValueLabel)   lv_obj_invalidate(uic_rashiPercentageValueLabel);
	if (uic_mangalaPercentageValueLabel) lv_obj_invalidate(uic_mangalaPercentageValueLabel);

	lv_obj_t* root = lv_screen_active();
	if (root) lv_obj_invalidate(root);
	lv_refr_now(lv_display_get_default());

	Serial.printf("[UI] set %% -> A:%d S:%d R:%d M:%d\n", api, seconds, rashi, mangala);
}

void uiFacadeSetBatchResult(bool pass) {
	if (!uic_batchResult) return;
	lv_label_set_text(uic_batchResult, pass ? "PASS" : "FAIL");
	lv_obj_set_style_text_color(
		uic_batchResult,
		pass ? lv_color_hex(0x00C853) : lv_color_hex(0xD32F2F),
		LV_PART_MAIN
	);
	lv_obj_invalidate(uic_batchResult);
	lv_obj_t* root = lv_screen_active();
	if (root) lv_obj_invalidate(root);
	lv_refr_now(lv_display_get_default());

	Serial.printf("[UI] batch result -> %s\n", pass ? "PASS" : "FAIL");
}

void uiFacadeClearBatchResult() {
	if (!uic_batchResult) return;
	lv_label_set_text(uic_batchResult, "");
	lv_obj_invalidate(uic_batchResult);
	lv_obj_t* root = lv_screen_active();
	if (root) lv_obj_invalidate(root);
	lv_refr_now(lv_display_get_default());
}

/* -------------------- mailbox (cross-task) -------------------- */
static portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;

static volatile bool pendPerc = false;
static volatile int  pendA = 0, pendS = 0, pendR = 0, pendM = 0;

static volatile bool pendBatch = false;
static volatile bool pendBatchPass = false;

static volatile bool pendUnknown = false;

void uiFacadePostPercentages(int api, int seconds, int rashi, int mangala) {
	portENTER_CRITICAL(&uiMux);
	pendA = api; pendS = seconds; pendR = rashi; pendM = mangala;
	pendPerc = true;
	portEXIT_CRITICAL(&uiMux);
}

void uiFacadePostBatchResult(bool pass) {
	portENTER_CRITICAL(&uiMux);
	pendBatchPass = pass;
	pendBatch = true;
	portEXIT_CRITICAL(&uiMux);
}

void uiFacadePostShowUnknownPrompt() {
	portENTER_CRITICAL(&uiMux);
	pendUnknown = true;
	portEXIT_CRITICAL(&uiMux);
}

/* -------------------- Unknown modal -------------------- */
static lv_obj_t* unknownOverlay = nullptr;
static UnknownCommitFn unknownCommit = nullptr;

void uiFacadeRegisterUnknownCommit(UnknownCommitFn fn) { unknownCommit = fn; }

static void unknownPick(lv_event_t* e) {
	if (!unknownOverlay) return;
	lv_obj_t* btn = (lv_obj_t*) lv_event_get_target(e);
	lv_obj_t* lbl = lv_obj_get_child(btn, 0);
	const char* txt = lbl ? lv_label_get_text(lbl) : "";
	NutClass chosen = NutClass::Unknown;
	if (txt) {
		if      (!strcmp(txt, "Api"))     chosen = NutClass::Api;
		else if (!strcmp(txt, "Seconds")) chosen = NutClass::Seconds;
		else if (!strcmp(txt, "Rashi"))   chosen = NutClass::Rashi;
		else if (!strcmp(txt, "Mangala")) chosen = NutClass::Mangala;
	}
	if (unknownCommit && chosen != NutClass::Unknown) unknownCommit(chosen);
	lv_obj_del(unknownOverlay); unknownOverlay = nullptr;
}

static void showUnknownNow() {
	if (unknownOverlay) lv_obj_del(unknownOverlay);

	unknownOverlay = lv_obj_create(lv_screen_active());
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
	makeBtn("Api"); makeBtn("Seconds"); makeBtn("Rashi"); makeBtn("Mangala");
}

void uiFacadePoll() {
	bool doPerc = false, doBatch = false, doUnknown = false;
	int a=0, s=0, r=0, m=0; bool pass=false;

	portENTER_CRITICAL(&uiMux);
	if (pendPerc)  { a=pendA; s=pendS; r=pendR; m=pendM; pendPerc=false;  doPerc=true; }
	if (pendBatch) { pass=pendBatchPass;           pendBatch=false; doBatch=true; }
	if (pendUnknown){                         pendUnknown=false; doUnknown=true; }
	portEXIT_CRITICAL(&uiMux);

	if (doPerc)  uiFacadeSetPercentages(a, s, r, m);
	if (doBatch) uiFacadeSetBatchResult(pass);
	if (doUnknown) showUnknownNow();
}
