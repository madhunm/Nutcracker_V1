#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"	// NutClass
extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

// init + basic setters
void uiFacadeInit();
void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala);
void uiFacadeSetBatchResult(bool pass);
void uiFacadeClearBatchResult();

// ----- Resume prompt (tap gestures on full-screen overlay) -----
// single tap = yes (resume), double tap = no (start new)
typedef void (*ResumeDecisionCb)(bool resumeYes);
void uiFacadeShowResumePrompt(int api, int seconds, int rashi, int mangala, ResumeDecisionCb cb);
void uiFacadeHideResumePrompt();

// ----- Unknown-nut prompt (four explicit buttons) -----
typedef void (*UnknownChoiceCb)(NutClass chosen);
void uiFacadeShowUnknownPrompt(UnknownChoiceCb cb);
void uiFacadeHideUnknownPrompt();
