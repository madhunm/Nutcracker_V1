#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "fs/fsCompat.h"

#include "app/sessionManager.h"
#include "UI/uiFacade.h"
#include "net/webPortal.h"

// ---------- simple global server + session ----------
static WebServer server(80);
static SessionManager gSession;

// ---------- config ----------
static const char* apSsid = "Areca-Classifier";
static const char* apPass = "";	// unsecured, per your spec

// ---------- helpers ----------
static String htmlHeader() {
	String h;
	h += "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>";
	h += "<style>body{font-family:system-ui,Arial;margin:16px} .row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0} button{padding:8px 12px}</style>";
	h += "</head><body><h1>Areca Classifier</h1>";
	return h;
}

static void sendIndex() {
	String html = htmlHeader();

	html += "<h2>Session</h2>";
	html += "<div class='row'>"
		"<form method='post' action='/api/session/start'><button>Start Session</button></form>"
		"<form method='post' action='/api/session/end'><button>End Session</button></form>"
	"</div>";

	html += "<h2>Simulate</h2>";
	html += "<div class='row'>"
		"<form method='post' action='/api/simulate?class=Api'><button>+ Api</button></form>"
		"<form method='post' action='/api/simulate?class=Seconds'><button>+ Seconds</button></form>"
		"<form method='post' action='/api/simulate?class=Rashi'><button>+ Rashi</button></form>"
		"<form method='post' action='/api/simulate?class=Mangala'><button>+ Mangala</button></form>"
	"</div>";

	html += "<div class='row'><a href='/health'>Health</a></div>";
	html += "</body></html>";
	server.send(200, "text/html", html);
}

static void sendJsonOk(const String &s) {
	server.send(200, "application/json", s);
}

static void sendJsonErr(const String &s) {
	server.send(500, "application/json", s);
}

// ---------- API handlers ----------
static void handleHealth() {
	server.send(200, "text/plain", "OK");
}

static void handleStart() {
	bool ok = gSession.startSession();
	if (ok) {
		uiFacadeClearBatchResult();
		uiFacadePostPercentages(0,0,0,0);	// post to LVGL thread
		sendJsonOk("{\"ok\":true}");
	} else {
		sendJsonErr("{\"ok\":false,\"err\":\"mkdir or path conflict\"}");
	}
}

static void handleSim() {
	if (!server.hasArg("class")) { server.send(400, "application/json", "{\"error\":\"missing class\"}"); return; }
	NutClass c = SessionManager::parseClass(server.arg("class"));
	if (c == NutClass::Unknown) { server.send(400, "application/json", "{\"error\":\"bad class\"}"); return; }

	if (!gSession.addSimulatedNut(c)) { sendJsonErr("{\"ok\":false}"); return; }

	ClassCounts cc = gSession.getCounts();
	float a=0,s=0,r=0,m=0; gSession.getPercentages(a,s,r,m);
	uiFacadePostPercentages((int)roundf(a), (int)roundf(s), (int)roundf(r), (int)roundf(m));

	char buf[256];
	snprintf(buf, sizeof(buf),
		"{\"ok\":true,\"counts\":{\"Api\":%u,\"Seconds\":%u,\"Rashi\":%u,\"Mangala\":%u},"
		"\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
		(unsigned)cc.api,(unsigned)cc.seconds,(unsigned)cc.rashi,(unsigned)cc.mangala,
		a,s,r,m);
	sendJsonOk(String(buf));
}

static void handleEnd() {
	bool ok = gSession.endSession();

	ClassCounts cc = gSession.getCounts();
	float api=0,sec=0,ras=0,man=0;
	gSession.getPercentages(api,sec,ras,man);

	// Batch rules: Api >= 25%, Seconds in [2,7], Mangala < 2%
	bool passed = false;
	String why = "";
	if (cc.total() > 0) {
		if (api < 25.0f)				why = "api<25%";
		else if (sec < 2.0f || sec > 7.0f)	why = "seconds out of [2,7]%";
		else if (man >= 2.0f)			why = "mangala>=2%";
		else							passed = true;
	} else {
		why = "empty";
	}

	gSession.writeResult(passed, api, sec, ras, man);
	uiFacadePostBatchResult(passed);

	char buf[256];
	snprintf(buf, sizeof(buf),
		"{\"ok\":%s,\"result\":\"%s\",\"why\":\"%s\",\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
		ok ? "true" : "false",
		passed ? "pass" : "fail",
		why.c_str(), api, sec, ras, man);
	sendJsonOk(String(buf));
}

// ---------- public ----------
void webPortalBegin() {
	WiFi.mode(WIFI_AP);
	WiFi.softAP(apSsid, apPass);
	Serial.printf("[WEB] SoftAP %s started, IP: %s\n", apSsid, WiFi.softAPIP().toString().c_str());

	gSession.begin();

	server.on("/", HTTP_GET, sendIndex);
	server.on("/health", HTTP_GET, handleHealth);

	server.on("/api/session/start", HTTP_POST, handleStart);
	server.on("/api/simulate", HTTP_POST, handleSim);
	server.on("/api/session/end", HTTP_POST, handleEnd);

	server.begin();
	Serial.println("[WEB] HTTP server started on :80");
}

void webPortalPoll() {
	server.handleClient();
}
