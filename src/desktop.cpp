// A retro beveled-gray desktop driven by the remote mouse: icons (double-click),
// a start menu, a taskbar clock, and draggable windows (Clock, Paint, System).
// Everything is drawn at roughly 2x classic pixel size so it reads on a 4" panel.

#include "app.h"
#include <WiFi.h>
#include <time.h>

typedef Arduino_GFX G;

// ---------------------------------------------------------------- palette
static const uint16_t C_DESK = RGB565(0, 128, 128);
static const uint16_t C_FACE = RGB565(192, 192, 192);
static const uint16_t C_LIGHT = RGB565(223, 223, 223);
static const uint16_t C_SHADOW = RGB565(128, 128, 128);
static const uint16_t C_TITLE = RGB565(0, 0, 128);
static const uint16_t C_WHITE = RGB565_WHITE, C_BLACK = RGB565_BLACK;

static const int TASK_H = 40, TASK_Y = SCREEN_H - TASK_H;
static const int TITLE_H = 28;

// ---------------------------------------------------------------- drawing helpers

static void bevel(G *g, int x, int y, int w, int h, bool raised, uint16_t fill = C_FACE) {
  g->fillRect(x, y, w, h, fill);
  uint16_t o1 = raised ? C_LIGHT : C_SHADOW, o2 = raised ? C_BLACK : C_WHITE;
  uint16_t i1 = raised ? C_WHITE : C_BLACK, i2 = raised ? C_SHADOW : C_LIGHT;
  g->drawFastHLine(x, y, w, o1);             g->drawFastVLine(x, y, h, o1);
  g->drawFastHLine(x, y + h - 1, w, o2);     g->drawFastVLine(x + w - 1, y, h, o2);
  g->drawFastHLine(x + 1, y + 1, w - 2, i1); g->drawFastVLine(x + 1, y + 1, h - 2, i1);
  g->drawFastHLine(x + 1, y + h - 2, w - 2, i2); g->drawFastVLine(x + w - 2, y + 1, h - 2, i2);
}

static void text(G *g, int x, int y, const char *s, uint16_t c, int size = 2) {
  g->setTextSize(size);
  g->setTextColor(c);
  g->setCursor(x, y);
  g->print(s);
}

static bool inside(int px, int py, int x, int y, int w, int h) {
  return px >= x && py >= y && px < x + w && py < y + h;
}

// Classic arrow, drawn at 2x. X = outline, o = fill.
static const char *const ARROW[] = {
    "X           ", "XX          ", "XoX         ", "XooX        ", "XoooX       ",
    "XooooX      ", "XoooooX     ", "XooooooX    ", "XoooooooX   ", "XooooooooX  ",
    "XoooooooooX ", "XooooooXXXXX", "XoooXooX    ", "XooX XooX   ", "XoX  XooX   ",
    "XX    XooX  ", "X     XooX  ", "       XooX ", "       XXX  ",
};

static void drawCursor(G *g, int x, int y) {
  for (int r = 0; r < 19; r++)
    for (int c = 0; c < 12; c++) {
      char ch = ARROW[r][c];
      if (ch == 'X') g->fillRect(x + c * 2, y + r * 2, 2, 2, C_BLACK);
      else if (ch == 'o') g->fillRect(x + c * 2, y + r * 2, 2, 2, C_WHITE);
    }
}

static void timeStr(char *out, size_t n, bool seconds) {
  struct tm t;
  if (!getLocalTime(&t, 0)) { snprintf(out, n, seconds ? "--:--:--" : "--:--"); return; }
  strftime(out, n, seconds ? "%H:%M:%S" : "%H:%M", &t);
}

// ---------------------------------------------------------------- windows

enum WinKind : uint8_t { W_CLOCK, W_PAINT, W_SYSTEM, W_COUNT };

struct Win {
  const char *title;
  int16_t x, y, w, h;
};
static Win wins[W_COUNT] = {
    {"Clock", 150, 40, 270, 150},
    {"Paint", 110, 70, 316, 290},
    {"System", 130, 100, 300, 236},
};
static uint8_t order[W_COUNT];   // open windows, back to front
static uint8_t nOpen = 0;

