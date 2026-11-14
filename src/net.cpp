// Wi-Fi, names for the page, the control web page, and the web socket that streams the mouse.
//
// Where the page lives:
//   main Wi-Fi:      http://HOSTNAME.local (mDNS), or the IP
//   device's own AP: http://AP_DOMAIN (any name works there, via a catch-all DNS), or http://192.168.4.1
//
// Socket protocol (text frames):
//   page -> device   "m X Y B"   pointer at X,Y (0..479), button B (0/1)
//                    "M n"       screen   (0 face, 1 music, 2 weather, 3 desktop)
//                    "E n"       expression
//                    "T n"       color theme
//   device -> page   "s MODE EXPR THEME"   current state (on connect and on every change)

#include "app.h"
#include "config.h"
#include "secrets.h"
#include "web_page.h"
#include "mona_sans.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#ifndef WIFI_USERNAME
#define WIFI_USERNAME ""
#endif

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static DNSServer dns;
static bool online = false, apOn = false, everClient = false;
static char apName[24];

bool netOnline() { return online; }
String netStaIP() { return WiFi.localIP().toString(); }
String netDomain() { return HOSTNAME ".local"; }
String netApInfo() { return apOn ? String(apName) + ": http://" AP_DOMAIN : String(); }
uint8_t netClients() { return ws.count(); }
bool netEverHadClient() { return everClient; }

static void sendState(AsyncWebSocketClient *c) {
  char b[32];
  snprintf(b, sizeof b, "s %u %u %u", gMode, gExpr, gTheme);
  if (c) c->text(b);
  else ws.textAll(b);
}

static void handle(const char *m) {
  int a = 0, b = 0, c = 0;
  switch (m[0]) {
    case 'm':
      if (sscanf(m + 1, "%d %d %d", &a, &b, &c) == 3) {
        ptr.x = constrain(a, 0, SCREEN_W - 1);
        ptr.y = constrain(b, 0, SCREEN_H - 1);
        if (c && !ptr.down) ptr.presses = ptr.presses + 1;
        ptr.down = c != 0;
        ptr.lastMoveMs = millis();
      }
      return;
    case 'M': if (sscanf(m + 1, "%d", &a) == 1 && a >= 0 && a < MODE_COUNT) gMode = a; break;
    case 'E': if (sscanf(m + 1, "%d", &a) == 1 && a >= 0 && a < EX_COUNT) gExpr = a; break;
    case 'T': if (sscanf(m + 1, "%d", &a) == 1 && a >= 0 && a < TH_COUNT) gTheme = a; break;
    default: return;
  }
  sendState(nullptr);   // keep every open page in sync
}

static void onWs(AsyncWebSocket *, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    everClient = true;
    c->setCloseClientOnQueueFull(false);
    sendState(c);
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;
    char buf[48];
    size_t n = len < sizeof buf - 1 ? len : sizeof buf - 1;
    memcpy(buf, data, n);
    buf[n] = 0;
    handle(buf);
  }
}

static void startAP() {
  if (apOn) return;
  snprintf(apName, sizeof apName, "CRTFace-%04X", (uint16_t)(ESP.getEfuseMac() >> 32));
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName, AP_PASSWORD);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());   // every name -> the device (also triggers "sign in" pop-ups)
  apOn = true;
  Serial.printf("[ap] %s up at http://%s / http://192.168.4.1\n", apName, AP_DOMAIN);
}

void netBegin() {
  WiFi.persistent(false);
  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);   // low latency for the mouse stream
  if (AP_MODE == 2) startAP();

  if (strlen(WIFI_USERNAME) > 0)
    WiFi.begin(WIFI_SSID, WPA2_AUTH_PEAP, WIFI_USERNAME, WIFI_USERNAME, WIFI_PASSWORD);
  else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  ws.onEvent(onWs);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r) {
    r->send(200, "text/html", (const uint8_t *)WEB_PAGE, sizeof(WEB_PAGE) - 1);
  });
  server.on("/mona-sans.woff2", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncWebServerResponse *res = r->beginResponse(200, "font/woff2", MONA_SANS, sizeof(MONA_SANS));
    res->addHeader("Cache-Control", "public, max-age=31536000, immutable");   // fetched once, then from the browser cache
    r->send(res);
  });
  server.onNotFound([](AsyncWebServerRequest *r) { r->redirect("/"); });
  server.begin();
}

void netLoop() {
  uint32_t now = millis();
  static uint32_t lastCheck = 0;
  if (gStateDirty) { gStateDirty = false; sendState(nullptr); }
  if (now - lastCheck < 500) return;
  lastCheck = now;
  ws.cleanupClients();

  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !online) {
    Serial.printf("[wifi] connected: http://%s  (http://%s.local)\n", netStaIP().c_str(), HOSTNAME);
    static bool started = false;
    if (!started) {
      started = true;
      if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
      configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com");
    }
  } else if (!up && online) {
    Serial.println("[wifi] lost connection (auto-reconnecting)");
  }
  online = up;

  if (AP_MODE == 1 && !online && !apOn && now > 20000) startAP();
}
