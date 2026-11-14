// The Music and Weather screens, in the same glowing terminal letters as the face (glow.cpp).
// Letter sizes: 2 = 12 px wide (36 across), 3 = 18 px (24 across), 4 = 24 px (18 across).

#include "app.h"
#include <time.h>

static const int X = 24;   // left/right margin

// Greedy word wrap into up to `rows` lines of `cols` letters; text left over ends the last line in "...".
static int wrap(const char *s, int cols, int rows, char out[][40]) {
  int n = 0;
  while (*s && n < rows) {
    while (*s == ' ') s++;
    int len = strlen(s), take = len;
    if (len > cols) {
      if (n == rows - 1) {
        memcpy(out[n], s, cols - 3);
        strcpy(out[n] + cols - 3, "...");
        return n + 1;
      }
      take = cols;
      while (take > 0 && s[take] != ' ') take--;   // break at the last space that fits
      if (!take) take = cols;                      // one long word: cut it
    }
    memcpy(out[n], s, take);
    out[n][take] = 0;
    s += take;
    n++;
  }
  return n;
}

static void drawClock(int y) {   // HH:MM at the top right, once the time is known
  struct tm t;
  char s[8];
  if (!getLocalTime(&t, 0)) return;
  strftime(s, sizeof s, "%H:%M", &t);
  glowText(s, SCREEN_W - X - glowWidth(s, 3), y, 3, 170);
}

// ---------------------------------------------------------------- music

static void mmss(char *out, size_t n, uint32_t ms) {
  unsigned s = ms / 1000;
  snprintf(out, n, "%u:%02u", s / 60, s % 60);
}

void musicDraw(uint32_t now) {
  Music m = musicNow();
  // The face up top, nodding along while something plays.
  const char *face = m.state == MU_PLAY ? "^_^" : m.state == MU_ERROR ? ";_;" : m.state == MU_WAIT || m.state == MU_OFF ? "o_o" : "-_-";
  glowText(face, X, 40 + (m.state == MU_PLAY ? (int)(5 * fabsf(sinf((now % 1142) * 0.0055f))) : 0), 6);
  drawClock(52);

  const char *head = nullptr, *sub = nullptr;
  if (!netOnline()) { head = "No Wi-Fi"; sub = "Waiting for the network..."; }
  else if (m.state == MU_OFF) { head = "Spotify isn't set up"; sub = "Add your keys to include/secrets.h"; }
  else if (m.state == MU_WAIT) head = "Asking Spotify...";
  else if (m.state == MU_ERROR) { head = "Can't show Spotify"; sub = m.note; }
  else if (m.state == MU_IDLE) { head = "Nothing playing"; sub = "Play something on Spotify"; }
  if (head) {
    glowText(head, X, 200, 3);
    if (sub) glowText(sub, X, 242, 2, 170);
    return;
  }

  char lines[2][40];
  int y = 150, n = wrap(m.title, 18, 2, lines);
  for (int i = 0; i < n; i++, y += 44) glowText(lines[i], X, y, 4);
  n = wrap(m.artist, 24, 2, lines);
  for (int i = 0; i < n; i++, y += 30) glowText(lines[i], X, y + 6, 3, 170);

  // Progress keeps moving between polls while playing.
  uint32_t p = m.progressMs + (m.state == MU_PLAY ? now - m.at : 0);
  if (m.durationMs && p > m.durationMs) p = m.durationMs;
  const int CELLS = 24;
  int filled = m.durationMs ? (int)((uint64_t)p * CELLS / m.durationMs) : 0;
  char bar[CELLS + 1];
  memset(bar, 0xDB, filled);           // full blocks
  bar[filled] = 0;
  glowText(bar, X, 360, 3);
  memset(bar, 0xB0, CELLS - filled);   // light shade
  bar[CELLS - filled] = 0;
  glowText(bar, X + filled * 18, 360, 3, 90);
  char a[12], b[12];
  mmss(a, sizeof a, p);
  mmss(b, sizeof b, m.durationMs);
  glowText(a, X, 400, 2, 170);
  if (m.state == MU_PAUSE) glowText("paused", (SCREEN_W - glowWidth("paused", 2)) / 2, 400, 2, 170);
  glowText(b, SCREEN_W - X - glowWidth(b, 2), 400, 2, 170);
}

// ---------------------------------------------------------------- weather

// Five-line pictures made of the font's own characters, at most 11 letters wide.
static const char *const SUN[5] = {"  \\   /", "   .-.", "--(   )--", "   `-'", "  /   \\"};
static const char *const PART[5] = {"  \\  /", "_ /\"\".-.", "  \\_(   ).", "  /(___(__)", ""};
static const char *const CLOUD[5] = {"", "    .--.", " .-(    ).", "(___.__)__)", ""};
static const char *const FOG[5] = {"", "_ - _ - _", " _ - _ -", "_ - _ - _", ""};
static const char *const RAIN[5] = {"    .--.", " .-(    ).", "(___.__)__)", " ' ' ' '", "' ' ' '"};
static const char *const SNOW[5] = {"    .--.", " .-(    ).", "(___.__)__)", " *  *  *", "*  *  *"};
static const char *const STORM[5] = {"    .--.", " .-(    ).", "(___.__)__)", "  /_  /_", "   /   /"};

// WMO weather codes, as Open-Meteo reports them.
static const char *sky(int c) {
  if (c == 0) return "clear";
  if (c <= 2) return "partly cloudy";
  if (c == 3) return "cloudy";
  if (c <= 48) return "fog";
  if (c <= 57) return "drizzle";
  if (c <= 67) return "rain";
  if (c <= 77) return "snow";
  if (c <= 82) return "showers";
  if (c <= 86) return "snow showers";
  return "thunderstorm";
}

static const char *const *picture(int c) {
  if (c == 0) return SUN;
  if (c <= 2) return PART;
  if (c == 3) return CLOUD;
  if (c <= 48) return FOG;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return SNOW;
  if (c >= 95) return STORM;
  return RAIN;
}

void weatherDraw(uint32_t now) {
  static const char *const DAY[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  Weather w = weatherNow();
  char line[48];
  if (w.city[0]) {
    strlcpy(line, w.city, 17);   // 16 letters leaves room for the clock
    glowText(line, X, 32, 3, 170);
  }
  drawClock(32);
  if (!w.valid) {
    glowText(netOnline() ? "Getting the weather..." : "No Wi-Fi", X, 200, 3);
    return;
  }

  const char *const *pic = picture(w.code);
  for (int i = 0; i < 5; i++) glowText(pic[i], X, 88 + i * 24, 3, 200);
  snprintf(line, sizeof line, "%d\xF8", w.temp);   // \xF8 = degree sign
  glowText(line, SCREEN_W - X - glowWidth(line, 10), 100, 10);
  glowText(sky(w.code), X, 228, 3);
  snprintf(line, sizeof line, "high %d\xF8  low %d\xF8  wind %d km/h", w.dMax[0], w.dMin[0], w.wind);
  glowText(line, X, 266, 2, 170);
  snprintf(line, sizeof line, "feels like %d\xF8  humidity %d%%", w.feels, w.humidity);
  glowText(line, X, 288, 2, 170);

  for (int i = 1; i < 4; i++) {   // the next three days
    int y = 330 + (i - 1) * 36;
    snprintf(line, sizeof line, "%s %3d\xF8 %3d\xF8", DAY[w.dDay[i] % 7], w.dMax[i], w.dMin[i]);
    glowText(line, X, y, 3, 200);
    glowText(sky(w.dCode[i]), X + 15 * 18, y + 4, 2, 170);
  }
}
