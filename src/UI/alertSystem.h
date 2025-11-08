#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"

extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

// Initialize after ui_init()/uiFacadeInit()
void alertInit();

// Start a standardized class flash (double-burst); use alertPostFlash() from web handlers
void alertFlash(NutClass cls);

// Stop any ongoing flashes and restore original styles
void alertStopAll();

// Thread-safe posting from non-LVGL contexts (e.g., WebServer handlers)
void alertPostFlash(NutClass cls);

// Call once per loop() on the LVGL thread to apply posted flashes
void alertPoll();
