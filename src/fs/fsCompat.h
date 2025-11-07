#pragma once
#include <FS.h>
#include <LittleFS.h>

// Unified FS alias
#define FSYS LittleFS

inline bool fsBegin(bool formatOnFail = true) {
	return FSYS.begin(formatOnFail);
}
