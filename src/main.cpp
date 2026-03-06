#include <Arduino.h>
#include <SPI.h>

namespace {

constexpr uint16_t TFT_WIDTH = 170;
constexpr uint16_t TFT_HEIGHT = 320;

constexpr uint8_t PIN_TFT_SCK = 12;
constexpr uint8_t PIN_TFT_MOSI = 11;
constexpr uint8_t PIN_TFT_CS = 10;
constexpr uint8_t PIN_TFT_DC = 9;
constexpr uint8_t PIN_TFT_RST = 14;

constexpr uint8_t PIN_BTN_UP = 4;
constexpr uint8_t PIN_BTN_DOWN = 5;
constexpr uint8_t PIN_BTN_LEFT = 6;
constexpr uint8_t PIN_BTN_RIGHT = 7;
constexpr uint8_t PIN_BTN_MID = 16;

constexpr uint16_t COLOR_BLACK = 0x0000;
constexpr uint16_t COLOR_WHITE = 0xFFFF;
constexpr uint16_t COLOR_RED = 0xF800;
constexpr uint16_t COLOR_GRAY = 0x8410;
constexpr uint16_t COLOR_CYAN = 0x07FF;

constexpr uint16_t DIGIT_W = 24;
constexpr uint16_t DIGIT_H = 46;
constexpr uint16_t DIGIT_THICK = 5;
constexpr uint16_t COLON_W = 10;
constexpr uint16_t DOT_W = 8;
constexpr uint16_t CHAR_SPACING = 4;

struct OffsetPreset {
  uint16_t x;
  uint16_t y;
};

constexpr OffsetPreset OFFSETS[] = {
    {0, 0},
    {35, 0},
    {0, 80},
};

SPIClass tftSpi(FSPI);

class ST7789 {
 public:
  void begin(uint16_t xOffset, uint16_t yOffset) {
    xOffset_ = xOffset;
    yOffset_ = yOffset;

    pinMode(PIN_TFT_CS, OUTPUT);
    pinMode(PIN_TFT_DC, OUTPUT);
    pinMode(PIN_TFT_RST, OUTPUT);

    digitalWrite(PIN_TFT_CS, HIGH);
    digitalWrite(PIN_TFT_DC, HIGH);

    tftSpi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);

    hardwareReset();

    sendCommand(0x01);  // SWRESET
    delay(150);

    sendCommand(0x11);  // SLPOUT
    delay(120);

    sendCommand(0x3A);  // COLMOD
    sendData8(0x55);    // 16-bit

    sendCommand(0x36);  // MADCTL
    sendData8(0x00);    // portrait RGB

    sendCommand(0x21);  // INVON (common for IPS)

    sendCommand(0x13);  // NORON
    sendCommand(0x29);  // DISPON
    delay(20);

    fillScreen(COLOR_BLACK);
  }

  uint16_t width() const { return TFT_WIDTH; }
  uint16_t height() const { return TFT_HEIGHT; }

