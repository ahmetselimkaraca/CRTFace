// CRTFace - Waveshare ESP32-S3-Touch-LCD-4 (touch unused) in a little CRT body.

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "app.h"

// ---------------------------------------------------------------- display
// Pin map + timings for the board's ST7701 480x480 RGB panel.
static Arduino_DataBus *bus = new Arduino_SWSPI(GFX_NOT_DEFINED, 42 /*CS*/, 2 /*SCK*/, 1 /*MOSI*/, GFX_NOT_DEFINED);
static Arduino_ESP32RGBPanel *panel = new Arduino_ESP32RGBPanel(
    40 /*DE*/, 39 /*VSYNC*/, 38 /*HSYNC*/, 41 /*PCLK*/,
    46, 3, 8, 18, 17,        // R0-R4
    14, 13, 12, 11, 10, 9,   // G0-G5
    5, 45, 48, 47, 21,       // B0-B4
    1, 10, 8, 50,            // hsync polarity, front porch, pulse, back porch
    1, 10, 8, 20);           // vsync polarity, front porch, pulse, back porch
static Arduino_RGB_Display *lcd = new Arduino_RGB_Display(
    SCREEN_W, SCREEN_H, panel, 2 /*rotation*/, true, bus, GFX_NOT_DEFINED,
    st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
// Draw off-screen, then flush once per frame (no flicker). Lives in PSRAM.
static Arduino_Canvas *canvas = new Arduino_Canvas(SCREEN_W, SCREEN_H, lcd);

static void powerOnPanel() {
  // The board's I/O expander (I2C 0x24) controls LCD reset and backlight.
  Wire.begin(15 /*SDA*/, 7 /*SCL*/);
  Wire.beginTransmission(0x24); Wire.write(0x02); Wire.write(0xFF); Wire.endTransmission(); // outputs high
  Wire.beginTransmission(0x24); Wire.write(0x03); Wire.write(0x3A); Wire.endTransmission(); // direction
}

// ---------------------------------------------------------------- shared state

Pointer ptr;
volatile uint8_t gMode = MODE_FACE, gExpr = EX_NEUTRAL, gTheme = TH_GREEN;
volatile bool gStateDirty = false;

uint16_t themeColor() {
  switch (gTheme) {
    case TH_AMBER: return RGB565(255, 176, 0);
    case TH_ICE:   return RGB565(170, 220, 255);
    case TH_PINK:  return RGB565(255, 110, 200);
    default:       return RGB565(60, 255, 110);
  }
}

// ---------------------------------------------------------------- BOOT button

static const int BOOT_BUTTON = 0;

// Tap BOOT: show the next screen (Face -> Music -> Weather -> Face; from Desktop, back to Face).
static void pollButton(uint32_t now) {
  static bool was = false;
  static uint32_t changedAt = 0;
  bool down = digitalRead(BOOT_BUTTON) == LOW;
  if (down == was || now - changedAt < 30) return;   // 30 ms debounce
  was = down;
  changedAt = now;
  if (!down) return;
  gMode = gMode == MODE_FACE ? MODE_MUSIC : gMode == MODE_MUSIC ? MODE_WEATHER : MODE_FACE;
  gStateDirty = true;   // tell the web page
}

// ---------------------------------------------------------------- main

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // never wait on USB serial: with no monitor reading, prints used to stall frames
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  powerOnPanel();
  if (!lcd->begin() || !canvas->begin(GFX_SKIP_OUTPUT_BEGIN)) {
    Serial.println("[lcd] init failed (PSRAM?)");
    while (true) delay(1000);
  }
  Serial.println("[lcd] ok");
  randomSeed(esp_random());

  netBegin();
  feedsBegin();
}

void loop() {
  static uint32_t lastFrame = 0;
  static uint8_t lastMode = 0xFF;
  netLoop();

  uint32_t now = millis();
  pollButton(now);
  if (now - lastFrame < 16) { delay(1); return; }   // cap at ~60 fps
  lastFrame = now;

  uint8_t mode = gMode;
  bool fresh = mode != lastMode;   // switched screens: start from black
  lastMode = mode;
  if (mode == MODE_DESKTOP) {
    desktopDraw(canvas, now);
    canvas->flush();
    return;
  }
  glowBegin(canvas, fresh);
  if (mode == MODE_MUSIC) musicDraw(now);
  else if (mode == MODE_WEATHER) weatherDraw(now);
  else faceDraw(now);
  Band b = glowEnd();   // copy only the changed rows to the panel, not all 480
  if (b.y1 > b.y0) lcd->draw16bitRGBBitmap(0, b.y0, canvas->getFramebuffer() + b.y0 * SCREEN_W, SCREEN_W, b.y1 - b.y0);
}