static int findOpen(uint8_t k) {
  for (int i = 0; i < nOpen; i++) if (order[i] == k) return i;
  return -1;
}
static void focusWin(uint8_t k) {
  int i = findOpen(k);
  if (i < 0) { order[nOpen++] = k; return; }
  for (; i < nOpen - 1; i++) order[i] = order[i + 1];
  order[nOpen - 1] = k;
}
static void closeWin(uint8_t k) {
  int i = findOpen(k);
  if (i < 0) return;
  for (; i < nOpen - 1; i++) order[i] = order[i + 1];
  nOpen--;
}
static bool isFront(uint8_t k) { return nOpen && order[nOpen - 1] == k; }

static void closeBox(const Win &w, int &x, int &y) { x = w.x + w.w - 30; y = w.y + 7; }

// --- Paint: a little bitmap in PSRAM you scribble on
static const int PAINT_W = 300, PAINT_H = 200;
static uint16_t *paintBuf = nullptr;
static const uint16_t PAINT_COLORS[] = {C_BLACK, RGB565(255, 0, 0), RGB565(0, 160, 0),
                                        RGB565(0, 0, 255), RGB565(255, 200, 0), C_WHITE};
static uint8_t paintColor = 0;
static int lastPX = -1, lastPY = -1;

static void paintRect(const Win &w, int &x, int &y) { x = w.x + 8; y = w.y + TITLE_H + 12; }

static void paintClear() {
  if (paintBuf) for (int i = 0; i < PAINT_W * PAINT_H; i++) paintBuf[i] = C_WHITE;
}

static void paintDot(int x, int y, uint16_t c) {
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      if (dx * dx + dy * dy > 5) continue;
      int px = x + dx, py = y + dy;
      if (px >= 0 && py >= 0 && px < PAINT_W && py < PAINT_H) paintBuf[py * PAINT_W + px] = c;
    }
}

