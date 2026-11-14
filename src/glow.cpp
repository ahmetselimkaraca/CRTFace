// Glowing dot-matrix text, drawn straight into the canvas (face, music, weather screens).
//
// Every lit font pixel is a small pre-shaded square with a soft edge, colored through a
// phosphor palette, with every third row dimmed like a scanline. The palette rises in every
// channel with brightness, so a larger RGB565 value is always a brighter pixel: overlapping
// glows combine with a plain max. Only what the last frame lit is cleared, and only the rows
// that changed go to the panel.

#include "app.h"
#include "font/glcdfont.h"   // the library's classic 5x7 font (code page 437)

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

static uint16_t *fb = nullptr;
static uint16_t pal[2][256];         // brightness -> RGB565; [1] is for the dim scanline rows
static uint16_t palFor = 0;          // theme color the palette was built for
static bool freshFrame = false;
struct Box { int x0, y0, x1, y1; };  // half-open pixel rect; empty when x1 <= x0
static Box drawn = {0, 0, 0, 0};     // lit so far this frame
static Box shown = {0, 0, 0, 0};     // lit last frame; cleared before drawing

static void grow(Box &b, int x0, int y0, int x1, int y1) {
  if (b.x1 <= b.x0) { b = {x0, y0, x1, y1}; return; }
  b.x0 = min(b.x0, x0); b.y0 = min(b.y0, y0);
  b.x1 = max(b.x1, x1); b.y1 = max(b.y1, y1);
}

// Glow keeps the phosphor's hue; only the hottest cores run toward white.
static void buildPalette(uint16_t c) {
  float rgb[3] = {((c >> 11) & 31) / 31.0f, ((c >> 5) & 63) / 63.0f, (c & 31) / 31.0f};
  for (int s = 0; s < 2; s++)
    for (int i = 0; i < 256; i++) {
      float t = i / 255.0f * (s ? 0.58f : 1.0f);
      float body = fminf(1, t / 0.75f), hot = 0.38f * smooth01((t - 0.7f) / 0.3f);
      uint8_t o[3];
      for (int k = 0; k < 3; k++) o[k] = (uint8_t)(255 * (rgb[k] * body + (1 - rgb[k]) * hot));
      pal[s][i] = RGB565(o[0], o[1], o[2]);
    }
  palFor = c;
}

// ---------------------------------------------------------------- stamps (one glowing font pixel)

struct Stamp { int scale, size; uint8_t px[32 * 32]; };
static Stamp stamps[6];
static int nStamps = 0;

static float sdBox(float px, float py, float half, float r) {
  float qx = fabsf(px) - half + r, qy = fabsf(py) - half + r;
  float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
  return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
}

// A square a hair bigger than its font pixel (so letters don't show seams), with a short halo.
static const Stamp &stampFor(int scale) {
  for (int i = 0; i < nStamps; i++) if (stamps[i].scale == scale) return stamps[i];
  Stamp &s = stamps[nStamps < 6 ? nStamps++ : 5];   // ponytail: 6 text sizes cached; a 7th reuses the last slot
  float half = scale / 2.0f + 0.5f, r = clampf(scale / 6.0f, 1, 2.5f), reach = scale < 6 ? 2 : 3;
  s.scale = scale;
  s.size = min(32, 2 * (int)ceilf(half + reach));
  float c = s.size / 2.0f;
  for (int y = 0; y < s.size; y++)
    for (int x = 0; x < s.size; x++) {
      float d = sdBox(x + 0.5f - c, y + 0.5f - c, half, r);
      float cover = clampf(0.5f - d, 0, 1), f = clampf(1 - fmaxf(d, 0) / reach, 0, 1);
      s.px[y * s.size + x] = (uint8_t)(255 * (cover + (1 - cover) * 0.4f * f * f));
    }
  return s;
}

static void stamp(const Stamp &s, int cx, int cy, uint8_t gain) {
  int x0 = cx - s.size / 2, y0 = cy - s.size / 2;
  int sx0 = max(0, -x0), sy0 = max(0, -y0), sx1 = min(s.size, SCREEN_W - x0), sy1 = min(s.size, SCREEN_H - y0);
  if (sx0 >= sx1 || sy0 >= sy1) return;
  grow(drawn, x0 + sx0, y0 + sy0, x0 + sx1, y0 + sy1);
  for (int y = sy0; y < sy1; y++) {
    uint16_t *row = fb + (y0 + y) * SCREEN_W + x0;
    const uint16_t *p = pal[(y0 + y) % 3 == 2];
    const uint8_t *src = s.px + y * s.size;
    for (int x = sx0; x < sx1; x++) {
      uint16_t c = p[src[x] * (gain + 1) >> 8];
      if (c > row[x]) row[x] = c;
    }
  }
}

