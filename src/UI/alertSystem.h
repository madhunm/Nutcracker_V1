#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"	// for NutClass

void alertInit();								// call once after ui_init()
void alertFlash(NutClass cls, uint32_t ms=1000);	// flash the category label
void alertStopAll();							// stop any running flashes