static void paintLine(int x0, int y0, int x1, int y1) {
  uint16_t c = PAINT_COLORS[paintColor];
  int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
  while (true) {
    paintDot(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void drawClock(G *g, const Win &w) {
  char t[12];
  timeStr(t, sizeof t, true);
  bevel(g, w.x + 10, w.y + TITLE_H + 14, w.w - 20, 64, false, C_BLACK);
  text(g, w.x + 135 - 96, w.y + TITLE_H + 30, t, RGB565(60, 255, 110), 4);
  struct tm tm;
  if (getLocalTime(&tm, 0)) {
    char d[24];
    strftime(d, sizeof d, "%a %d %b %Y", &tm);
    text(g, w.x + 135 - strlen(d) * 6, w.y + TITLE_H + 92, d, C_BLACK);
  } else {
    text(g, w.x + 16, w.y + TITLE_H + 92, "waiting for time", C_SHADOW);
  }
}

static void drawPaint(G *g, const Win &w) {
  int px, py;
  paintRect(w, px, py);
  bevel(g, px - 2, py - 2, PAINT_W + 4, PAINT_H + 4, false);
  if (paintBuf) g->draw16bitRGBBitmap(px, py, paintBuf, PAINT_W, PAINT_H);
  int sy = py + PAINT_H + 10;
  for (int i = 0; i < 6; i++) {
    int sx = px + i * 34;
    bevel(g, sx, sy, 30, 30, i != paintColor);
    g->fillRect(sx + 5, sy + 5, 20, 20, PAINT_COLORS[i]);
  }
  bevel(g, px + PAINT_W - 86, sy, 86, 30, true);
  text(g, px + PAINT_W - 73, sy + 8, "Clear", C_BLACK);
}

static void drawSystem(G *g, const Win &w) {
  char line[40];
  int x = w.x + 14, y = w.y + TITLE_H + 16;
  uint32_t s = millis() / 1000;
  auto row = [&](const char *l) { text(g, x, y, l, C_BLACK); y += 24; };
  row("CRTFace v0.1");
  snprintf(line, sizeof line, "IP   %s", netOnline() ? netStaIP().c_str() : "offline"); row(line);
  if (netOnline()) { snprintf(line, sizeof line, "WiFi %d dBm", WiFi.RSSI()); row(line); }
  snprintf(line, sizeof line, "Up   %luh %02lum %02lus", s / 3600, (s / 60) % 60, s % 60); row(line);
  snprintf(line, sizeof line, "RAM  %u KB free", (unsigned)(ESP.getFreeHeap() / 1024)); row(line);
  snprintf(line, sizeof line, "PSRAM %u KB free", (unsigned)(ESP.getFreePsram() / 1024)); row(line);
  snprintf(line, sizeof line, "Web  %u client%s", netClients(), netClients() == 1 ? "" : "s"); row(line);
}

static void drawWin(G *g, uint8_t k) {
  const Win &w = wins[k];
  bevel(g, w.x, w.y, w.w, w.h, true);
  g->fillRect(w.x + 4, w.y + 4, w.w - 8, TITLE_H, isFront(k) ? C_TITLE : C_SHADOW);
  text(g, w.x + 12, w.y + 11, w.title, isFront(k) ? C_WHITE : C_FACE);
  int cx, cy;
  closeBox(w, cx, cy);
  bevel(g, cx, cy, 24, 22, true);
  for (int d = 0; d < 2; d++) {   // a 2px-thick "x"
    g->drawLine(cx + 7 + d, cy + 6, cx + 15 + d, cy + 15, C_BLACK);
    g->drawLine(cx + 15 + d, cy + 6, cx + 7 + d, cy + 15, C_BLACK);
  }
  switch (k) {
    case W_CLOCK: drawClock(g, w); break;
    case W_PAINT: drawPaint(g, w); break;
    case W_SYSTEM: drawSystem(g, w); break;
  }
}

// ---------------------------------------------------------------- icons

enum IconKind : uint8_t { I_FACE, I_CLOCK, I_PAINT, I_SYSTEM, I_COUNT };
static const char *const ICON_LABEL[I_COUNT] = {"Face", "Clock", "Paint", "System"};
static const int ICON_X = 14, ICON_Y = 16, ICON_STEP = 92, ICON_W = 80, ICON_H = 84;
static int8_t selIcon = -1, lastClickIcon = -1;
static uint32_t lastClickMs = 0;

static void drawIconArt(G *g, uint8_t k, int x, int y) {   // 48x48 at (x,y)
  switch (k) {
    case I_FACE:   // little CRT monitor with a face
      bevel(g, x + 2, y + 2, 44, 36, true);
      g->fillRect(x + 7, y + 7, 34, 26, C_BLACK);
      g->fillRoundRect(x + 13, y + 12, 7, 10, 2, themeColor());
      g->fillRoundRect(x + 28, y + 12, 7, 10, 2, themeColor());
      g->drawFastHLine(x + 19, y + 27, 10, themeColor());
      g->fillRect(x + 16, y + 38, 16, 6, C_FACE);
      g->fillRect(x + 10, y + 44, 28, 4, C_SHADOW);
      break;
    case I_CLOCK:
      g->fillCircle(x + 24, y + 24, 22, C_BLACK);
      g->fillCircle(x + 24, y + 24, 19, C_WHITE);
      g->drawLine(x + 24, y + 24, x + 24, y + 10, C_BLACK);
      g->drawLine(x + 25, y + 24, x + 25, y + 10, C_BLACK);
      g->drawLine(x + 24, y + 24, x + 34, y + 28, C_TITLE);
      g->drawLine(x + 24, y + 25, x + 34, y + 29, C_TITLE);
      break;
    case I_PAINT:
      bevel(g, x + 4, y + 4, 40, 40, false, C_WHITE);
      for (int i = 0; i < 3; i++) g->drawLine(x + 10, y + 32 + i, x + 36, y + 12 + i, RGB565(255, 0, 0));
      g->fillCircle(x + 16, y + 16, 5, RGB565(0, 0, 255));
      g->fillRect(x + 26, y + 30, 10, 8, RGB565(0, 160, 0));
      break;
    case I_SYSTEM:
      g->fillCircle(x + 24, y + 24, 21, C_TITLE);
      g->fillCircle(x + 24, y + 13, 3, C_WHITE);
      g->fillRect(x + 21, y + 20, 7, 18, C_WHITE);
      break;
  }
}

static void drawIcons(G *g) {
  for (int i = 0; i < I_COUNT; i++) {
    int x = ICON_X, y = ICON_Y + i * ICON_STEP;
    drawIconArt(g, i, x + 16, y);
    const char *l = ICON_LABEL[i];
    int tw = strlen(l) * 12, tx = x + ICON_W / 2 - tw / 2;
    if (selIcon == i) g->fillRect(tx - 3, y + 54, tw + 6, 20, C_TITLE);
    text(g, tx, y + 57, l, C_WHITE);
  }
}

// ---------------------------------------------------------------- start menu + taskbar

enum MenuItem : uint8_t { M_FACE, M_CLOCK, M_PAINT, M_SYSTEM, M_SLEEP, M_COUNT };
static const char *const MENU_LABEL[M_COUNT] = {"Face mode", "Clock", "Paint", "System", "Sleep..."};
static const int MENU_W = 220, MENU_ITEM_H = 40, MENU_H = M_COUNT * MENU_ITEM_H + 14;
static const int MENU_X = 2, MENU_Y = TASK_Y - MENU_H;
static bool startOpen = false;

static void drawStartButton(G *g) {
  bevel(g, 4, TASK_Y + 5, 100, 31, !startOpen);
  // tiny CRT logo
  g->fillRect(13, TASK_Y + 12, 20, 15, C_BLACK);
  g->fillRect(17, TASK_Y + 16, 3, 4, themeColor());
  g->fillRect(26, TASK_Y + 16, 3, 4, themeColor());
  g->fillRect(18, TASK_Y + 28, 10, 3, C_SHADOW);
  text(g, 40, TASK_Y + 13, "Start", C_BLACK);
}

static void drawTaskbar(G *g) {
  g->fillRect(0, TASK_Y, SCREEN_W, TASK_H, C_FACE);
  g->drawFastHLine(0, TASK_Y, SCREEN_W, C_LIGHT);
  g->drawFastHLine(0, TASK_Y + 1, SCREEN_W, C_WHITE);
  drawStartButton(g);
  // a button per open window, in the order they were opened (stable left to right)
  int x = 112;
  for (int k = 0; k < W_COUNT; k++) {
    if (findOpen(k) < 0) continue;
    bevel(g, x, TASK_Y + 5, 90, 31, !isFront(k));
    text(g, x + 9, TASK_Y + 13, wins[k].title, C_BLACK);
    x += 94;
  }
  char t[8];
  timeStr(t, sizeof t, false);
  bevel(g, SCREEN_W - 88, TASK_Y + 5, 84, 31, false);
  text(g, SCREEN_W - 76, TASK_Y + 13, t, C_BLACK);
}

static int taskButtonAt(int px, int py) {   // window kind under the cursor in the taskbar, or -1
  int x = 112;
  for (int k = 0; k < W_COUNT; k++) {
    if (findOpen(k) < 0) continue;
    if (inside(px, py, x, TASK_Y + 5, 90, 31)) return k;
    x += 94;
  }
  return -1;
}

static void drawMenu(G *g, int hx, int hy) {
  bevel(g, MENU_X, MENU_Y, MENU_W, MENU_H, true);
  g->fillRect(MENU_X + 4, MENU_Y + 4, 28, MENU_H - 8, C_SHADOW);   // side band
  const char *band = "CRT";
  for (int i = 0; i < 3; i++) {
    char s[2] = {band[i], 0};
    text(g, MENU_X + 12, MENU_Y + MENU_H - 72 + i * 22, s, C_FACE);
  }
  for (int i = 0; i < M_COUNT; i++) {
    int y = MENU_Y + 7 + i * MENU_ITEM_H;
    bool hot = inside(hx, hy, MENU_X + 34, y, MENU_W - 40, MENU_ITEM_H);
    if (hot) g->fillRect(MENU_X + 34, y, MENU_W - 40, MENU_ITEM_H, C_TITLE);
    if (i == M_SLEEP) g->drawFastHLine(MENU_X + 38, y - 1, MENU_W - 48, C_SHADOW);
    text(g, MENU_X + 44, y + 13, MENU_LABEL[i], hot ? C_WHITE : C_BLACK);
  }
}

// ---------------------------------------------------------------- input

enum Drag : uint8_t { D_NONE, D_WIN, D_PAINT };
static Drag drag = D_NONE;
static uint8_t dragWin = 0;
static int dragDX = 0, dragDY = 0;

static void openWin(uint8_t k) { focusWin(k); }

static void runMenu(uint8_t item) {
  switch (item) {
    case M_FACE: gMode = MODE_FACE; gStateDirty = true; break;
    case M_CLOCK: openWin(W_CLOCK); break;
    case M_PAINT: openWin(W_PAINT); break;
    case M_SYSTEM: openWin(W_SYSTEM); break;
    case M_SLEEP: gMode = MODE_FACE; gExpr = EX_SLEEPY; gStateDirty = true; break;
  }
}

static void openIcon(uint8_t i) {
  switch (i) {
    case I_FACE: gMode = MODE_FACE; gStateDirty = true; break;
    case I_CLOCK: openWin(W_CLOCK); break;
    case I_PAINT: openWin(W_PAINT); break;
    case I_SYSTEM: openWin(W_SYSTEM); break;
  }
}

static void windowPress(uint8_t k, int x, int y) {
  if (k != W_PAINT) return;
  const Win &w = wins[k];
  int px, py;
  paintRect(w, px, py);
  if (inside(x, y, px, py, PAINT_W, PAINT_H)) {
    drag = D_PAINT;
    lastPX = x - px; lastPY = y - py;
    if (paintBuf) paintLine(lastPX, lastPY, lastPX, lastPY);
    return;
  }
  int sy = py + PAINT_H + 10;
  for (int i = 0; i < 6; i++) if (inside(x, y, px + i * 34, sy, 30, 30)) paintColor = i;
  if (inside(x, y, px + PAINT_W - 86, sy, 86, 30)) paintClear();
}

static void onPress(int x, int y, uint32_t now) {
  if (startOpen) {
    startOpen = false;
    if (inside(x, y, MENU_X, MENU_Y, MENU_W, MENU_H)) {
      int i = (y - MENU_Y - 7) / MENU_ITEM_H;
      if (i >= 0 && i < M_COUNT && x > MENU_X + 34) runMenu(i);
      return;
    }
    if (inside(x, y, 4, TASK_Y + 5, 100, 31)) return;   // clicking Start again closes it
  }
  if (inside(x, y, 4, TASK_Y + 5, 100, 31)) { startOpen = true; return; }

  for (int i = nOpen - 1; i >= 0; i--) {   // front to back
    uint8_t k = order[i];
    Win &w = wins[k];
    if (!inside(x, y, w.x, w.y, w.w, w.h)) continue;
    focusWin(k);
    int cx, cy;
    closeBox(w, cx, cy);
    if (inside(x, y, cx, cy, 24, 22)) { closeWin(k); return; }
    if (y < w.y + 4 + TITLE_H) { drag = D_WIN; dragWin = k; dragDX = x - w.x; dragDY = y - w.y; return; }
    windowPress(k, x, y);
    return;
  }

  if (y >= TASK_Y) {
    int k = taskButtonAt(x, y);
    if (k >= 0) focusWin(k);
    return;
  }

  for (int i = 0; i < I_COUNT; i++) {
    if (!inside(x, y, ICON_X, ICON_Y + i * ICON_STEP, ICON_W, ICON_H)) continue;
    if (lastClickIcon == i && now - lastClickMs < 600) { openIcon(i); lastClickIcon = -1; }
    else { lastClickIcon = i; lastClickMs = now; }
    selIcon = i;
    return;
  }
  selIcon = -1;
}

static void onHold(int x, int y) {
  if (drag == D_WIN) {
    Win &w = wins[dragWin];
    w.x = constrain(x - dragDX, 60 - w.w, SCREEN_W - 60);
    w.y = constrain(y - dragDY, 0, TASK_Y - TITLE_H);
  } else if (drag == D_PAINT && paintBuf) {
    int px, py;
    paintRect(wins[W_PAINT], px, py);
    int nx = x - px, ny = y - py;
    paintLine(lastPX, lastPY, nx, ny);
    lastPX = nx; lastPY = ny;
  }
}

// ---------------------------------------------------------------- frame

void desktopDraw(G *g, uint32_t now) {
  static uint16_t lastPresses = 0;
  if (!paintBuf) {
    paintBuf = (uint16_t *)ps_malloc(PAINT_W * PAINT_H * sizeof(uint16_t));
    paintClear();
  }

  int x = ptr.x, y = ptr.y;
  bool down = ptr.down;
  uint16_t presses = ptr.presses;
  if (presses != lastPresses) {
    lastPresses = presses;
    drag = D_NONE;
    onPress(x, y, now);
  }
  if (down) onHold(x, y);
  else drag = D_NONE;

  g->fillScreen(C_DESK);
  drawIcons(g);
  for (int i = 0; i < nOpen; i++) drawWin(g, order[i]);
  drawTaskbar(g);
  if (startOpen) drawMenu(g, x, y);
  drawCursor(g, x, y);
}