  void fillScreen(uint16_t color) { fillRect(0, 0, width(), height(), color); }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= static_cast<int16_t>(width()) || y >= static_cast<int16_t>(height())) {
      return;
    }
    setAddrWindow(x, y, 1, 1);
    writeColor(color, 1);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= static_cast<int16_t>(height()) || w <= 0) {
      return;
    }
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (x + w > static_cast<int16_t>(width())) {
      w = width() - x;
    }
    if (w <= 0) {
      return;
    }
    setAddrWindow(x, y, w, 1);
    writeColor(color, w);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= static_cast<int16_t>(width()) || h <= 0) {
      return;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (y + h > static_cast<int16_t>(height())) {
      h = height() - y;
    }
    if (h <= 0) {
      return;
    }
    setAddrWindow(x, y, 1, h);
    writeColor(color, h);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) {
      return;
    }
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (x + w > static_cast<int16_t>(width())) {
      w = width() - x;
    }
    if (y + h > static_cast<int16_t>(height())) {
      h = height() - y;
    }
    if (w <= 0 || h <= 0) {
      return;
    }

    setAddrWindow(x, y, w, h);
    writeColor(color, static_cast<uint32_t>(w) * h);
  }

 private:
  uint16_t xOffset_ = 0;
  uint16_t yOffset_ = 0;

  void hardwareReset() {
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(10);
    digitalWrite(PIN_TFT_RST, LOW);
    delay(20);
    digitalWrite(PIN_TFT_RST, HIGH);
    delay(150);
  }

  void sendCommand(uint8_t cmd) {
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, LOW);
    tftSpi.transfer(cmd);
    digitalWrite(PIN_TFT_CS, HIGH);
  }

  void sendData8(uint8_t data) {
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    tftSpi.transfer(data);
    digitalWrite(PIN_TFT_CS, HIGH);
  }

  void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    const uint16_t xs = x + xOffset_;
    const uint16_t xe = xs + w - 1;
    const uint16_t ys = y + yOffset_;
    const uint16_t ye = ys + h - 1;

    sendCommand(0x2A);
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    tftSpi.transfer16(xs);
    tftSpi.transfer16(xe);
    digitalWrite(PIN_TFT_CS, HIGH);

    sendCommand(0x2B);
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    tftSpi.transfer16(ys);
    tftSpi.transfer16(ye);
    digitalWrite(PIN_TFT_CS, HIGH);

    sendCommand(0x2C);
  }

  void writeColor(uint16_t color, uint32_t count) {
    const uint8_t hi = color >> 8;
    const uint8_t lo = color & 0xFF;

    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    while (count--) {
      tftSpi.transfer(hi);
      tftSpi.transfer(lo);
    }
    digitalWrite(PIN_TFT_CS, HIGH);
  }
};

ST7789 tft;

enum class Theme : uint8_t { White = 0, Red = 1 };

Theme currentTheme = Theme::White;
bool stopwatchRunning = true;
uint8_t offsetPresetIndex = 1;
uint32_t elapsedTenths = 0;
uint32_t lastTickMs = 0;
uint32_t bootTimeMs = 0;
bool showingBoot = true;

struct Button {
  const char* name;
  uint8_t pin;
  uint32_t lastChangeMs;
  bool stable;
  bool rawPrev;
};

Button buttons[] = {
    {"UP", PIN_BTN_UP, 0, true, true},
    {"DOWN", PIN_BTN_DOWN, 0, true, true},
    {"LEFT", PIN_BTN_LEFT, 0, true, true},
    {"RIGHT", PIN_BTN_RIGHT, 0, true, true},
    {"MID", PIN_BTN_MID, 0, true, true},
};

constexpr uint16_t TIME_AREA_W = DIGIT_W * 6 + COLON_W + DOT_W + CHAR_SPACING * 7;
constexpr uint16_t TIME_AREA_H = DIGIT_H;
constexpr uint16_t TIME_AREA_X = (TFT_WIDTH - TIME_AREA_W) / 2;
constexpr uint16_t TIME_AREA_Y = 116;

char previousTimeText[9] = "";

uint16_t fgColor() { return currentTheme == Theme::White ? COLOR_WHITE : COLOR_RED; }

void drawRectOutline(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  tft.drawFastHLine(x, y, w, color);
  tft.drawFastHLine(x, y + h - 1, w, color);
  tft.drawFastVLine(x, y, h, color);
  tft.drawFastVLine(x + w - 1, y, h, color);
}

