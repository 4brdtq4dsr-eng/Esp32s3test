#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Arduino_GFX_Library.h>
#include <vector>

// -------------------- Hardware mapping --------------------
static constexpr int PIN_TFT_SCK = 12;
static constexpr int PIN_TFT_MOSI = 11;
static constexpr int PIN_TFT_CS = 10;
static constexpr int PIN_TFT_DC = 9;
static constexpr int PIN_TFT_RST = 14;

static constexpr int PIN_JOY_UP = 4;
static constexpr int PIN_JOY_DOWN = 5;
static constexpr int PIN_JOY_LEFT = 6;
static constexpr int PIN_JOY_RIGHT = 7;
static constexpr int PIN_JOY_MID = 16;

// Tactile buttons (adjust if your PCB differs)
static constexpr int PIN_BTN_CURSOR = 1;
static constexpr int PIN_BTN_INTERACT = 2;
static constexpr int PIN_BTN_BACK = 15;
static constexpr int PIN_BTN_EXIT = 17;

static constexpr int SCREEN_W = 170;
static constexpr int SCREEN_H = 320;
static constexpr int TOPBAR_H = 20;

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, -1);
Arduino_GFX *gfx = new Arduino_ST7789(bus, PIN_TFT_RST, 1 /*rotation*/, true /*IPS*/, SCREEN_W, SCREEN_H, 35 /*x offset*/, 0 /*y offset*/);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

struct Theme {
  uint16_t bg;
  uint16_t fg;
  uint16_t accent;
  uint16_t danger;
};

Theme themes[] = {
    {0x0000, 0xFFFF, 0x07FF, 0xF800},
    {0x18E3, 0xFFFF, 0xFFE0, 0xF81F},
    {0xFFFF, 0x0000, 0x001F, 0xF800},
};
uint8_t themeIndex = 0;

struct InputState {
  bool up, down, left, right, mid;
  bool cursor, interact, back, exit;
  bool upPressed, downPressed, leftPressed, rightPressed, midPressed;
  bool cursorPressed, interactPressed, backPressed, exitPressed;
} input{};

enum class ScreenMode { MENU, APP_STORE, GAME };
ScreenMode mode = ScreenMode::MENU;

int menuIndex = 0;
const char *menuItems[2] = {"App Store", "2D Game"};

struct Rect { int16_t x, y, w, h; };
std::vector<Rect> dirty;