// ---------------------------------------------------------------- frame

void glowBegin(Arduino_Canvas *g, bool fresh) {
  fb = g->getFramebuffer();
  if (themeColor() != palFor) buildPalette(themeColor());
  freshFrame = fresh;
  if (fresh) { g->fillScreen(RGB565_BLACK); shown = {0, 0, 0, 0}; }
  for (int y = shown.y0; y < shown.y1; y++) memset(fb + y * SCREEN_W + shown.x0, 0, (shown.x1 - shown.x0) * 2);
  drawn = {0, 0, 0, 0};
}

void glowText(const char *s, int x, int y, int scale, uint8_t gain) {
  const Stamp &st = stampFor(scale);
  for (; *s; s++, x += 6 * scale)
    for (int col = 0; col < 5; col++) {
      uint8_t bits = font[(uint8_t)*s * 5 + col];
      for (int row = 0; row < 8; row++)
        if (bits >> row & 1) stamp(st, x + col * scale + scale / 2, y + row * scale + scale / 2, gain);
    }
}

int glowWidth(const char *s, int scale) {
  int n = strlen(s);
  return n ? (n * 6 - 1) * scale : 0;
}

Band glowEnd() {
  Box dirty = shown;   // the panel needs last frame's lit rows (now dark) and this frame's
  if (drawn.x1 > drawn.x0) grow(dirty, drawn.x0, drawn.y0, drawn.x1, drawn.y1);
  shown = drawn;
  if (freshFrame) return {0, SCREEN_H};
  return dirty.x1 > dirty.x0 ? Band{(int16_t)dirty.y0, (int16_t)dirty.y1} : Band{0, 0};
}

// ---------------------------------------------------------------- UTF-8 -> code page 437

void toCP437(const char *in, char *out, size_t n) {
  static const struct { uint16_t u; uint8_t c; } MAP[] = {
      {0xC7, 0x80}, {0xFC, 0x81}, {0xE9, 0x82}, {0xE2, 0x83}, {0xE4, 0x84}, {0xE0, 0x85}, {0xE5, 0x86},
      {0xE7, 0x87}, {0xEA, 0x88}, {0xEB, 0x89}, {0xE8, 0x8A}, {0xEF, 0x8B}, {0xEE, 0x8C}, {0xEC, 0x8D},
      {0xC4, 0x8E}, {0xC5, 0x8F}, {0xC9, 0x90}, {0xE6, 0x91}, {0xC6, 0x92}, {0xF4, 0x93}, {0xF6, 0x94},
      {0xF2, 0x95}, {0xFB, 0x96}, {0xF9, 0x97}, {0xFF, 0x98}, {0xD6, 0x99}, {0xDC, 0x9A}, {0xE1, 0xA0},
      {0xED, 0xA1}, {0xF3, 0xA2}, {0xFA, 0xA3}, {0xF1, 0xA4}, {0xD1, 0xA5}, {0xDF, 0xE1},
      {0x131, 'i'}, {0x130, 'I'}, {0x15F, 's'}, {0x15E, 'S'}, {0x11F, 'g'}, {0x11E, 'G'},   // Turkish
      {0x2019, '\''}, {0x2018, '\''}, {0x201C, '"'}, {0x201D, '"'}, {0x2013, '-'}, {0x2014, '-'},
  };
  size_t o = 0;
  while (*in && o + 1 < n) {
    uint8_t b = *in;
    uint32_t u;
    int len = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 1;
    if (len == 1) u = b < 0x80 ? b : '?';
    else {
      u = b & (0x3F >> (len - 1));
      for (int i = 1; i < len; i++) {
        if ((in[i] & 0xC0) != 0x80) { len = i; u = '?'; break; }   // broken sequence
        u = u << 6 | (in[i] & 0x3F);
      }
    }
    in += len;
    char c = u < 0x80 ? (char)u : '?';
    for (auto &m : MAP) if (m.u == u) c = (char)m.c;
    out[o++] = c;
  }
  out[o] = 0;
}
