#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <FS.h>
#include "fs/fsCompat.h"

#include "app/sessionManager.h"
#include "UI/uiFacade.h"
#include "UI/alertSystem.h"

static const char* apSsid = "Areca-Classifier";	// open AP
static const char* sessionsDir = "/sessions";		// storage root

static WebServer server(80);
static SessionManager gSession;

// ---------- helpers ----------
static String contentTypeFor(const String& path) {
	if (path.endsWith(".htm") || path.endsWith(".html")) return "text/html";
	if (path.endsWith(".css"))	return "text/css";
	if (path.endsWith(".csv"))	return "text/csv";
	if (path.endsWith(".json")) return "application/json";
	if (path.endsWith(".txt"))	return "text/plain";
	return "application/octet-stream";
}

static bool isSafePath(String p) {
	// Normalize: ensure single leading slash; reject parent traversal.
	if (!p.startsWith("/")) p = "/" + p;
	while (p.indexOf("//") >= 0) p.replace("//", "/");
	if (p.indexOf("..") >= 0) return false;

	// Must be exactly /sessions or inside it.
	const String root = String(sessionsDir);
	return p == root || p.startsWith(root + "/");
}


static String humanSize(uint64_t b) {
	const char* units[] = {"B","KB","MB","GB"};
	int u = 0; double x = b;
	while (x >= 1024.0 && u < 3) { x /= 1024.0; ++u; }
	char buf[32]; snprintf(buf, sizeof(buf), (u==0?"%.0f %s":"%.1f %s"), x, units[u]);
	return String(buf);
}

static String htmlHeader() {
	return
		"<!doctype html><html><head><meta charset='utf-8'/>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'/>"
		"<title>Areca Classifier — Sessions</title>"
		"<style>body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,Helvetica;"
		"margin:20px;max-width:900px}h1{font-size:1.4rem;margin:0 0 12px}"
		"table{width:100%;border-collapse:collapse}th,td{padding:8px;border-bottom:1px solid #eee}"
		"a.button,button{padding:6px 10px;border:1px solid #ccc;border-radius:8px;text-decoration:none;background:#fff;cursor:pointer}"
		"code{background:#f6f6f6;padding:2px 6px;border-radius:6px}"
		".row{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}</style></head><body>";
}

static String htmlFooter() {
	return "<footer>AP: <code>Areca-Classifier</code> • Download files from <code>/sessions</code>.</footer></body></html>";
}

