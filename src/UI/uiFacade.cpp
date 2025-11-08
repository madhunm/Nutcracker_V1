#include "uiFacade.h"

// -------- helper --------
static inline int clamp0_100(int v) {
	if (v < 0) return 0;
	if (v > 100) return 100;
	return v;
}

static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", clamp0_100(v));
	lv_label_set_text(lbl, buf);
}

// -------- init --------
void uiFacadeInit() {
	// Ensure the *uic* screen is the active one (per your requirement)
	if (uic_Screen1) {
		lv_screen_load(uic_Screen1);
	}

	Serial.printf("[UI] uic screen=%p  api=%p sec=%p rashi=%p mangala=%p batch=%p\n",
		(void*)uic_Screen1,
		(void*)uic_apiPercentageValueLabel,
		(void*)uic_secondsPercentageValueLabel,
		(void*)uic_rashiPercentageValueLabel,
		(void*)uic_mangalaPercentageValueLabel,
		(void*)uic_batchResult
	);
}

// -------- immediate setters (must be called on LVGL thread) --------
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

// -------- mailbox for cross-task posting --------
static portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;

static volatile bool pendPerc = false;
static volatile int  pendA = 0, pendS = 0, pendR = 0, pendM = 0;

static volatile bool pendBatch = false;
static volatile bool pendBatchPass = false;

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

void uiFacadePoll() {
	bool doPerc = false, doBatch = false;
	int a=0, s=0, r=0, m=0; bool pass=false;

	portENTER_CRITICAL(&uiMux);
	if (pendPerc)  { a=pendA; s=pendS; r=pendR; m=pendM; pendPerc=false;  doPerc=true; }
	if (pendBatch) { pass=pendBatchPass;            pendBatch=false; doBatch=true; }
	portEXIT_CRITICAL(&uiMux);

	if (doPerc)  uiFacadeSetPercentages(a, s, r, m);
	if (doBatch) uiFacadeSetBatchResult(pass);
}