void markDirty(int x, int y, int w, int h) {
  dirty.push_back({(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h});
}

void flushDirty() {
  for (auto &r : dirty) {
    gfx->fillRect(r.x, r.y, r.w, r.h, themes[themeIndex].bg);
  }
  dirty.clear();
}

struct Player {
  float x = 10, y = 250;
  float vx = 0, vy = 0;
  bool onGround = false;
  bool alive = true;
  bool doubleJumpUsed = false;
};

Player player;
int currentLevel = 1;
bool levelComplete = false;
uint32_t levelStart = 0;
int lives = 0;

// Per-level state
bool l3DoorGrabbed = false;
int l4Digits[3] = {0, 0, 0};
int l4Input[3] = {-1, -1, -1};
int l4Index = 0;
bool l5Floor = false, l5Lever = false;
bool l6Drawing = false;
int l6LineLen = 0;
bool l7StoneHit = false;
uint32_t l8EndTime = 0;
int l9Platforms[3] = {0, 0, 0};
bool l10RestartPressed = false;
int l11State = 0;
bool l12Corpse = false;
std::vector<int> l14Sequence;
std::vector<int> l14Input;
int l16Sides = 3;
int l17Answer = 0;
bool l18Dragging = false;
int l19Blocks = 0;
bool l20Selecting = false;
int l20RectW = 10;

bool stopwatchRunning = false;
uint32_t stopwatchMs = 0;
uint32_t stopwatchLast = 0;
int offsetCycle = 0;

void resetLevel();

bool readActiveLow(int pin) { return digitalRead(pin) == LOW; }

void updateInput() {
  static InputState prev{};
  input.up = readActiveLow(PIN_JOY_UP);
  input.down = readActiveLow(PIN_JOY_DOWN);
  input.left = readActiveLow(PIN_JOY_LEFT);
  input.right = readActiveLow(PIN_JOY_RIGHT);
  input.mid = readActiveLow(PIN_JOY_MID);
  input.cursor = readActiveLow(PIN_BTN_CURSOR);
  input.interact = readActiveLow(PIN_BTN_INTERACT);
  input.back = readActiveLow(PIN_BTN_BACK);
  input.exit = readActiveLow(PIN_BTN_EXIT);

  input.upPressed = input.up && !prev.up;
  input.downPressed = input.down && !prev.down;
  input.leftPressed = input.left && !prev.left;
  input.rightPressed = input.right && !prev.right;
  input.midPressed = input.mid && !prev.mid;
  input.cursorPressed = input.cursor && !prev.cursor;
  input.interactPressed = input.interact && !prev.interact;
  input.backPressed = input.back && !prev.back;
  input.exitPressed = input.exit && !prev.exit;
  prev = input;
}

void drawTopBar() {
  gfx->fillRect(0, 0, SCREEN_W, TOPBAR_H, themes[themeIndex].accent);
  gfx->setTextColor(themes[themeIndex].fg);
  gfx->setCursor(2, 6);
  gfx->printf("BAT:%d%%", 100 - (millis() / 10000) % 30);

  if (!timeClient.isTimeSet()) timeClient.begin();
  timeClient.update();
  unsigned long epoch = timeClient.getEpochTime();
  int hh = (epoch % 86400L) / 3600;
  int mm = (epoch % 3600) / 60;
  int dd = ((epoch / 86400L) % 30) + 1;
  int mon = ((epoch / (86400L * 30)) % 12) + 1;
  int yyyy = 2026;

  gfx->setCursor(58, 6);
  gfx->printf("%02d:%02d", hh, mm);
  gfx->setCursor(102, 6);
  gfx->printf("%02d/%02d/%d", dd, mon, yyyy);
  gfx->setCursor(148, 6);
  gfx->print("PWR");
}

void drawMenu() {
  gfx->fillRect(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H, themes[themeIndex].bg);
  for (int i = 0; i < 2; ++i) {
    int y = TOPBAR_H + 36 + i * 48 + (sin((millis() / 200.0) + i) * 2);
    uint16_t c = (i == menuIndex) ? themes[themeIndex].accent : themes[themeIndex].fg;
    gfx->setTextColor(c);
    gfx->setCursor(20, y);
    gfx->setTextSize(i == menuIndex ? 2 : 1);
    gfx->print(menuItems[i]);
  }
  gfx->setTextSize(1);
}

void drawPlayer() {
  gfx->fillRect((int)player.x, (int)player.y, 8, 8, themes[themeIndex].fg);
}

bool intersects(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
  return !(x1 + w1 <= x2 || x2 + w2 <= x1 || y1 + h1 <= y2 || y2 + h2 <= y1);
}

void updatePhysics() {
  player.vy += 0.35f;
  player.x += player.vx;
  player.y += player.vy;
  if (player.y >= 280) {
    player.y = 280;
    player.vy = 0;
    player.onGround = true;
    player.doubleJumpUsed = false;
  } else {
    player.onGround = false;
  }
}

void failLevel() {
  lives++;
  resetLevel();
}

void nextLevel() {
  if (currentLevel < 20) currentLevel++;
  else currentLevel = 1;
  resetLevel();
}

void resetLevel() {
  player = Player{};
  player.x = 12;
  player.y = 280;
  levelStart = millis();
  levelComplete = false;
  l3DoorGrabbed = false;
  l4Index = 0;
  for (int &n : l4Input) n = -1;
  l5Floor = false;
  l5Lever = false;
  l6Drawing = false;
  l6LineLen = 0;
  l7StoneHit = false;
  l8EndTime = millis() + random(10000, 15000);
  l9Platforms[0] = l9Platforms[1] = l9Platforms[2] = 0;
  l10RestartPressed = false;
  l11State = 0;
  l12Corpse = false;
  l14Sequence = {random(0, 2), random(0, 2), random(0, 2), random(0, 2)};
  l14Input.clear();
  l16Sides = random(3, 9);
  l17Answer = random(3, 7) + random(0, 4);
  l18Dragging = false;
  l19Blocks = 0;
  l20Selecting = false;
  l20RectW = 10;

  l4Digits[0] = random(0, 10);
  l4Digits[1] = random(0, 10);
  l4Digits[2] = random(0, 10);
}

void drawLevelFrame() {
  gfx->fillRect(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H, themes[themeIndex].bg);
  gfx->drawRect(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H, themes[themeIndex].fg);
  gfx->setCursor(4, TOPBAR_H + 6);
  gfx->setTextColor(themes[themeIndex].fg);
  gfx->printf("Level %d", currentLevel);
  gfx->drawRect(150, 280, 12, 20, themes[themeIndex].accent); // door
}

void runLevelLogic() {
  player.vx = 0;
  if (input.left) player.vx = -1.3;
  if (input.right) player.vx = 1.3;

  if (input.midPressed) {
    if (player.onGround) {
      player.vy = -4.8;
      player.onGround = false;
    } else if (currentLevel == 15 && !player.doubleJumpUsed) {
      player.vy = -4.6;
      player.doubleJumpUsed = true;
    }
  }

  switch (currentLevel) {
    case 1: {
      gfx->fillRect(30, 290, 30, 10, themes[themeIndex].fg);
      gfx->fillRect(120, 290, 40, 10, themes[themeIndex].fg);
      if (player.x > 60 && player.x < 120 && player.y > 300) failLevel();
      if (player.x > 145 && player.y > 260) levelComplete = true;
      break;
    }
    case 2: {
      gfx->fillRect(85, 220, 12, 80, themes[themeIndex].fg);
      if (intersects(player.x, player.y, 8, 8, 84, 210, 14, 6)) failLevel(); // invisible block behavior
      if (player.x > 95 && player.y < 230) levelComplete = true;
      if (player.x > 80 && player.x < 90 && player.y > 260) failLevel();
      break;
    }
    case 3: {
      int doorX = l3DoorGrabbed ? (int)player.x + 8 : 150;
      gfx->drawRect(doorX, 280, 12, 20, themes[themeIndex].accent);
      if (input.interact && intersects(player.x, player.y, 8, 8, doorX, 280, 12, 20)) l3DoorGrabbed = true;
      if (!l3DoorGrabbed && player.x > 80) failLevel();
      if (l3DoorGrabbed && player.x > 140) levelComplete = true;
      break;
    }
    case 4: {
      if (millis() - levelStart < 1000) {
        gfx->setCursor(60, 150); gfx->setTextSize(3);
        gfx->printf("%d%d%d", l4Digits[0], l4Digits[1], l4Digits[2]);
        gfx->setTextSize(1);
      } else {
        int val = input.upPressed ? 1 : input.downPressed ? 2 : input.leftPressed ? 3 : input.rightPressed ? 4 : -1;
        if (val >= 0 && l4Index < 3) l4Input[l4Index++] = val;
        if (input.interactPressed && l4Index == 3) {
          bool ok = true;
          for (int i = 0; i < 3; ++i) ok &= ((l4Digits[i] % 4) + 1) == l4Input[i];
          if (ok) levelComplete = true; else failLevel();
        }
      }
      break;
    }
    case 5:
      if (intersects(player.x, player.y, 8, 8, 40, 292, 16, 8)) l5Floor = true;
      if (input.interactPressed && intersects(player.x, player.y, 8, 8, 110, 250, 12, 30)) l5Lever = true;
      gfx->fillRect(40, 292, 16, 8, l5Floor ? themes[themeIndex].accent : themes[themeIndex].fg);
      gfx->drawRect(110, 250, 12, 30, l5Lever ? themes[themeIndex].accent : themes[themeIndex].fg);
      if (l5Lever && !l5Floor) failLevel();
      if (l5Floor && l5Lever && player.x > 145) levelComplete = true;
      break;
    case 6:
      l6Drawing = input.cursor;
      if (l6Drawing && (input.left || input.right)) l6LineLen = min(90, l6LineLen + 2);
      gfx->drawLine(50, 285, 50 + l6LineLen, 285, themes[themeIndex].fg);
      if (!l6Drawing && l6LineLen < 70 && player.x > 55) failLevel();
      if (l6LineLen >= 70 && player.x > 140) levelComplete = true;
      break;
    case 7: {
      int cloneX = SCREEN_W - player.x - 8;
      gfx->fillRect(cloneX, player.y, 8, 8, themes[themeIndex].accent);
      gfx->fillRect(84, 280, 6, 10, themes[themeIndex].fg);
      if (cloneX < 90 && cloneX > 80) l7StoneHit = true;
      if (l7StoneHit && player.x > 145) levelComplete = true;
      if (!l7StoneHit && player.x > 120) failLevel();
      break;
    }
    case 8: {
      int rx = (millis() / 300 * 17) % SCREEN_W;
      int ry = (millis() / 8) % 300 + TOPBAR_H;
      gfx->fillCircle(rx, ry, 3, themes[themeIndex].danger);
      if (intersects(player.x, player.y, 8, 8, rx - 3, ry - 3, 6, 6)) failLevel();
      if (millis() > l8EndTime) levelComplete = true;
      break;
    }
    case 9:
      if (input.interact) {
        if (input.up) l9Platforms[0]++;
        if (input.right) l9Platforms[1]++;
        if (input.left) l9Platforms[2]++;
      }
      for (int i = 0; i < 3; ++i) gfx->fillRect(40 + i * 35, 260 - l9Platforms[i] * 8, 24, 6, themes[themeIndex].fg);
      if (l9Platforms[0] > 2 && l9Platforms[1] > 2 && l9Platforms[2] > 2 && player.x > 145) levelComplete = true;
      if (player.x > 70 && (l9Platforms[0] < 2 || l9Platforms[1] < 2)) failLevel();
      break;
    case 10:
      gfx->drawRect(10, 250, 80, 50, themes[themeIndex].fg);
      if (input.backPressed) l10RestartPressed = true;
      if (!l10RestartPressed) gfx->drawLine(90, 250, 90, 300, themes[themeIndex].fg);
      if (l10RestartPressed && player.x > 145) levelComplete = true;
      break;
    case 11:
      if (input.interactPressed) l11State++;
      gfx->setCursor(20, 150);
      gfx->printf("Align hands: %d/4", l11State);
      if (l11State >= 4) levelComplete = true;
      break;
    case 12:
      gfx->fillRect(70, 290, 20, 10, themes[themeIndex].danger);
      if (intersects(player.x, player.y, 8, 8, 70, 290, 20, 10) && !l12Corpse) {
        l12Corpse = true;
        player.x = 20;
      }
      if (l12Corpse) gfx->fillRect(75, 282, 10, 8, themes[themeIndex].fg);
      if (l12Corpse && player.x > 145) levelComplete = true;
      break;
    case 13:
      gfx->drawTriangle(60, 260, 110, 260, 85, 220, themes[themeIndex].fg);
      if (player.x > 145) levelComplete = true;
      break;
    case 14:
      gfx->setCursor(10, 120); gfx->print("Replay tone: UP=high DOWN=low");
      if (input.upPressed) l14Input.push_back(1);
      if (input.downPressed) l14Input.push_back(0);
      if (l14Input.size() == l14Sequence.size()) {
        if (l14Input == l14Sequence) levelComplete = true;
        else failLevel();
      }
      break;
    case 15:
      gfx->fillRect(100, 240, 12, 60, themes[themeIndex].fg);
      if (player.x > 112 && player.y > 250) failLevel();
      if (player.x > 145 && player.y < 250) levelComplete = true;
      break;
    case 16: {
      gfx->setCursor(25, 120); gfx->printf("Count sides: %d?", l16Sides);
      static int answer = 0;
      if (input.upPressed) answer = min(12, answer + 1);
      if (input.downPressed) answer = max(0, answer - 1);
      gfx->setCursor(60, 150); gfx->printf("%d", answer);
      if (input.interactPressed) {
        if (answer == l16Sides) levelComplete = true; else failLevel();
        answer = 0;
      }
      break;
    }
    case 17: {
      static int ans = 0;
      gfx->setCursor(15, 120); gfx->printf("Acute=1 Obtuse=2 Sum?");
      if (input.upPressed) ans++;
      if (input.downPressed) ans = max(0, ans - 1);
      gfx->setCursor(70, 150); gfx->printf("%d", ans);
      if (input.interactPressed) {
        if (ans == l17Answer) levelComplete = true; else failLevel();
        ans = 0;
      }
      break;
    }
    case 18:
      l18Dragging = input.cursor;
      if (!l18Dragging && player.x > 40) failLevel();
      gfx->drawRect(30, 100, 110, 180, themes[themeIndex].fg);
      if (l18Dragging && player.x > 145) levelComplete = true;
      break;
    case 19:
      if (input.cursor && input.interactPressed) l19Blocks++;
      gfx->setCursor(10, 120); gfx->printf("Blocks: %d", l19Blocks);
      if (l19Blocks >= 3) gfx->drawLine(80, 285, 140, 285, themes[themeIndex].fg);
      if (l19Blocks >= 3 && player.x > 145) levelComplete = true;
      break;
    case 20:
      l20Selecting = input.cursor && input.interact;
      if (l20Selecting && input.right) l20RectW = min(70, l20RectW + 2);
      gfx->drawRect(50, 260, l20RectW, 20, themes[themeIndex].fg);
      if (l20RectW >= 50 && player.x > 145) levelComplete = true;
      break;
  }

  updatePhysics();
  if (player.x < 0) player.x = 0;
  if (player.x > SCREEN_W - 8) player.x = SCREEN_W - 8;

  if (player.y > SCREEN_H) failLevel();
  if (levelComplete) nextLevel();
}

void drawGame() {
  drawLevelFrame();
  runLevelLogic();
  drawPlayer();
}

void drawAppStore() {
  gfx->fillRect(0, TOPBAR_H, SCREEN_W, SCREEN_H - TOPBAR_H, themes[themeIndex].bg);
  gfx->setTextColor(themes[themeIndex].fg);
  gfx->setCursor(20, 40);
  gfx->print("App Store");
  gfx->setCursor(20, 60);
  gfx->print("Placeholder - empty");

  if (input.midPressed) stopwatchRunning = !stopwatchRunning;
  if (input.upPressed) stopwatchMs = 0;
  if (input.downPressed) offsetCycle = (offsetCycle + 1) % 4;
  if (input.leftPressed || input.rightPressed) themeIndex = (themeIndex + 1) % 3;

  uint32_t now = millis();
  if (stopwatchRunning) stopwatchMs += now - stopwatchLast;
  stopwatchLast = now;

  gfx->setCursor(20, 100);
  gfx->printf("Stopwatch: %lu.%03lus", stopwatchMs / 1000, stopwatchMs % 1000);
  gfx->setCursor(20, 120);
  gfx->printf("Offset cycle: %d", offsetCycle);
}

void setupPins() {
  for (int p : {PIN_JOY_UP, PIN_JOY_DOWN, PIN_JOY_LEFT, PIN_JOY_RIGHT, PIN_JOY_MID,
                PIN_BTN_CURSOR, PIN_BTN_INTERACT, PIN_BTN_BACK, PIN_BTN_EXIT}) {
    pinMode(p, INPUT_PULLUP);
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  setupPins();
  gfx->begin();
  gfx->fillScreen(BLACK);
  gfx->setTextSize(1);
  timeClient.begin();
  resetLevel();
}

void loop() {
  updateInput();

  if (input.exitPressed) {
    mode = ScreenMode::MENU;
  }

  if (mode == ScreenMode::MENU) {
    if (input.upPressed) menuIndex = (menuIndex + 1) % 2;
    if (input.downPressed) menuIndex = (menuIndex + 1) % 2;
    if (input.leftPressed || input.rightPressed) themeIndex = (themeIndex + 1) % 3;
    if (input.midPressed) {
      mode = (menuIndex == 0) ? ScreenMode::APP_STORE : ScreenMode::GAME;
    }
    drawTopBar();
    drawMenu();
  } else if (mode == ScreenMode::APP_STORE) {
    drawTopBar();
    drawAppStore();
  } else if (mode == ScreenMode::GAME) {
    if (input.backPressed) resetLevel();
    drawTopBar();
    drawGame();
  }

  flushDirty();
  delay(16);
}
