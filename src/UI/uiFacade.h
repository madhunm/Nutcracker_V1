#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"

extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

// ---------- init + immediate (LVGL-thread) setters ----------
void uiFacadeInit();
void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala);
void uiFacadeSetBatchResult(bool pass);
void uiFacadeClearBatchResult();

// ---------- cross-task posting (safe to call from WebServer handlers) ----------
void uiFacadePostPercentages(int api, int seconds, int rashi, int mangala);
void uiFacadePostBatchResult(bool pass);

// Apply posted updates on the LVGL thread (call from loop())
void uiFacadePoll();

// ---------- optional modals (no-ops if unused elsewhere) ----------
typedef void (*ResumeDecisionCb)(bool resumeYes);
void uiFacadeShowResumePrompt(int api, int seconds, int rashi, int mangala, ResumeDecisionCb cb);
void uiFacadeHideResumePrompt();

typedef void (*UnknownChoiceCb)(NutClass chosen);
void uiFacadeShowUnknownPrompt(UnknownChoiceCb cb);
void uiFacadeHideUnknownPrompt();
