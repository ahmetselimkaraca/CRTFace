// Data for the Music and Weather screens, fetched by a task on the Wi-Fi core so the screens
// never wait on the network.
//
//   Spotify: what's playing, every 3 s while the Music screen is up. Needs the keys in secrets.h
//            (see secrets.example.h); the connection is kept open between polls.
//   Weather: Open-Meteo every 10 min, no account needed. Location from config.h, or by default
//            from the IP address (ip-api.com, city-level).

#include "app.h"
#include "config.h"
#include "secrets.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <time.h>

#ifndef SPOTIFY_REFRESH_TOKEN
#define SPOTIFY_CLIENT_ID ""
#define SPOTIFY_CLIENT_SECRET ""
#define SPOTIFY_REFRESH_TOKEN ""
#endif

// Mozilla's root certificates, already built into the framework: HTTPS with real certificate checks.
extern const uint8_t crtStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t crtEnd[] asm("_binary_x509_crt_bundle_end");

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;   // the screens read while this task writes
static Music music = {};
static Weather weather = {};

Music musicNow() { portENTER_CRITICAL(&lock); Music m = music; portEXIT_CRITICAL(&lock); return m; }
Weather weatherNow() { portENTER_CRITICAL(&lock); Weather w = weather; portEXIT_CRITICAL(&lock); return w; }
static void setMusic(const Music &m) { portENTER_CRITICAL(&lock); music = m; portEXIT_CRITICAL(&lock); }
static void setWeather(const Weather &w) { portENTER_CRITICAL(&lock); weather = w; portEXIT_CRITICAL(&lock); }

// GET into body. secure: HTTPS with certificate checks. Returns the HTTP status or a negative error.
static int get(const char *url, bool secure, String &body) {
  NetworkClientSecure tls;
  NetworkClient plain;
  if (secure) tls.setCACertBundle(crtStart, crtEnd - crtStart);
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(secure ? (NetworkClient &)tls : plain, url)) return -1;
  int code = http.GET();
  if (code == 200) body = http.getString();
  http.end();
  return code;
}

// ---------------------------------------------------------------- Spotify

static String token, refresh = SPOTIFY_REFRESH_TOKEN;
static uint32_t tokenUntil = 0;
static NetworkClientSecure apiTls;   // stays connected between polls, saving a TLS handshake each time
static HTTPClient api;

static void musicError(const char *fmt, int code) {
  Music m = musicNow();
  m.state = MU_ERROR;
  snprintf(m.note, sizeof m.note, fmt, code);
  setMusic(m);
}

// Trade the long-lived refresh token for an access token (good for an hour).
static bool signIn() {
  NetworkClientSecure tls;
  tls.setCACertBundle(crtStart, crtEnd - crtStart);
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(tls, "https://accounts.spotify.com/api/token")) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + base64::encode(SPOTIFY_CLIENT_ID ":" SPOTIFY_CLIENT_SECRET));
  int code = http.POST("grant_type=refresh_token&refresh_token=" + refresh);
  String body = code == 200 ? http.getString() : String();
  http.end();
  JsonDocument doc;
  if (code != 200 || deserializeJson(doc, body) || !doc["access_token"].is<const char *>()) {
    Serial.printf("[spotify] sign-in: HTTP %d\n", code);
    musicError(code < 0 ? "No answer from Spotify (%d)" : "Spotify refused the keys (%d)", code);
    return false;
  }
  token = doc["access_token"].as<const char *>();
  if (doc["refresh_token"].is<const char *>()) refresh = doc["refresh_token"].as<const char *>();   // Spotify may hand out a new one
  tokenUntil = millis() + ((doc["expires_in"] | 3600) - 60) * 1000UL;
  return true;
}

