// main.cpp — DoIt ESP32 + TFT_eSPI + LVGL v9 (tabs, camelCase)
// Uses lv_tft_espi_create for the display, heap DMA buffer, SoftAP portal, demo session.

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include "fs/fsCompat.h"
#include <esp_heap_caps.h>

#include <lvgl.h>
#include <TFT_eSPI.h>

#include <ADS1220_WE.h>
#include <NS2009.h>

#include "net/webPortal.h"
#include "UI/ui.h"
#include "UI/alertSystem.h"

// -------- build switches --------
#define protoHasHardware	0	// set to 1 when ADC/touch are wired

// -------- ADS1220 pins (your mapping) --------
#define ADS1220_CS_PIN		10
#define ADS1220_DRDY_PIN	5

// -------- objects --------
ADS1220_WE ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
NS2009 ts;

// -------- LVGL display via lv_tft_espi_create --------
static lv_display_t* disp = nullptr;

static void lvglCreateDisplay() {
	// Match your panel/UI resolution/orientation
	const uint16_t hor = 240;	// change to 240 if your panel is 240x320 portrait
	const uint16_t ver = 320;	// change to 320 accordingly
	const uint32_t lines = 8;	// small partial buffer to save RAM

	const size_t bufPixels = hor * lines;
	lv_color_t* drawBuf = (lv_color_t*) heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
	if (!drawBuf) drawBuf = (lv_color_t*) malloc(bufPixels * sizeof(lv_color_t));	// fallback

	// Create LVGL display bound to TFT_eSPI (TFT_eSPI is initialized internally)
	disp = lv_tft_espi_create(hor, ver, drawBuf, bufPixels * sizeof(lv_color_t));
	// Rotate if needed (0/90/180/270). Adjust to your mounting.
	lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
	// If colors look swapped (BGR), enable LV_COLOR_16_SWAP 1 in lv_conf.h
}

// -------- demo session (so portal has files) --------
static void seedSampleSession() {
	if (!SPIFFS.exists("/sessions")) SPIFFS.mkdir("/sessions");
	String folder = "/sessions/demo_0001";
	if (!SPIFFS.exists(folder)) SPIFFS.mkdir(folder);

	// session.json
	{
		fs::File f = SPIFFS.open(folder + "/session.json", "w");
		if (f) {
			f.println("{\"start\":\"2025-11-07T12:00:00Z\",\"last\":2,"
			          "\"counts\":{\"Api\":1,\"Seconds\":1,\"Rashi\":0,\"Mangala\":0}}");
			f.close();
		}
	}
	// two tiny CSVs
	for (int i = 1; i <= 2; ++i) {
		char name[24];
		snprintf(name, sizeof(name), "/nut%02d.csv", i);
		fs::File f = SPIFFS.open(folder + String(name), "w");
		if (f) {
			f.println("t_ms,adc_code,baseline,pred_class,override_class");
			f.println("0,123,120,Api,");
			f.println("10,140,120,Api,");
			f.println("20,95,120,Api,");
			f.close();
		}
	}
}

// -------- optional: init when hardware arrives --------
static void initAds1220IfPresent() {
#if protoHasHardware
	SPI.begin();
	ads.setGain(ADS1220_WE_PGA_GAIN_128);
	ads.setDataRate(ADS1220_WE_DR_600SPS);
	ads.setVRefSource(ADS1220_WE_INTERNAL_REF_OFF);	// ratiometric (AVDD/AVSS)
	ads.start();
#endif
}

static void initNs2009IfPresent() {
#if protoHasHardware
	Wire.begin();
#endif
}

// -------- Arduino setup/loop --------
void setup() {
	Serial.begin(115200);
	delay(100);

	// LVGL
	lv_init();
	lvglCreateDisplay();

	// Build your generated UI (now it will render to the TFT)
	ui_init();
  alertInit();					// add this


	// Web portal
	webPortalBegin();
	seedSampleSession();

	// Optional peripherals
	initAds1220IfPresent();
	initNs2009IfPresent();

	Serial.println("[BOOT] TFT+LVGL ready. AP 'Areca-Classifier' → http://192.168.4.1/");
}

void loop() {
	// Advance LVGL tick and run its handler
	static uint32_t last = 0;
	uint32_t now = millis();
	lv_tick_inc(now - last);
	last = now;

	lv_timer_handler();
	delay(5);

	webPortalPoll();
}
