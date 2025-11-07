#pragma once
#include <Arduino.h>

enum class NutClass : uint8_t { Api=0, Seconds=1, Rashi=2, Mangala=3, Unknown=255 };

struct ClassCounts {
	uint32_t api = 0, seconds = 0, rashi = 0, mangala = 0;
	uint32_t total() const { return api + seconds + rashi + mangala; }
};

class SessionManager {
public:
	void begin();
	bool startSession();
	bool endSession();
	bool isOpen() const { return _open; }
	String currentPath() const { return _sessionPath; }

	bool addSimulatedNut(NutClass cls);
	ClassCounts getCounts() const { return _counts; }
	void getPercentages(float &api, float &seconds, float &rashi, float &mangala) const;

	static NutClass parseClass(const String &s);
	static const char* className(NutClass c);

	// Persist batch result
	bool writeResult(bool passed, float api, float seconds, float rashi, float mangala);

	// ---------- Resume helpers (PUBLIC) ----------
	// Find newest open session (closed:false). Returns true and sets pathOut if found.
	bool findResumeCandidate(String& pathOut);

	// Read counts/last from a session.json at 'path' (no state change).
	bool previewSessionAtPath(const String& path, ClassCounts& countsOut, uint32_t& lastIdxOut, bool& closedOut);

	// Load a session (sets internal state open=true, closed=false).
	bool loadSessionFromPath(const String& path);

	// Mark a session closed in-place (used when user chooses "Start New").
	bool markClosedAtPath(const String& path);

private:
	bool writeSessionJson();
	bool writeCsvForNut(uint32_t idx, NutClass cls);

private:
	bool _open = false;
	String _sessionPath;
	ClassCounts _counts;
	uint32_t _lastIndex = 0;
};
