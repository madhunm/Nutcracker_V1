#include "uiFacade.h"

static inline lv_obj_t* pick(lv_obj_t* a, lv_obj_t* b) { return a ? a : b; }

/* Choose the label that actually belongs to the ACTIVE screen. 
   If one pointer is on the active screen, use it; otherwise fall back. */
static lv_obj_t* chooseForActive(lv_obj_t* uicPtr, lv_obj_t* uiPtr) {
	lv_obj_t* act = lv_screen_active();
	if (uicPtr && lv_obj_get_screen(uicPtr) == act) return uicPtr;
	if (uiPtr  && lv_obj_get_screen(uiPtr)  == act) return uiPtr;
	return uicPtr ? uicPtr : uiPtr;
}

static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", v);
	lv_label_set_text(lbl, buf);
}

// Optional tiny banner so you always see changes even if panels are hidden
static lv_obj_t* percentBanner = nullptr;

void uiFacadeInit() {
	// Create banner on the ACTIVE screen (kept small at top)
	if (!percentBanner) {
		percentBanner = lv_label_create(lv_screen_active());
		lv_obj_align(percentBanner, LV_ALIGN_TOP_MID, 0, 4);
		lv_obj_move_foreground(percentBanner);
		lv_label_set_text(percentBanner, "A:--%  S:--%  R:--%  M:--%");
	}

	Serial.printf("[UI] act=%p  uic_screen=%p  ui_screen=%p\n",
		(void*)lv_screen_active(), (void*)uic_Screen1, (void*)ui_Screen1);

	// Log which value labels are on the active screen
	lv_obj_t* a = chooseForActive(uic_apiPercentageValueLabel,     ui_apiPercentageValueLabel);
	lv_obj_t* s = chooseForActive(uic_secondsPercentageValueLabel, ui_secondsPercentageValueLabel);
	lv_obj_t* r = chooseForActive(uic_rashiPercentageValueLabel,   ui_rashiPercentageValueLabel);
	lv_obj_t* m = chooseForActive(uic_mangalaPercentageValueLabel, ui_mangalaPercentageValueLabel);
	Serial.printf("[UI] chosen  api=%p sec=%p rashi=%p mangala=%p\n", (void*)a,(void*)s,(void*)r,(void*)m);
}

void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala) {
	// Pick the labels that belong to the ACTIVE screen
	lv_obj_t* apiLbl     = chooseForActive(uic_apiPercentageValueLabel,     ui_apiPercentageValueLabel);
	lv_obj_t* secondsLbl = chooseForActive(uic_secondsPercentageValueLabel, ui_secondsPercentageValueLabel);
	lv_obj_t* rashiLbl   = chooseForActive(uic_rashiPercentageValueLabel,   ui_rashiPercentageValueLabel);
	lv_obj_t* mangalaLbl = chooseForActive(uic_mangalaPercentageValueLabel, ui_mangalaPercentageValueLabel);

	setLabelInt(apiLbl,     api);
	setLabelInt(secondsLbl, seconds);
	setLabelInt(rashiLbl,   rashi);
	setLabelInt(mangalaLbl, mangala);

	// Update banner too (always on active screen)
	if (percentBanner) {
		char line[64];
		snprintf(line, sizeof(line), "A:%d%%  S:%d%%  R:%d%%  M:%d%%", api, seconds, rashi, mangala);
		lv_label_set_text(percentBanner, line);
		lv_obj_invalidate(percentBanner);
	}

	if (apiLbl)     lv_obj_invalidate(apiLbl);
	if (secondsLbl) lv_obj_invalidate(secondsLbl);
	if (rashiLbl)   lv_obj_invalidate(rashiLbl);
	if (mangalaLbl) lv_obj_invalidate(mangalaLbl);
	if (lv_screen_active()) lv_obj_invalidate(lv_screen_active());

	#if LV_USE_REFR
	lv_refr_now(lv_display_get_default());
	#endif

	Serial.printf("[UI] set %% -> A:%d S:%d R:%d M:%d\n", api, seconds, rashi, mangala);
}

void uiFacadeSetBatchResult(bool pass) {
	// Choose the PASS/FAIL label that belongs to the ACTIVE screen
	lv_obj_t* lbl = chooseForActive(uic_batchResult, ui_batchResult);
	if (!lbl) return;

	lv_label_set_text(lbl, pass ? "PASS" : "FAIL");
	lv_obj_set_style_text_color(lbl,
		pass ? lv_color_hex(0x00C853) : lv_color_hex(0xD32F2F),
		LV_PART_MAIN);

	lv_obj_invalidate(lbl);
	if (lv_screen_active()) lv_obj_invalidate(lv_screen_active());
	#if LV_USE_REFR
	lv_refr_now(lv_display_get_default());
	#endif

	Serial.printf("[UI] batch result -> %s\n", pass ? "PASS" : "FAIL");
}

void uiFacadeClearBatchResult() {
	lv_obj_t* lbl = chooseForActive(uic_batchResult, ui_batchResult);
	if (!lbl) return;
	lv_label_set_text(lbl, "");
	lv_obj_invalidate(lbl);
	if (lv_screen_active()) lv_obj_invalidate(lv_screen_active());
	#if LV_USE_REFR
	lv_refr_now(lv_display_get_default());
	#endif
}

/* -------------------- mailbox for cross-task posts -------------------- */
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
	int a=0,s=0,r=0,m=0; bool pass=false;

	portENTER_CRITICAL(&uiMux);
	if (pendPerc) { a=pendA; s=pendS; r=pendR; m=pendM; pendPerc=false; doPerc=true; }
	if (pendBatch){ pass=pendBatchPass; pendBatch=false; doBatch=true; }
	portEXIT_CRITICAL(&uiMux);

	if (doPerc)  uiFacadeSetPercentages(a,s,r,m);
	if (doBatch) uiFacadeSetBatchResult(pass);
}

/* -------------------- Optional stubs -------------------- */
void uiFacadeShowResumePrompt(int, int, int, int, ResumeDecisionCb) {}
void uiFacadeHideResumePrompt() {}
void uiFacadeShowUnknownPrompt(UnknownChoiceCb) {}
void uiFacadeHideUnknownPrompt() {}
