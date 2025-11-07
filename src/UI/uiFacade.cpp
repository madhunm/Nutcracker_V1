#include "uiFacade.h"
extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

static void setLabelInt(lv_obj_t* lbl, int v) {
	if (!lbl) return;
	char buf[8];
	snprintf(buf, sizeof(buf), "%d", v);
	lv_label_set_text(lbl, buf);
}

void uiFacadeInit() {
	// no-op for now
}

void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala) {
	setLabelInt(uic_apiPercentageValueLabel, api);
	setLabelInt(uic_secondsPercentageValueLabel, seconds);
	setLabelInt(uic_rashiPercentageValueLabel, rashi);
	setLabelInt(uic_mangalaPercentageValueLabel, mangala);
}

void uiFacadeSetBatchResult(bool pass) {
	// Prefer custom var if present; fall back to raw handle from generated UI
	lv_obj_t* lbl = uic_batchResult ? uic_batchResult : ui_batchResult; // both are exported. :contentReference[oaicite:1]{index=1}
	if (!lbl) return;

	// text: "pass" (green) or "fail" (red)
	lv_label_set_text(lbl, pass ? "PASS" : "FAIL");
	lv_obj_set_style_text_color(
		lbl,
		pass ? lv_color_hex(0x00C853) : lv_color_hex(0xD32F2F),
		LV_PART_MAIN
	);
}

void uiFacadeClearBatchResult() {
	lv_obj_t* lbl = uic_batchResult ? uic_batchResult : ui_batchResult;
	if (!lbl) return;
	lv_label_set_text(lbl, "");
}
