#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"
extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include "UI/ui_attention_multi.h"
	#include <lvgl.h>
}

void alertInit();								// call once after ui_init()/uiFacadeInit()
void alertFlash(NutClass cls, uint32_t ms=1000);	// start a flash for the category
void alertStopAll();							// stop all flashes

// post from non-LVGL tasks (e.g., HTTP handlers)
void alertPostFlash(NutClass cls, uint32_t ms=1000);
void alertPoll();								// call in loop()
