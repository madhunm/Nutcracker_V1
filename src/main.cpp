#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include "fs/fsCompat.h"

#include <lvgl.h>
#include <TFT_eSPI.h>

#include <ADS1220_WE.h>
#include <NS2009.h>

#include "net/webPortal.h"
#include "UI/ui.h"
#include "UI/ui_Screen1.h"
#include "UI/uiFacade.h"

// -------- ADS1220 pins (your mapping; unused in this stub) --------
#define ADS1220_CS_PIN		10
#define ADS1220_DRDY_PIN	5

// -------- objects (declared but not used until hardware arrives) --------
ADS1220_WE ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
NS2009 ts;

// -------- LVGL display via lv_tft_espi_create --------
static lv_display_t* disp = nullptr;

static void lvglCreateDisplay() {
	const uint16_t hor = 240;
	const uint16_t ver = 320;
	const uint32_t lines = 8;

	// 1) Initialize LVGL
	lv_init();

	// 2) Create a draw buffer
	const size_t bufPixels = hor * lines;
	lv_color_t* drawBuf = (lv_color_t*) heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
	if (!drawBuf) drawBuf = (lv_color_t*) malloc(bufPixels * sizeof(lv_color_t)); // fallback

	// 3) Create and rotate the TFT_eSPI display
	disp = lv_tft_espi_create(hor, ver, drawBuf, bufPixels * sizeof(lv_color_t));
	lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

	// 4) Make it the default so ui_init() uses it
	lv_display_set_default(disp);
}


// ---------- tiny helpers to probe for "open" session (no result.json) ----------
static bool readSessionJsonCounts(const String& dirPath, uint32_t& last, uint32_t& api, uint32_t& seconds, uint32_t& rashi, uint32_t& mangala) {
	fs::File f = FSYS.open(dirPath + "/session.json", "r");
	if (!f) return false;
	String js; js.reserve(512);
	while (f.available()) js += (char)f.read();
	f.close();

	auto grabNum = [&](const char* key, const char* parent) -> uint32_t {
		if (parent) {
			const int p0 = js.indexOf(String("\"") + parent + "\":");
			if (p0 < 0) return 0;
			const int p1 = js.indexOf(String("\"") + key + "\":", p0);
			if (p1 < 0) return 0;
			return (uint32_t) js.substring(p1 + strlen(key) + 3).toInt();
		} else {
			const int p = js.indexOf(String("\"") + key + "\":");
			if (p < 0) return 0;
			return (uint32_t) js.substring(p + strlen(key) + 3).toInt();
		}
	};
	last    = grabNum("last", nullptr);
	api     = grabNum("Api", "counts");
	seconds = grabNum("Seconds", "counts");
	rashi   = grabNum("Rashi", "counts");
	mangala = grabNum("Mangala", "counts");
	return true;
}

static bool findOpenSession(String& outPath, uint32_t& last, uint32_t& api, uint32_t& seconds, uint32_t& rashi, uint32_t& mangala) {
	outPath = "";
	last = api = seconds = rashi = mangala = 0;

	if (!FSYS.exists("/sessions")) return false;
	fs::File root = FSYS.open("/sessions");
	if (!root || !root.isDirectory()) return false;

	String best;
	for (fs::File e = root.openNextFile(); e; e = root.openNextFile()) {
		if (!e.isDirectory()) continue;
		String dir = String(e.path());
		if (!dir.startsWith("/")) dir = "/" + dir;
		// "open" if there is no result.json
		if (FSYS.exists(dir + "/result.json")) continue;

		if (best.isEmpty() || dir > best) best = dir;
	}
	if (best.isEmpty()) return false;

	if (!readSessionJsonCounts(best, last, api, seconds, rashi, mangala)) return false;
	outPath = best;
	return true;
}

static void markSessionClosed(const String& dirPath, uint32_t last, uint32_t api, uint32_t seconds, uint32_t rashi, uint32_t mangala) {
	fs::File f = FSYS.open(dirPath + "/session.json", "w");
	if (!f) return;
	f.print("{\"path\":\""); f.print(dirPath); f.print("\",");
	f.print("\"last\":"); f.print(last); f.print(",");
	f.print("\"closed\":true,");
	f.print("\"counts\":{");
	f.print("\"Api\":"); f.print(api); f.print(",");
	f.print("\"Seconds\":"); f.print(seconds); f.print(",");
	f.print("\"Rashi\":"); f.print(rashi); f.print(",");
	f.print("\"Mangala\":"); f.print(mangala); f.print("}}");
	f.close();
}

static void maybePromptResume() {
	String path;
	uint32_t last=0, a=0, s=0, r=0, m=0;
	if (!findOpenSession(path, last, a, s, r, m)) return;

	const uint32_t tot = a + s + r + m;
	int pa=0, ps=0, pr=0, pm=0;
	if (tot > 0) {
		pa = (int)((100.0f * a) / tot + 0.5f);
		ps = (int)((100.0f * s) / tot + 0.5f);
		pr = (int)((100.0f * r) / tot + 0.5f);
		pm = (int)((100.0f * m) / tot + 0.5f);
	}

	// keep values for lambda
	static String keptPath;
	static uint32_t keptLast, ka, ks, kr, km;
	keptPath = path; keptLast = last; ka=a; ks=s; kr=r; km=m;

	auto onDecision = [](bool resumeYes){
		if (resumeYes) {
			// Just reflect saved percentages on the TFT
			const uint32_t tot = ka + ks + kr + km;
			int pa=0, ps=0, pr=0, pm=0;
			if (tot > 0) {
				pa = (int)((100.0f * ka) / tot + 0.5f);
				ps = (int)((100.0f * ks) / tot + 0.5f);
				pr = (int)((100.0f * kr) / tot + 0.5f);
				pm = (int)((100.0f * km) / tot + 0.5f);
			}
			uiFacadeSetPercentages(pa, ps, pr, pm);
			Serial.printf("[RESUME] Resumed view from %s (last=%u)\n", keptPath.c_str(), (unsigned)keptLast);
		} else {
			// Mark closed and reset UI
			markSessionClosed(keptPath, keptLast, ka, ks, kr, km);
			uiFacadeClearBatchResult();
			uiFacadeSetPercentages(0,0,0,0);
			Serial.printf("[RESUME] Start new; marked closed: %s\n", keptPath.c_str());
		}
	};

	uiFacadeShowResumePrompt(pa, ps, pr, pm, onDecision);
}

void setup() {
	Serial.begin(115200);
	delay(50);

	if (!fsBegin(true)) Serial.println("[MAIN] FS mount failed");

	lvglCreateDisplay();
	ui_init();

	uiFacadeInit();

	// Prompt resume if an unfinished session exists (single/double tap)
	maybePromptResume();

	// Bring up web portal (SoftAP in webPortalBegin)
	webPortalBegin();
}

void loop() {
	lv_timer_handler();
	delay(5);
}
