#pragma once
#include <Arduino.h>

void uiFacadeInit();	// call once after ui_init()

// Update percentage labels (integer %)
void uiFacadeSetPercentages(int api, int seconds, int rashi, int mangala);

// Show batch result badge at top center. Persists until next Start Session.
void uiFacadeSetBatchResult(bool pass);

void uiFacadeClearBatchResult();