void drawSegmentedDigit(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg) {
  tft.fillRect(x, y, DIGIT_W, DIGIT_H, bg);

  if (c == ':') {
    const int16_t dotW = 6;
    const int16_t dotH = 6;
    tft.fillRect(x + 2, y + 12, dotW, dotH, color);
    tft.fillRect(x + 2, y + 28, dotW, dotH, color);
    return;
  }

  if (c == '.') {
    tft.fillRect(x + 1, y + DIGIT_H - 8, 6, 6, color);
    return;
  }

  if (c < '0' || c > '9') {
    return;
  }

  static constexpr uint8_t SEGMENT_MAP[10] = {
      0b1111110,  // 0
      0b0110000,  // 1
      0b1101101,  // 2
      0b1111001,  // 3
      0b0110011,  // 4
      0b1011011,  // 5
      0b1011111,  // 6
      0b1110000,  // 7
      0b1111111,  // 8
      0b1111011   // 9
  };

  const uint8_t bits = SEGMENT_MAP[c - '0'];

  auto hSegment = [&](int16_t segY) {
    tft.fillRect(x + DIGIT_THICK, y + segY, DIGIT_W - 2 * DIGIT_THICK, DIGIT_THICK, color);
  };

  auto vSegmentL = [&](int16_t segY, int16_t segH) {
    tft.fillRect(x, y + segY, DIGIT_THICK, segH, color);
  };

  auto vSegmentR = [&](int16_t segY, int16_t segH) {
    tft.fillRect(x + DIGIT_W - DIGIT_THICK, y + segY, DIGIT_THICK, segH, color);
  };

  const int16_t upperH = (DIGIT_H / 2) - DIGIT_THICK;
  const int16_t lowerY = DIGIT_H / 2;
  const int16_t lowerH = DIGIT_H / 2 - DIGIT_THICK;

  if (bits & 0b1000000) hSegment(0);                 // a
  if (bits & 0b0100000) vSegmentR(DIGIT_THICK, upperH);   // b
  if (bits & 0b0010000) vSegmentR(lowerY, lowerH);        // c
  if (bits & 0b0001000) hSegment(DIGIT_H - DIGIT_THICK);  // d
  if (bits & 0b0000100) vSegmentL(lowerY, lowerH);        // e
  if (bits & 0b0000010) vSegmentL(DIGIT_THICK, upperH);   // f
  if (bits & 0b0000001) hSegment((DIGIT_H / 2) - (DIGIT_THICK / 2));  // g
}

void drawSevenTime(const char* text, bool force) {
  int16_t x = TIME_AREA_X;
  for (size_t i = 0; i < 8; ++i) {
    if (!force && previousTimeText[i] == text[i]) {
      if (text[i] == ':') {
        x += COLON_W + CHAR_SPACING;
      } else if (text[i] == '.') {
        x += DOT_W + CHAR_SPACING;
      } else {
        x += DIGIT_W + CHAR_SPACING;
      }
      continue;
    }

    if (text[i] == ':') {
      drawSegmentedDigit(x, TIME_AREA_Y, ':', fgColor(), COLOR_BLACK);
      x += COLON_W + CHAR_SPACING;
    } else if (text[i] == '.') {
      drawSegmentedDigit(x, TIME_AREA_Y, '.', fgColor(), COLOR_BLACK);
      x += DOT_W + CHAR_SPACING;
    } else {
      drawSegmentedDigit(x, TIME_AREA_Y, text[i], fgColor(), COLOR_BLACK);
      x += DIGIT_W + CHAR_SPACING;
    }
  }
  strncpy(previousTimeText, text, sizeof(previousTimeText));
}

void clearTimeMemory() { memset(previousTimeText, 0, sizeof(previousTimeText)); }

void formatTime(char out[9]) {
  const uint32_t tenths = elapsedTenths % 10;
  const uint32_t totalSeconds = elapsedTenths / 10;
  const uint32_t minutes = (totalSeconds / 60) % 100;
  const uint32_t seconds = totalSeconds % 60;

  snprintf(out, 9, "%02lu:%02lu.%lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds), static_cast<unsigned long>(tenths));
}

void drawSimpleLetter_T(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 18, 4, c);
  tft.fillRect(x + 7, y, 4, 24, c);
}

void drawSimpleLetter_E(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 4, 24, c);
  tft.fillRect(x, y, 16, 4, c);
  tft.fillRect(x, y + 10, 14, 4, c);
  tft.fillRect(x, y + 20, 16, 4, c);
}

