#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"

extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

// Initialize highlight system (call after uiFacadeInit / ui_init)
void alertInit();

// Flash the category (LVGL thread only; use alertPostFlash from other tasks)
void alertFlash(NutClass cls, uint32_t ms = 900);

// Stop any ongoing flashes
void alertStopAll();

// Thread-safe posting from non-LVGL contexts (e.g., WebServer handlers)
void alertPostFlash(NutClass cls, uint32_t ms = 900);

// Call once per loop() on LVGL thread to apply posted flashes
void alertPoll();
