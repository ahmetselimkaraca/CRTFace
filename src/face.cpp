// The face: a terminal emoticon (o_o) in glowing dot-matrix letters (glow.cpp).
//
// Gaze holds still, then jumps in quick saccades and stops dead, like real eyes.
// A click anywhere (boop) makes it happy for a moment; it dozes off when left alone.

#include "app.h"

static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static const int SIZE = 15;   // px per font pixel: the face is 255 x 120

// ---------------------------------------------------------------- blink

static float lid = 0;                 // 0 open .. 1 shut
static uint32_t blinkAt = 0, nextBlink = 1500;
static bool blinking = false;

static void startBlink(uint32_t now) {
  if (!blinking) { blinking = true; blinkAt = now; }
}

// A light blink: shut in 50 ms, hold 20 ms, open in 100 ms.
static void updateBlink(uint32_t now) {
  if ((int32_t)(now - nextBlink) >= 0) {
    startBlink(now);
    nextBlink = now + (random(6) == 0 ? 250 : random(2400, 6500));   // sometimes a double blink
  }
  lid = 0;
  if (!blinking) return;
  uint32_t t = now - blinkAt;
  if (t < 50) lid = t / 50.0f;
  else if (t < 70) lid = 1;
  else if (t < 170) { float u = 1 - (t - 70) / 100.0f; lid = u * u; }
  else blinking = false;
}

// ---------------------------------------------------------------- gaze (saccades)

static float gx = 0, gy = 0;          // current gaze, -1..1
static float fromX = 0, fromY = 0, toX = 0, toY = 0;
static uint32_t sacStart = 0, sacDur = 1, lastSac = 0, nextWander = 0;
static bool glanceBack = false;       // next wander returns to where we were
static float homeX = 0, homeY = 0;

static void saccade(float x, float y, uint32_t now) {
  x = clampf(x, -1, 1);
  y = clampf(y, -1, 1);
  float d = hypotf(x - gx, y - gy);
  fromX = gx; fromY = gy; toX = x; toY = y;
  sacStart = now;
  sacDur = 35 + (uint32_t)(d * 45);   // bigger jumps take a little longer
  lastSac = now;
  if (d > 0.9f && random(4) == 0) startBlink(now);   // people often blink on big eye moves
}

static void updateGaze(uint32_t now, bool tracking, bool sleepy) {
  if (tracking) {
    float tx = clampf((ptr.x - 240) / 220.0f, -1, 1), ty = clampf((ptr.y - 220) / 220.0f, -1, 1);
    float d = fmaxf(fabsf(tx - toX), fabsf(ty - toY));
    // Jump when the cursor has moved enough; settle small leftovers after a pause.
    if ((d > 0.12f && now - lastSac > 90) || (d > 0.03f && now - lastSac > 450)) saccade(tx, ty, now);
    nextWander = now + 1500;
  } else if (sleepy) {
    if (fabsf(toX) > 0.01f || fabsf(toY - 0.3f) > 0.01f) saccade(0, 0.3f, now);
  } else if ((int32_t)(now - nextWander) >= 0) {
    int r = random(100);
    if (glanceBack) { saccade(homeX, homeY, now); glanceBack = false; }
    else if (r < 40) saccade(toX + random(-6, 7) / 100.0f, toY + random(-5, 6) / 100.0f, now);   // tiny shift
    else if (r < 60) saccade(0, 0, now);
    else if (r < 75) {                                                   // glance away and back
      homeX = toX; homeY = toY; glanceBack = true;
      saccade(random(2) ? 0.9f : -0.9f, random(-40, 41) / 100.0f, now);
    } else saccade(random(-80, 81) / 100.0f, random(-50, 51) / 100.0f, now);
    nextWander = now + (glanceBack ? random(350, 700) : random(600, 2800));
  }
  float t = (now - sacStart) / (float)sacDur;
  if (t >= 1) { gx = toX; gy = toY; }
  else {
    float e = 1 - (1 - t) * (1 - t) * (1 - t);   // fast start, hard stop
    gx = lerpf(fromX, toX, e);
    gy = lerpf(fromY, toY, e);
  }
}

// ---------------------------------------------------------------- frame

static const uint32_t TRACK_MS = 4000;     // follow the mouse this long after it last moved
static const uint32_t DOZE_MS = 120000;    // neutral face dozes off after this much idle
static uint32_t boopUntil = 0, lastDraw = 0, zClock = 0;
static uint16_t lastPresses = 0;

// Three z's drifting up and away, fading in and out. They run on their own clock that only
// advances while drawn (at most 50 ms a frame), so a hiccup pauses them instead of skipping ahead.
static void drawZs() {
  for (int i = 0; i < 3; i++) {
    float t = ((zClock + i * 1100) % 3300) / 3300.0f;
    glowText("z", 384 + (int)(56 * t + 7 * sinf(t * 9.42f)), 140 - (int)(104 * t), 4, (uint8_t)(255 * sinf(t * 3.1416f)));
  }
}

void faceDraw(uint32_t now) {
  if (ptr.presses != lastPresses) { lastPresses = ptr.presses; boopUntil = now + 900; }
  uint8_t e = gExpr;
  bool idle = !ptr.lastMoveMs || now - ptr.lastMoveMs > DOZE_MS;
  if (e == EX_NEUTRAL && idle && now > DOZE_MS) e = EX_SLEEPY;
  if ((int32_t)(boopUntil - now) > 0 && e != EX_ANGRY) e = EX_HAPPY;

  uint32_t dt = lastDraw && now - lastDraw < 50 ? now - lastDraw : 16;
  lastDraw = now;
  updateBlink(now);
  updateGaze(now, ptr.lastMoveMs && now - ptr.lastMoveMs < TRACK_MS, e == EX_SLEEPY);

  static const char EYES_L[EX_COUNT] = {'o', '^', ';', '>', 'O', '-'};
  static const char EYES_R[EX_COUNT] = {'o', '^', ';', '<', 'O', '-'};
  static const char MOUTH[EX_COUNT] = {'_', 'w', '_', '_', 'o', '_'};
  bool shut = lid > 0.5f;
  char f[4] = {shut ? '-' : EYES_L[e], MOUTH[e], shut ? '-' : EYES_R[e], 0};
  int w = 17 * SIZE, h = 8 * SIZE;   // three letters 6 font pixels apart, 8 rows tall
  glowText(f, (SCREEN_W - w) / 2 + (int)(gx * 40), (SCREEN_H - h) / 2 + (int)(gy * 28), SIZE);
  if (e == EX_SLEEPY) { zClock += dt; drawZs(); }

  // Until someone opens the control page, show where to find it.
  if (!netEverHadClient()) {
    String lines[3];
    int n = 0;
    if (netOnline()) {
      lines[n++] = "http://" + netDomain();
      lines[n++] = "or http://" + netStaIP();
    } else {
      lines[n++] = "connecting to Wi-Fi...";
    }
    String ap = netApInfo();
    if (ap.length()) lines[n++] = ap;
    for (int i = 0; i < n; i++)
      glowText(lines[i].c_str(), (SCREEN_W - glowWidth(lines[i].c_str(), 2)) / 2, SCREEN_H - 24 * (n - i) - 12, 2, 150);
  }
}