void drawSimpleLetter_C(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 16, 4, c);
  tft.fillRect(x, y + 20, 16, 4, c);
  tft.fillRect(x, y + 4, 4, 16, c);
}

void drawSimpleLetter_K(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 4, 24, c);
  tft.drawFastVLine(x + 9, y + 6, 6, c);
  tft.drawFastVLine(x + 10, y + 12, 6, c);
  tft.drawFastVLine(x + 12, y + 3, 3, c);
  tft.drawFastVLine(x + 12, y + 18, 3, c);
  tft.drawFastVLine(x + 14, y, 3, c);
  tft.drawFastVLine(x + 14, y + 21, 3, c);
}

void drawSimpleLetter_R(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 4, 24, c);
  tft.fillRect(x, y, 14, 4, c);
  tft.fillRect(x, y + 10, 14, 4, c);
  tft.fillRect(x + 10, y + 4, 4, 6, c);
}

void drawSimpleLetter_A(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 4, 24, c);
  tft.fillRect(x + 12, y, 4, 24, c);
  tft.fillRect(x, y, 16, 4, c);
  tft.fillRect(x, y + 10, 16, 4, c);
}

void drawSimpleLetter_H(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x, y, 4, 24, c);
  tft.fillRect(x + 12, y, 4, 24, c);
  tft.fillRect(x, y + 10, 16, 4, c);
}

void drawSimpleLetter_Ee(int16_t x, int16_t y, uint16_t c) {
  tft.fillRect(x + 12, y + 4, 4, 16, c);
  tft.fillRect(x, y, 12, 4, c);
  tft.fillRect(x, y + 10, 12, 4, c);
  tft.fillRect(x, y + 20, 12, 4, c);
}

void drawRussianTitle(int16_t x, int16_t y, uint16_t c) {
  // "ТЕСТ ЭКРАНА"
  drawSimpleLetter_T(x, y, c);
  x += 20;
  drawSimpleLetter_E(x, y, c);
  x += 18;
  drawSimpleLetter_C(x, y, c);
  x += 18;
  drawSimpleLetter_T(x, y, c);
  x += 24;
  drawSimpleLetter_Ee(x, y, c);
  x += 18;
  drawSimpleLetter_K(x, y, c);
  x += 18;
  drawSimpleLetter_R(x, y, c);
  x += 18;
  drawSimpleLetter_A(x, y, c);
  x += 18;
  drawSimpleLetter_H(x, y, c);
  x += 18;
  drawSimpleLetter_A(x, y, c);
}

void drawSmallText(int16_t x, int16_t y, const char* text, uint16_t color) {
  const int16_t cw = 6;
  const int16_t ch = 8;
  // Minimal readable box text: each character rendered as rectangle frame for visibility.
  for (size_t i = 0; text[i] != '\0'; ++i) {
    const int16_t cx = x + i * cw;
    tft.drawFastHLine(cx, y, 5, color);
    tft.drawFastHLine(cx, y + ch - 1, 5, color);
    tft.drawFastVLine(cx, y, ch, color);
    tft.drawFastVLine(cx + 4, y, ch, color);
  }
}

void drawBootScreen() {
  tft.fillScreen(COLOR_BLACK);
  drawRussianTitle(5, 34, COLOR_CYAN);

  drawRectOutline(8, 78, TFT_WIDTH - 16, 44, COLOR_GRAY);
  drawSmallText(14, 88, "ST7789 170x320", COLOR_WHITE);

  char offsetLine[40];
  const auto& p = OFFSETS[offsetPresetIndex];
  snprintf(offsetLine, sizeof(offsetLine), "OFFSET: X=%u Y=%u (P%u)", p.x, p.y, offsetPresetIndex);
  drawSmallText(14, 102, offsetLine, COLOR_WHITE);
}