static void sendIndex() {
	String html = htmlHeader();
	html += "<h1>Areca — Controls</h1>";
	html += "<div class='row'>"
	        "<form method='post' action='/api/session/start'><button>Start Session</button></form>"
	        "<form method='post' action='/api/session/end'><button>End Session</button></form>"
	        "</div>";
	html += "<div class='row'>"
	        "<form method='post' action='/api/simulate?class=Api'><button>+ Api</button></form>"
	        "<form method='post' action='/api/simulate?class=Seconds'><button>+ Seconds</button></form>"
	        "<form method='post' action='/api/simulate?class=Rashi'><button>+ Rashi</button></form>"
	        "<form method='post' action='/api/simulate?class=Mangala'><button>+ Mangala</button></form>"
	        "</div>";

	html += "<h2>Reclassify Last</h2>";
	html += "<div class='row'>"
        "<form method='post' action='/api/reclassify?class=Api'><button>↺ Last → Api</button></form>"
        "<form method='post' action='/api/reclassify?class=Seconds'><button>↺ Last → Seconds</button></form>"
        "<form method='post' action='/api/reclassify?class=Rashi'><button>↺ Last → Rashi</button></form>"
        "<form method='post' action='/api/reclassify?class=Mangala'><button>↺ Last → Mangala</button></form>"
        "</div>";


		// Live status (server-side render)
ClassCounts cc = gSession.getCounts();
float pa=0, ps=0, pr=0, pm=0; gSession.getPercentages(pa, ps, pr, pm);
html += "<h2>Status</h2>";
html += "<div class='row' style='gap:16px;align-items:center'>";
html += String("<code>") + (gSession.isOpen() ? "Session: OPEN" : "Session: CLOSED") + "</code>";
html += "<code>Api: " + String((int)roundf(pa)) + "% (" + String(cc.api) + ")</code>";
html += "<code>Seconds: " + String((int)roundf(ps)) + "% (" + String(cc.seconds) + ")</code>";
html += "<code>Rashi: " + String((int)roundf(pr)) + "% (" + String(cc.rashi) + ")</code>";
html += "<code>Mangala: " + String((int)roundf(pm)) + "% (" + String(cc.mangala) + ")</code>";
html += "</div>";


	html += "<h1>Sessions</h1>";
	if (!FSYS.exists(sessionsDir)) {
		html += "<p><em>No sessions yet.</em></p>";
		server.send(200, "text/html", html + htmlFooter());
		return;
	}

	fs::File root = FSYS.open(sessionsDir);
	if (!root || !root.isDirectory()) {
		server.send(500, "text/plain", "sessionsDir missing or not a directory");
		return;
	}

	html += "<table><thead><tr><th style='width:45%'>Name</th>"
			"<th style='width:20%'>Type</th><th style='width:20%'>Size</th>"
			"<th style='width:15%'>Action</th></tr></thead><tbody>";

	fs::File entry = root.openNextFile();
	while (entry) {
		const bool isDir = entry.isDirectory();
String name = String(entry.path());         
if (!name.startsWith("/")) name = "/" + name;		if (isDir) {
			fs::File sub = FSYS.open(name);
			uint64_t total = 0;
			if (sub) {
				fs::File f = sub.openNextFile();
				while (f) { total += f.size(); f = sub.openNextFile(); }
			}
			html += "<tr><td><b>" + name + "</b></td><td>Session</td><td>"
					+ humanSize(total) + "</td><td>"
					"<a class='button' href='/list?dir=" + name + "'>Open</a></td></tr>";
		} else {
			const String href = "/download?path=" + name;
			html += "<tr><td>" + name + "</td><td>File</td><td>" + humanSize(entry.size()) +
					"</td><td><a class='button' href='" + href + "'>Download</a></td></tr>";
		}
		entry = root.openNextFile();
	}

	html += "</tbody></table>";
	html += htmlFooter();
	server.send(200, "text/html", html);
}

static void sendDirListing(const String& dirPath) {
	if (!isSafePath(dirPath)) {
		server.send(400, "text/plain", "Bad path");
		return;
	}
	fs::File dir = FSYS.open(dirPath);
	if (!dir || !dir.isDirectory()) {
		server.send(404, "text/plain", "Not a directory");
		return;
	}

	String html = htmlHeader();
	html += "<p><a class='button' href='/'>← Back</a></p>";
	html += "<h1>Session: " + dirPath + "</h1>";
	html += "<table><thead><tr><th style='width:55%'>File</th>"
			"<th style='width:25%'>Size</th><th style='width:20%'>Action</th>"
			"</tr></thead><tbody>";

	fs::File f = dir.openNextFile();
	while (f) {
		if (!f.isDirectory()) {
			String p = String(f.path());
			String href = "/download?path=" + p;
			html += "<tr><td>" + p + "</td><td>" + humanSize(f.size()) +
					"</td><td><a class='button' href='" + href + "'>Download</a></td></tr>";
		}
		f = dir.openNextFile();
	}
	html += "</tbody></table>";
	html += htmlFooter();
	server.send(200, "text/html", html);
}

static void handleDownload() {
	if (!server.hasArg("path")) { server.send(400, "text/plain", "Missing path"); return; }
	String path = server.arg("path");
	if (!isSafePath(path)) { server.send(400, "text/plain", "Bad path"); return; }

	if (!FSYS.exists(path)) { server.send(404, "text/plain", "File not found"); return; }
	fs::File file = FSYS.open(path, "r");
	if (!file || file.isDirectory()) { server.send(404, "text/plain", "Not a file"); return; }

	const String ct = contentTypeFor(path);
	const String filename = path.substring(path.lastIndexOf('/') + 1);

	server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
	server.streamFile(file, ct);
	file.close();
}

