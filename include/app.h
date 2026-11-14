#pragma once
// Shared state between the web task (net.cpp), the fetch task (feeds.cpp) and the render loop.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define SCREEN_W 480
#define SCREEN_H 480

// What's on screen. The BOOT button cycles Face -> Music -> Weather; Desktop is web-only.
enum Mode : uint8_t { MODE_FACE, MODE_MUSIC, MODE_WEATHER, MODE_DESKTOP, MODE_COUNT };
enum Expr : uint8_t { EX_NEUTRAL, EX_HAPPY, EX_SAD, EX_ANGRY, EX_SURPRISED, EX_SLEEPY, EX_COUNT };
enum Theme : uint8_t { TH_GREEN, TH_AMBER, TH_ICE, TH_PINK, TH_COUNT };

// The remote mouse, in screen pixels. Written by the web socket handler.
struct Pointer {
  volatile int16_t x = SCREEN_W / 2, y = SCREEN_H / 2;
  volatile bool down = false;
  volatile uint16_t presses = 0;     // bumps on every button press, so quick clicks between frames aren't lost
  volatile uint32_t lastMoveMs = 0;  // 0 = never moved
};
extern Pointer ptr;

extern volatile uint8_t gMode, gExpr, gTheme;
extern volatile bool gStateDirty;    // set when the device changes state itself -> pushed to web clients

uint16_t themeColor();

// net.cpp
void netBegin();
void netLoop();
bool netOnline();
String netStaIP();
String netDomain();      // name of the page on the main Wi-Fi: HOSTNAME.local
String netApInfo();      // "" when the setup network is off
uint8_t netClients();
bool netEverHadClient();

// feeds.cpp: fetched in the background; read with musicNow() / weatherNow() (copies)
enum MusicState : uint8_t { MU_OFF, MU_WAIT, MU_IDLE, MU_PLAY, MU_PAUSE, MU_ERROR };   // MU_OFF = no keys
struct Music {
  uint8_t state;
  char title[80], artist[80], note[40];   // code page 437, ready to draw; note explains MU_ERROR
  uint32_t progressMs, durationMs, at;    // progress as of millis() == at
};
struct Weather {
  bool valid;
  char city[32];
  int temp, feels, humidity, wind, code;      // now: deg C, %, km/h, WMO weather code
  int dCode[4], dMax[4], dMin[4], dDay[4];    // today + 3 days; dDay 0 = Sunday
};
void feedsBegin();
Music musicNow();
Weather weatherNow();

// glow.cpp: glowing dot-matrix text drawn straight into the canvas. Per frame: glowBegin,
// draw, glowEnd -> the rows that changed, [y0, y1), so only those go to the panel.
struct Band { int16_t y0, y1; };
void glowBegin(Arduino_Canvas *g, bool fresh);   // fresh: start from a black screen
void glowText(const char *s, int x, int y, int scale, uint8_t gain = 255);   // code page 437; scale = px per font pixel
int glowWidth(const char *s, int scale);
Band glowEnd();
void toCP437(const char *utf8, char *out, size_t n);

// One frame of each screen.
void faceDraw(uint32_t now);
void musicDraw(uint32_t now);
void weatherDraw(uint32_t now);
void desktopDraw(Arduino_GFX *g, uint32_t now);