void drawMainFrame() {
  tft.fillScreen(COLOR_BLACK);
  drawRectOutline(6, 6, TFT_WIDTH - 12, TFT_HEIGHT - 12, fgColor());

  drawRectOutline(10, TIME_AREA_Y - 10, TFT_WIDTH - 20, TIME_AREA_H + 20, COLOR_GRAY);
  clearTimeMemory();
  char now[9];
  formatTime(now);
  drawSevenTime(now, true);

  const auto& p = OFFSETS[offsetPresetIndex];
  char statusLine[60];
  snprintf(statusLine, sizeof(statusLine), "OFFSET: X=%u Y=%u (P%u)", p.x, p.y, offsetPresetIndex);
  drawSmallText(14, TFT_HEIGHT - 48, statusLine, fgColor());
  drawSmallText(14, TFT_HEIGHT - 36, stopwatchRunning ? "RUN" : "STOP", fgColor());
}

void applyOffsetPreset() {
  const auto& preset = OFFSETS[offsetPresetIndex];
  tft.begin(preset.x, preset.y);
  Serial.printf("Display re-init with OFFSET preset P%u: X=%u Y=%u\n", offsetPresetIndex, preset.x,
                preset.y);
  if (showingBoot) {
    drawBootScreen();
  } else {
    drawMainFrame();
  }
}

void cycleOffsetPreset() {
  offsetPresetIndex = (offsetPresetIndex + 1) % (sizeof(OFFSETS) / sizeof(OFFSETS[0]));
  Serial.println("DOWN pressed -> cycle offset preset");
  applyOffsetPreset();
}

void toggleTheme() {
  currentTheme = currentTheme == Theme::White ? Theme::Red : Theme::White;
  drawMainFrame();
}

void handlePress(const char* name) {
  if (strcmp(name, "MID") == 0) {
    stopwatchRunning = !stopwatchRunning;
    Serial.println("MID pressed -> toggle start/stop");
    drawMainFrame();
    return;
  }

  if (strcmp(name, "UP") == 0) {
    elapsedTenths = 0;
    Serial.println("UP pressed -> reset stopwatch");
    char now[9];
    formatTime(now);
    clearTimeMemory();
    drawSevenTime(now, true);
    return;
  }

  if (strcmp(name, "LEFT") == 0) {
    Serial.println("LEFT pressed -> toggle theme");
    toggleTheme();
    return;
  }

  if (strcmp(name, "RIGHT") == 0) {
    Serial.println("RIGHT pressed -> toggle theme");
    toggleTheme();
    return;
  }

  if (strcmp(name, "DOWN") == 0) {
    cycleOffsetPreset();
    return;
  }
}

void pollButtons() {
  constexpr uint32_t debounceMs = 40;
  const uint32_t now = millis();

  for (auto& btn : buttons) {
    const bool raw = digitalRead(btn.pin);

    if (raw != btn.rawPrev) {
      btn.rawPrev = raw;
      btn.lastChangeMs = now;
    }

    if ((now - btn.lastChangeMs) >= debounceMs && btn.stable != raw) {
      btn.stable = raw;
      if (!btn.stable) {
        handlePress(btn.name);
      }
    }
  }
}

void updateStopwatch() {
  const uint32_t now = millis();
  if (stopwatchRunning && (now - lastTickMs >= 100)) {
    lastTickMs += 100;
    elapsedTenths++;

    char text[9];
    formatTime(text);
    drawSevenTime(text, false);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32-S3 ST7789 170x320 test firmware booting...");

  for (auto& btn : buttons) {
    pinMode(btn.pin, INPUT_PULLUP);
    btn.rawPrev = digitalRead(btn.pin);
    btn.stable = btn.rawPrev;
    btn.lastChangeMs = millis();
  }

  applyOffsetPreset();
  drawBootScreen();
  bootTimeMs = millis();
  lastTickMs = bootTimeMs;
}

void loop() {
  pollButtons();

  if (showingBoot && (millis() - bootTimeMs > 1500)) {
    showingBoot = false;
    drawMainFrame();
  }

  if (!showingBoot) {
    updateStopwatch();
  }

  delay(2);
}