static void handleApiList() {
	String dir = server.hasArg("dir") ? server.arg("dir") : String(sessionsDir);
	if (!isSafePath(dir)) { server.send(400, "application/json", "{\"error\":\"bad path\"}"); return; }
	fs::File root = FSYS.open(dir);
	if (!root || !root.isDirectory()) { server.send(404, "application/json", "{\"error\":\"not found\"}"); return; }

	String json = "{\"path\":\"" + dir + "\",\"items\":[";
	bool first = true;
	fs::File e = root.openNextFile();
	while (e) {
		if (!first) json += ",";
		first = false;
		json += "{\"name\":\"";
		json += String(e.name());
		json += "\",\"dir\":";
		json += (e.isDirectory() ? "true" : "false");
		json += ",\"size\":";
		json += String((uint32_t)e.size());
		json += "}";
		e = root.openNextFile();
	}
	json += "]}";
	server.send(200, "application/json", json);
}

void webPortalBegin() {
	// FS mount
	if (!fsBegin(true)) Serial.println("[WEB] FS mount failed");
	if (!FSYS.exists(sessionsDir)) FSYS.mkdir(sessionsDir);

	// SoftAP
	WiFi.mode(WIFI_AP);
	bool ok = WiFi.softAP(apSsid);
	IPAddress ip = WiFi.softAPIP();
	Serial.printf("[WEB] SoftAP %s %s, IP: %s\n", apSsid, ok ? "started" : "failed", ip.toString().c_str());

	// Session manager + UI facade
	gSession.begin();
	uiFacadeInit();

	if (gSession.resumeIfOpen()) {
	ClassCounts cc = gSession.getCounts();
	float a,s,r,m; gSession.getPercentages(a,s,r,m);
	uiFacadeSetPercentages((int)roundf(a), (int)roundf(s), (int)roundf(r), (int)roundf(m));
	Serial.println("[WEB] resumed open session");
	}

	// HTML routes
	server.on("/", HTTP_GET, []() { sendIndex(); });
	server.on("/list", HTTP_GET, []() { if (!server.hasArg("dir")) { sendIndex(); return; } sendDirListing(server.arg("dir")); });
	server.on("/download", HTTP_GET, handleDownload);
	server.on("/api/list", HTTP_GET, handleApiList);
	server.on("/health", HTTP_GET, []() { server.send(200, "text/plain", "OK"); });

	// API routes (POST)
server.on("/api/session/start", HTTP_POST, [](){
	bool ok = gSession.startSession();
	if (ok) {
		// Immediately reflect a fresh session on the TFT
		uiFacadeClearBatchResult();          // clear PASS/FAIL text
		uiFacadeSetPercentages(0, 0, 0, 0);  // show 0% for all categories
		// optional visual ping so the operator sees it started
		// alertFlash(NutClass::Api, 600);

		server.send(200, "application/json", "{\"ok\":true}");
	} else {
		server.send(500, "application/json", "{\"ok\":false,\"err\":\"mkdir or path conflict\"}");
	}
});
;

server.on("/api/session/end", HTTP_POST, [](){
	bool ok = gSession.endSession();

	// Compute percentages
	ClassCounts cc = gSession.getCounts();
	float a=0,s=0,r=0,m=0;
	gSession.getPercentages(a,s,r,m);

	// Apply batch rule
	bool passed = false;
	String why = "";
	if (cc.total() <= 0) {
		why = "empty";
		passed = false;
	} else if (a < 25.0f) {
		why = "api<25%";
	} else if (s < 2.0f || s > 7.0f) {
		why = "seconds out of [2,7]%";
	} else if (m >= 2.0f) {
		why = "mangala>=2%";
	} else {
		passed = true;
	}

	// Persist result.json
	gSession.writeResult(passed, a, s, r, m);

	// Show PASS/FAIL badge on TFT
	uiFacadeSetBatchResult(passed);

	char buf[256];
	snprintf(buf, sizeof(buf),
		"{\"ok\":%s,\"result\":\"%s\",\"why\":\"%s\",\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
		ok ? "true" : "false",
		passed ? "pass" : "fail",
		why.c_str(),
		a, s, r, m);

	server.send(ok ? 200 : 500, "application/json", buf);
});


	server.on("/api/simulate", HTTP_POST, [](){
		if (!server.hasArg("class")) { server.send(400, "application/json", "{\"error\":\"missing class\"}"); return; }
		NutClass c = SessionManager::parseClass(server.arg("class"));
		if (c == NutClass::Unknown) { server.send(400, "application/json", "{\"error\":\"bad class\"}"); return; }

		bool ok = gSession.addSimulatedNut(c);
		if (!ok) { server.send(500, "application/json", "{\"ok\":false}"); return; }

		ClassCounts cc = gSession.getCounts();
		float a,s,r,m; gSession.getPercentages(a,s,r,m);
		uiFacadeSetPercentages((int)roundf(a), (int)roundf(s), (int)roundf(r), (int)roundf(m));
		alertFlash(c, 1000);

		char buf[256];
		snprintf(buf, sizeof(buf),
			"{\"ok\":true,\"counts\":{\"Api\":%u,\"Seconds\":%u,\"Rashi\":%u,\"Mangala\":%u},"
			"\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
			(unsigned)cc.api,(unsigned)cc.seconds,(unsigned)cc.rashi,(unsigned)cc.mangala,
			a,s,r,m);
		server.send(200, "application/json", buf);
	});

	// POST /api/reclassify?class=Api|Seconds|Rashi|Mangala
// POST /api/reclassify?class=Api|Seconds|Rashi|Mangala
server.on("/api/reclassify", HTTP_POST, [](){
	if (!server.hasArg("class")) { server.send(400, "application/json", "{\"error\":\"missing class\"}"); return; }
	NutClass newC = SessionManager::parseClass(server.arg("class"));
	if (newC == NutClass::Unknown) { server.send(400, "application/json", "{\"error\":\"bad class\"}"); return; }

	NutClass oldC = NutClass::Unknown;
	bool ok = gSession.reclassifyLast(newC, &oldC);
	if (!ok) { server.send(500, "application/json", "{\"ok\":false}"); return; }

	ClassCounts cc = gSession.getCounts();
	float a,s,r,m; gSession.getPercentages(a,s,r,m);
	uiFacadeSetPercentages((int)roundf(a), (int)roundf(s), (int)roundf(r), (int)roundf(m));
	alertFlash(newC, 800);

	char buf[256];
	snprintf(buf, sizeof(buf),
		"{\"ok\":true,\"old\":\"%s\",\"new\":\"%s\",\"counts\":{\"Api\":%u,\"Seconds\":%u,\"Rashi\":%u,\"Mangala\":%u},"
		"\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
		SessionManager::className(oldC), SessionManager::className(newC),
		(unsigned)cc.api,(unsigned)cc.seconds,(unsigned)cc.rashi,(unsigned)cc.mangala, a,s,r,m);
	server.send(200, "application/json", buf);
});

server.on("/api/status", HTTP_GET, [](){
	ClassCounts cc = gSession.getCounts();
	float a,s,r,m; gSession.getPercentages(a,s,r,m);
	char buf[256];
	snprintf(buf, sizeof(buf),
		"{\"open\":%s,\"path\":\"%s\",\"last\":%u,"
		"\"counts\":{\"Api\":%u,\"Seconds\":%u,\"Rashi\":%u,\"Mangala\":%u},"
		"\"percents\":{\"Api\":%.1f,\"Seconds\":%.1f,\"Rashi\":%.1f,\"Mangala\":%.1f}}",
		gSession.isOpen() ? "true" : "false",
		gSession.currentPath().c_str(),
		(unsigned)cc.total(),
		(unsigned)cc.api,(unsigned)cc.seconds,(unsigned)cc.rashi,(unsigned)cc.mangala,
		a,s,r,m);
	server.send(200, "application/json", buf);
});


	server.onNotFound([]() { sendIndex(); });

	server.begin();
	Serial.println("[WEB] HTTP server started on :80");
}

void webPortalPoll() {
	server.handleClient();
}
