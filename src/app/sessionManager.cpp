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

// ---- helpers for JSON reading (tiny & tolerant) ----
static bool sm_readSessionJsonAtPath(const String& dirPath, ClassCounts& counts, uint32_t& lastIdx) {
	fs::File f = FSYS.open(dirPath + "/session.json", "r");
	if (!f) return false;
	String js; js.reserve(512);
	while (f.available()) js += (char)f.read();
	f.close();

	auto grabNum = [&](const char* key, const char* parent) -> uint32_t {
		if (parent) {
			const int p0 = js.indexOf(String("\"") + parent + "\":");
			if (p0 < 0) return 0;
			const int p1 = js.indexOf(String("\"") + key + "\":", p0);
			if (p1 < 0) return 0;
			return (uint32_t) js.substring(p1 + strlen(key) + 3).toInt();
		} else {
			const int p = js.indexOf(String("\"") + key + "\":");
			if (p < 0) return 0;
			return (uint32_t) js.substring(p + strlen(key) + 3).toInt();
		}
	};

	lastIdx          = grabNum("last", nullptr);
	counts.api       = grabNum("Api", "counts");
	counts.seconds   = grabNum("Seconds", "counts");
	counts.rashi     = grabNum("Rashi", "counts");
	counts.mangala   = grabNum("Mangala", "counts");
	return true;
}

bool SessionManager::resumeIfOpen() {
	// find newest /sessions/<folder> that has *no* result.json
	if (!FSYS.exists("/sessions")) return false;
	fs::File root = FSYS.open("/sessions");
	if (!root || !root.isDirectory()) return false;

	String best;
	for (fs::File e = root.openNextFile(); e; e = root.openNextFile()) {
		if (!e.isDirectory()) continue;
		String dir = String(e.path());
		if (!dir.startsWith("/")) dir = "/" + dir;
		if (FSYS.exists(dir + "/result.json")) continue; // treated as closed
		if (best.isEmpty() || dir > best) best = dir;    // lexicographically newest
	}
	if (best.isEmpty()) return false;

	ClassCounts c; uint32_t last = 0;
	if (!sm_readSessionJsonAtPath(best, c, last)) return false;

	_sessionPath = best;
	_counts = c;
	_lastIndex = last;
	_open = true;

	Serial.printf("[SESSION] resumeIfOpen -> %s (last=%u)\n", _sessionPath.c_str(), (unsigned)_lastIndex);
	return true;
}

// tiny CSV splitter
static int splitCsv(const String& s, char delim, String out[], int maxParts) {
	int count = 0, start = 0;
	for (int i = 0; i < (int)s.length() && count < maxParts - 1; ++i) {
		if (s[i] == delim) { out[count++] = s.substring(start, i); start = i + 1; }
	}
	out[count++] = s.substring(start);
	return count;
}

bool SessionManager::reclassifyLast(NutClass newClass, NutClass* oldClassOut) {
	if (!_open || _lastIndex == 0) return false;
	if (newClass == NutClass::Unknown) return false;

	char fileName[32];
	snprintf(fileName, sizeof(fileName), "/nut%05u.csv", (unsigned)_lastIndex);
	String path = _sessionPath + String(fileName);

	fs::File f = FSYS.open(path, "r");
	if (!f) return false;
	String content; content.reserve(2048);
	while (f.available()) content += (char)f.read();
	f.close();

	int lastLineStart = content.lastIndexOf('\n', content.length() - 2);
	if (lastLineStart < 0) return false;
	String lastLine = content.substring(lastLineStart + 1);
	lastLine.trim();
	if (lastLine.length() == 0) {
		int prev = content.lastIndexOf('\n', lastLineStart - 1);
		if (prev < 0) return false;
		lastLine = content.substring(prev + 1, lastLineStart);
		lastLine.trim();
	}

	// t_ms,adc_code,baseline,pred_class,override_class
	String cols[6];
	int n = splitCsv(lastLine, ',', cols, 6);
	if (n < 4) return false;
	NutClass pred = parseClass(cols[3]);
	NutClass prevEff = pred;
	if (n >= 5 && cols[4].length() > 0) prevEff = parseClass(cols[4]);

	if (oldClassOut) *oldClassOut = prevEff;
	if (prevEff == newClass) return true;

	switch (prevEff) {
		case NutClass::Api:		if (_counts.api) _counts.api--; break;
		case NutClass::Seconds:	if (_counts.seconds) _counts.seconds--; break;
		case NutClass::Rashi:	if (_counts.rashi) _counts.rashi--; break;
		case NutClass::Mangala:	if (_counts.mangala) _counts.mangala--; break;
		default: break;
	}
	switch (newClass) {
		case NutClass::Api:		_counts.api++; break;
		case NutClass::Seconds:	_counts.seconds++; break;
		case NutClass::Rashi:	_counts.rashi++; break;
		case NutClass::Mangala:	_counts.mangala++; break;
		default: break;
	}

	// rewrite CSV with override_class=newClass on every data row
	String rebuilt; rebuilt.reserve(content.length() + 32);
	int pos = 0; int lineNo = 0;
	while (pos < (int)content.length()) {
		int end = content.indexOf('\n', pos);
		if (end < 0) end = content.length();
		String line = content.substring(pos, end);

		if (lineNo == 0) {
			rebuilt += line; rebuilt += '\n';
		} else {
			String c[6];
			int cc = splitCsv(line, ',', c, 6);
			if (cc >= 4) {
				rebuilt += c[0]; rebuilt += ',';
				rebuilt += c[1]; rebuilt += ',';
				rebuilt += c[2]; rebuilt += ',';
				rebuilt += c[3]; rebuilt += ',';
				rebuilt += className(newClass); rebuilt += '\n';
			} else {
				rebuilt += line; rebuilt += '\n';
			}
		}
		pos = end + 1; lineNo++;
	}

	fs::File w = FSYS.open(path, "w");
	if (!w) return false;
	w.print(rebuilt);
	w.close();

	return writeSessionJson();
}
