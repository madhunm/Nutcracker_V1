#include "uiFacade.h"

static inline lv_obj_t* pick(lv_obj_t* a, lv_obj_t* b) { return a ? a : b; }

static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", v);
	lv_label_set_text(lbl, buf);
}

void uiFacadeInit() {
	Serial.printf("[UI] screen=%p api=%p sec=%p rashi=%p mangala=%p batch=%p\n",
		(void*)uic_Screen1,
		(void*)pick(uic_apiPercentageValueLabel,    ui_apiPercentageValueLabel),
		(void*)pick(uic_secondsPercentageValueLabel,ui_secondsPercentageValueLabel),
		(void*)pick(uic_rashiPercentageValueLabel,  ui_rashiPercentageValueLabel),
		(void*)pick(uic_mangalaPercentageValueLabel,ui_mangalaPercentageValueLabel),
		(void*)pick(uic_batchResult, ui_batchResult)
	);
}

void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala) {
	lv_obj_t* apiLbl     = pick(uic_apiPercentageValueLabel,     ui_apiPercentageValueLabel);
	lv_obj_t* secondsLbl = pick(uic_secondsPercentageValueLabel, ui_secondsPercentageValueLabel);
	lv_obj_t* rashiLbl   = pick(uic_rashiPercentageValueLabel,   ui_rashiPercentageValueLabel);
	lv_obj_t* mangalaLbl = pick(uic_mangalaPercentageValueLabel, ui_mangalaPercentageValueLabel);

	setLabelInt(apiLbl,     api);
	setLabelInt(secondsLbl, seconds);
	setLabelInt(rashiLbl,   rashi);
	setLabelInt(mangalaLbl, mangala);

	if (apiLbl)     lv_obj_invalidate(apiLbl);
	if (secondsLbl) lv_obj_invalidate(secondsLbl);
	if (rashiLbl)   lv_obj_invalidate(rashiLbl);
	if (mangalaLbl) lv_obj_invalidate(mangalaLbl);
	if (uic_Screen1) lv_obj_invalidate(uic_Screen1);

	#if LV_USE_REFR
	lv_refr_now(lv_display_get_default());
	#endif

	Serial.printf("[UI] set %% -> A:%d S:%d R:%d M:%d\n", api, seconds, rashi, mangala);
}

void uiFacadeSetBatchResult(bool pass) {
	lv_obj_t* lbl = pick(uic_batchResult, ui_batchResult);
	if (!lbl) return;

	lv_label_set_text(lbl, pass ? "PASS" : "FAIL");
	lv_obj_set_style_text_color(lbl,
		pass ? lv_color_hex(0x00C853) : lv_color_hex(0xD32F2F),
		LV_PART_MAIN);

	lv_obj_invalidate(lbl);
	if (uic_Screen1) lv_obj_invalidate(uic_Screen1);
	#if LV_USE_REFR
	lv_refr_now(lv_display_get_default());
	#endif

	Serial.printf("[UI] batch result -> %s\n", pass ? "PASS" : "FAIL");
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

/* -------------------- Optional modals (safe no-ops if you don't use them) -------------------- */
typedef void (*ResumeDecisionCb)(bool resumeYes);
void uiFacadeShowResumePrompt(int, int, int, int, ResumeDecisionCb) {}
void uiFacadeHideResumePrompt() {}

typedef void (*UnknownChoiceCb)(NutClass chosen);
void uiFacadeShowUnknownPrompt(UnknownChoiceCb) {}
void uiFacadeHideUnknownPrompt() {}
