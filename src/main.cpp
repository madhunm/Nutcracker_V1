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
#include "UI/ui_Screen1.h"
#include "UI/uiFacade.h"

// -------- ADS1220 pins (kept for completeness) --------
#define ADS1220_CS_PIN		10
#define ADS1220_DRDY_PIN	5

ADS1220_WE ads(ADS1220_CS_PIN, ADS1220_DRDY_PIN);
NS2009 ts;

static lv_display_t* disp = nullptr;

static void lvglCreateDisplay() {
	const uint16_t hor = 240;
	const uint16_t ver = 320;
	const uint32_t lines = 8;

	lv_init();	// IMPORTANT

	const size_t bufPixels = hor * lines;
	lv_color_t* drawBuf = (lv_color_t*) heap_caps_malloc(bufPixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
	if (!drawBuf) drawBuf = (lv_color_t*) malloc(bufPixels * sizeof(lv_color_t));

	disp = lv_tft_espi_create(hor, ver, drawBuf, bufPixels * sizeof(lv_color_t));
	lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
	lv_display_set_default(disp);	// IMPORTANT
}

void setup() {
	Serial.begin(115200);
	delay(50);

	if (!fsBegin(true)) Serial.println("[MAIN] FS mount failed");

	lvglCreateDisplay();
	ui_init();				// build the SquareLine UI on the default display
	uiFacadeInit();

	// ensure our screen is active (in case export didn't load it)
	extern lv_obj_t* ui_Screen1;
	if (ui_Screen1) lv_screen_load(ui_Screen1);

	webPortalBegin();
}

void loop() {
	lv_timer_handler();
	uiFacadePoll();		// apply posted UI changes on LVGL thread
	webPortalPoll();
	delay(5);
}
