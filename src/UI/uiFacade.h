#pragma once
#include <Arduino.h>
#include "app/sessionManager.h"

extern "C" {
	#include "UI/ui.h"
	#include "UI/ui_Screen1.h"
	#include <lvgl.h>
}

// init + immediate setters (LVGL thread only)
void uiFacadeInit();	// loads uic_Screen1 and logs pointers
void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala);
void uiFacadeSetBatchResult(bool pass);
void uiFacadeClearBatchResult();

// cross-task posting (safe from WebServer handlers)
void uiFacadePostPercentages(int api, int seconds, int rashi, int mangala);
void uiFacadePostBatchResult(bool pass);

// Apply posted updates on LVGL thread
void uiFacadePoll();

// Unknown category flow
typedef void (*UnknownCommitFn)(NutClass chosen);
void uiFacadeRegisterUnknownCommit(UnknownCommitFn fn);
void uiFacadePostShowUnknownPrompt();	// call from HTTP thread to show modal