static void pollSpotify() {
  if (!token.length() || (int32_t)(millis() - tokenUntil) >= 0)
    if (!signIn()) return;
  static bool ready = false;
  if (!ready) { apiTls.setCACertBundle(crtStart, crtEnd - crtStart); api.setReuse(true); api.setTimeout(8000); ready = true; }
  if (!api.begin(apiTls, "https://api.spotify.com/v1/me/player/currently-playing?market=from_token&additional_types=episode")) return;
  api.addHeader("Authorization", "Bearer " + token);
  int code = api.GET();
  String body = code == 200 ? api.getString() : String();
  api.end();

  Music m = musicNow();
  if (code == 401) { token = ""; return; }   // token ran out early: sign in again next poll
  if (code == 204) m.state = MU_IDLE;        // nothing playing
  else if (code != 200) {
    Serial.printf("[spotify] now playing: HTTP %d\n", code);
    return musicError(code < 0 ? "No answer from Spotify (%d)" : "Spotify said %d", code);
  } else {
    // Keep only the fields we show; the full reply is several KB.
    JsonDocument filter, doc;
    filter["is_playing"] = true;
    filter["progress_ms"] = true;
    filter["item"]["name"] = true;
    filter["item"]["duration_ms"] = true;
    filter["item"]["artists"][0]["name"] = true;
    filter["item"]["show"]["name"] = true;   // podcasts
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return musicError("Odd reply from Spotify", 0);
    JsonObject item = doc["item"];
    if (item.isNull()) m.state = MU_IDLE;    // e.g. an ad, or a private session
    else {
      m.state = (doc["is_playing"] | false) ? MU_PLAY : MU_PAUSE;
      toCP437(item["name"] | "", m.title, sizeof m.title);
      String who;
      JsonArray artists = item["artists"];
      for (JsonObject a : artists) {
        if (who.length()) who += ", ";
        who += a["name"] | "";
      }
      if (!who.length()) who = item["show"]["name"] | "";
      toCP437(who.c_str(), m.artist, sizeof m.artist);
      m.progressMs = doc["progress_ms"] | 0;
      m.durationMs = item["duration_ms"] | 0;
      m.at = millis();
    }
  }
  setMusic(m);
}

// ---------------------------------------------------------------- weather

static float lat = WEATHER_LAT, lon = WEATHER_LON;
static char city[32] = WEATHER_CITY;

static bool locate() {
  if (lat != 0 || lon != 0) return true;
  String body;
  int code = get("http://ip-api.com/json/?fields=status,city,lat,lon", false, body);   // free tier is HTTP only
  JsonDocument doc;
  if (code != 200 || deserializeJson(doc, body) || doc["status"] != "success") {
    Serial.printf("[weather] location: HTTP %d\n", code);
    return false;
  }
  lat = doc["lat"];
  lon = doc["lon"];
  toCP437(doc["city"] | "", city, sizeof city);
  return true;
}

static bool pollWeather() {
  if (!locate()) return false;
  char url[300];
  snprintf(url, sizeof url,
           "https://api.open-meteo.com/v1/forecast?latitude=%.3f&longitude=%.3f"
           "&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=4",
           lat, lon);
  String body;
  int code = get(url, true, body);
  JsonDocument doc;
  if (code != 200 || deserializeJson(doc, body)) {
    Serial.printf("[weather] forecast: HTTP %d\n", code);
    return false;
  }
  Weather w = {};
  w.valid = true;
  strlcpy(w.city, city, sizeof w.city);
  JsonObject c = doc["current"], d = doc["daily"];
  w.temp = lroundf(c["temperature_2m"] | 0.0f);
  w.feels = lroundf(c["apparent_temperature"] | 0.0f);
  w.humidity = c["relative_humidity_2m"] | 0;
  w.wind = lroundf(c["wind_speed_10m"] | 0.0f);
  w.code = c["weather_code"] | 0;
  for (int i = 0; i < 4; i++) {
    w.dCode[i] = d["weather_code"][i] | 0;
    w.dMax[i] = lroundf(d["temperature_2m_max"][i] | 0.0f);
    w.dMin[i] = lroundf(d["temperature_2m_min"][i] | 0.0f);
    struct tm t = {};
    if (sscanf(d["time"][i] | "", "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
      t.tm_year -= 1900; t.tm_mon -= 1; t.tm_hour = 12;
      mktime(&t);   // fills in the weekday
    }
    w.dDay[i] = t.tm_wday;
  }
  setWeather(w);
  return true;
}

// ---------------------------------------------------------------- task

static void feedTask(void *) {
  uint32_t lastMusic = 0, nextWeather = 0;
  for (;;) {
    uint32_t now = millis();
    if (netOnline()) {
      if (gMode == MODE_MUSIC && music.state != MU_OFF && now - lastMusic >= 3000) { lastMusic = now; pollSpotify(); }
      if ((int32_t)(now - nextWeather) >= 0) nextWeather = now + (pollWeather() ? 10 * 60000 : 60000);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void feedsBegin() {
  music.state = strlen(SPOTIFY_REFRESH_TOKEN) ? MU_WAIT : MU_OFF;
  xTaskCreatePinnedToCore(feedTask, "feeds", 16384, nullptr, 1, nullptr, 0);   // core 0: the Wi-Fi core
}
