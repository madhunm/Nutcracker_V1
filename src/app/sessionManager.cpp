#include "sessionManager.h"
#include "fs/fsCompat.h"
#include <FS.h>
#include <time.h>

static const char* sessionsDir = "/sessions";

static String two(uint32_t v)  { char b[3];  snprintf(b, sizeof(b), "%02u", (unsigned)v); return String(b); }
static String four(uint32_t v) { char b[5];  snprintf(b, sizeof(b), "%04u", (unsigned)v); return String(b); }

void SessionManager::begin() {
	if (!FSYS.exists(sessionsDir)) FSYS.mkdir(sessionsDir);
}

bool SessionManager::startSession() {
	_counts = ClassCounts{};
	_lastIndex = 0;
	_open = false;

	if (!FSYS.exists(sessionsDir)) {
		if (!FSYS.mkdir(sessionsDir)) {
			Serial.println("[SESSION] mkdir /sessions failed");
			return false;
		}
	}

	time_t now = time(nullptr);
	struct tm t; gmtime_r(&now, &t);
	// fallback if time not set
	if (t.tm_year < 120) { t.tm_year = 125; t.tm_mon = 10; t.tm_mday = 7; t.tm_hour = 12; t.tm_min = 0; t.tm_sec = 0; }

	String base = String(sessionsDir) + "/"
		+ four(t.tm_year + 1900) + two(t.tm_mon + 1) + two(t.tm_mday)
		+ "_" + two(t.tm_hour) + two(t.tm_min) + two(t.tm_sec);

	// ensure uniqueness if same timestamp
	String path = base;
	for (int n = 1; FSYS.exists(path) && n <= 99; ++n) {
		char suf[5]; snprintf(suf, sizeof(suf), "-%02d", n);
		path = base + String(suf);
	}
	if (FSYS.exists(path)) {
		Serial.printf("[SESSION] path still exists after suffixing: %s\n", path.c_str());
		return false;
	}
	if (!FSYS.mkdir(path)) {
		Serial.printf("[SESSION] mkdir failed: %s\n", path.c_str());
		return false;
	}

	_sessionPath = path;
	_open = true;
	return writeSessionJson();
}

bool SessionManager::endSession() {
	_open = false;
	return true;
}

bool SessionManager::addSimulatedNut(NutClass cls) {
	if (!_open) {
		if (!startSession()) return false;
	}
	_lastIndex++;
	switch (cls) {
		case NutClass::Api:		_counts.api++;		break;
		case NutClass::Seconds:	_counts.seconds++;	break;
		case NutClass::Rashi:	_counts.rashi++;	break;
		case NutClass::Mangala:	_counts.mangala++;	break;
		default: /* Unknown */	break;
	}
	if (!writeCsvForNut(_lastIndex, cls)) return false;
	return writeSessionJson();
}

bool SessionManager::writeCsvForNut(uint32_t idx, NutClass cls) {
	if (!_open) return false;

	char fileName[32];
	snprintf(fileName, sizeof(fileName), "/nut%05u.csv", (unsigned)idx);
	String path = _sessionPath + String(fileName);

	fs::File f = FSYS.open(path, "w");
	if (!f) {
		Serial.printf("[SESSION] open file failed: %s\n", path.c_str());
		return false;
	}

	// Minimal synthetic trace so you can download something real
	f.println("t_ms,adc_code,baseline,pred_class,override_class");
	for (int i = 0; i <= 20; ++i) {
		int code = 100 + (i <= 10 ? i * 5 : (20 - i) * 5);
		f.printf("%d,%d,%d,%s,\n", i * 10, code, 100, className(cls));
	}
	f.close();
	return true;
}

bool SessionManager::writeSessionJson() {
	if (!_open) return false;
	fs::File f = FSYS.open(_sessionPath + "/session.json", "w");
	if (!f) {
		Serial.printf("[SESSION] open session.json failed in %s\n", _sessionPath.c_str());
		return false;
	}

	f.print("{\"path\":\"");		f.print(_sessionPath);		f.print("\",");
	f.print("\"last\":");			f.print(_lastIndex);		f.print(",");
	f.print("\"counts\":{");
	f.print("\"Api\":");			f.print(_counts.api);		f.print(",");
	f.print("\"Seconds\":");		f.print(_counts.seconds);	f.print(",");
	f.print("\"Rashi\":");			f.print(_counts.rashi);		f.print(",");
	f.print("\"Mangala\":");		f.print(_counts.mangala);	f.print("}}");
	f.close();
	return true;
}

void SessionManager::getPercentages(float &api, float &seconds, float &rashi, float &mangala) const {
	const float tot = (float)_counts.total();
	if (tot <= 0.0f) { api = seconds = rashi = mangala = 0.0f; return; }
	api		= 100.0f * _counts.api		/ tot;
	seconds	= 100.0f * _counts.seconds	/ tot;
	rashi	= 100.0f * _counts.rashi	/ tot;
	mangala	= 100.0f * _counts.mangala	/ tot;
}

NutClass SessionManager::parseClass(const String &s) {
	if (s.equalsIgnoreCase("Api"))		return NutClass::Api;
	if (s.equalsIgnoreCase("Seconds"))	return NutClass::Seconds;
	if (s.equalsIgnoreCase("Rashi"))	return NutClass::Rashi;
	if (s.equalsIgnoreCase("Mangala"))	return NutClass::Mangala;
	return NutClass::Unknown;
}

const char* SessionManager::className(NutClass c) {
	switch (c) {
		case NutClass::Api:		return "Api";
		case NutClass::Seconds:	return "Seconds";
		case NutClass::Rashi:	return "Rashi";
		case NutClass::Mangala:	return "Mangala";
		default:				return "Unknown";
	}
}

bool SessionManager::writeResult(bool passed, float api, float seconds, float rashi, float mangala) {
	if (_sessionPath.isEmpty()) return false;
	fs::File f = FSYS.open(_sessionPath + "/result.json", "w");
	if (!f) return false;
	f.print("{\"passed\":"); f.print(passed ? "true" : "false");
	f.print(",\"percents\":{");
	f.print("\"Api\":"); f.print(api, 1); f.print(",");
	f.print("\"Seconds\":"); f.print(seconds, 1); f.print(",");
	f.print("\"Rashi\":"); f.print(rashi, 1); f.print(",");
	f.print("\"Mangala\":"); f.print(mangala, 1); f.print("}}");
	f.close();
	return true;
}
