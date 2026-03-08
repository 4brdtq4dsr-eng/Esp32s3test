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

constexpr uint32_t TFT_SPI_HZ = 40000000;

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
    sendData8(0x55);    // RGB565

    sendCommand(0x36);  // MADCTL
    sendData8(0x00);    // Portrait

    sendCommand(0x21);  // INVON
    sendCommand(0x13);  // NORON
    sendCommand(0x29);  // DISPON
    delay(20);

    fillScreen(COLOR_BLACK);
  }

  uint16_t width() const { return TFT_WIDTH; }
  uint16_t height() const { return TFT_HEIGHT; }

  void fillScreen(uint16_t color) { fillRect(0, 0, width(), height(), color); }

  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    if (y < 0 || y >= static_cast<int16_t>(height()) || w <= 0) return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (x + w > static_cast<int16_t>(width())) w = width() - x;
    if (w <= 0) return;
    setAddrWindow(x, y, w, 1);
    writeColor(color, w);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    if (x < 0 || x >= static_cast<int16_t>(width()) || h <= 0) return;
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (y + h > static_cast<int16_t>(height())) h = height() - y;
    if (h <= 0) return;
    setAddrWindow(x, y, 1, h);
    writeColor(color, h);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (x + w > static_cast<int16_t>(width())) w = width() - x;
    if (y + h > static_cast<int16_t>(height())) h = height() - y;
    if (w <= 0 || h <= 0) return;

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
    tftSpi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, LOW);
    tftSpi.transfer(cmd);
    digitalWrite(PIN_TFT_CS, HIGH);
    tftSpi.endTransaction();
  }

  void sendData8(uint8_t data) {
    tftSpi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    tftSpi.transfer(data);
    digitalWrite(PIN_TFT_CS, HIGH);
    tftSpi.endTransaction();
  }

  static void tx16be(uint16_t v) {
    tftSpi.transfer(static_cast<uint8_t>(v >> 8));
    tftSpi.transfer(static_cast<uint8_t>(v & 0xFF));
  }

  void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    const uint16_t xs = x + xOffset_;
    const uint16_t xe = xs + w - 1;
    const uint16_t ys = y + yOffset_;
    const uint16_t ye = ys + h - 1;

    tftSpi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_TFT_CS, LOW);

    digitalWrite(PIN_TFT_DC, LOW);
    tftSpi.transfer(0x2A);
    digitalWrite(PIN_TFT_DC, HIGH);
    tx16be(xs);
    tx16be(xe);

    digitalWrite(PIN_TFT_DC, LOW);
    tftSpi.transfer(0x2B);
    digitalWrite(PIN_TFT_DC, HIGH);
    tx16be(ys);
    tx16be(ye);

    digitalWrite(PIN_TFT_DC, LOW);
    tftSpi.transfer(0x2C);
    digitalWrite(PIN_TFT_DC, HIGH);

    digitalWrite(PIN_TFT_CS, HIGH);
    tftSpi.endTransaction();
  }

  void writeColor(uint16_t color, uint32_t count) {
    const uint8_t hi = color >> 8;
    const uint8_t lo = color & 0xFF;

    tftSpi.beginTransaction(SPISettings(TFT_SPI_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(PIN_TFT_CS, LOW);
    digitalWrite(PIN_TFT_DC, HIGH);
    while (count--) {
      tftSpi.transfer(hi);
      tftSpi.transfer(lo);
    }
    digitalWrite(PIN_TFT_CS, HIGH);
    tftSpi.endTransaction();
  }
};

ST7789 tft;

enum class Theme : uint8_t { White = 0, Red = 1 };
Theme currentTheme = Theme::White;

bool stopwatchRunning = true;
bool showingBoot = true;
uint8_t offsetPresetIndex = 1;
uint32_t elapsedTenths = 0;
uint32_t lastTickMs = 0;
uint32_t bootTimeMs = 0;

struct Button {
  const char* name;
  uint8_t pin;
  bool stable;
  bool rawPrev;
  uint32_t lastChangeMs;
};

Button buttons[] = {
    {"UP", PIN_BTN_UP, true, true, 0},
    {"DOWN", PIN_BTN_DOWN, true, true, 0},
    {"LEFT", PIN_BTN_LEFT, true, true, 0},
    {"RIGHT", PIN_BTN_RIGHT, true, true, 0},
    {"MID", PIN_BTN_MID, true, true, 0},
};

constexpr uint16_t DIGIT_W = 24;
constexpr uint16_t DIGIT_H = 46;
constexpr uint16_t DIGIT_THICK = 5;
constexpr uint16_t COLON_W = 10;
constexpr uint16_t DOT_W = 8;
constexpr uint16_t CHAR_SPACING = 4;
constexpr uint16_t TIME_AREA_W = DIGIT_W * 6 + COLON_W + DOT_W + CHAR_SPACING * 7;
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

const uint8_t* glyph5x7(char c) {
  // columns, LSB at top
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
  static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t dash[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t lp[5] = {0x00, 0x1C, 0x22, 0x41, 0x00};
  static const uint8_t rp[5] = {0x00, 0x41, 0x22, 0x1C, 0x00};

  static const uint8_t n0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
  static const uint8_t n1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
  static const uint8_t n2[5] = {0x62, 0x51, 0x49, 0x49, 0x46};
  static const uint8_t n3[5] = {0x22, 0x41, 0x49, 0x49, 0x36};
  static const uint8_t n4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
  static const uint8_t n5[5] = {0x2F, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t n6[5] = {0x3E, 0x49, 0x49, 0x49, 0x32};
  static const uint8_t n7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
  static const uint8_t n8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
  static const uint8_t n9[5] = {0x26, 0x49, 0x49, 0x49, 0x3E};

  static const uint8_t A[5] = {0x7E, 0x09, 0x09, 0x09, 0x7E};
  static const uint8_t E[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
  static const uint8_t F[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
  static const uint8_t N[5] = {0x7F, 0x10, 0x08, 0x04, 0x7F};
  static const uint8_t O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
  static const uint8_t P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
  static const uint8_t R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
  static const uint8_t S[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
  static const uint8_t T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
  static const uint8_t U[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
  static const uint8_t X[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
  static const uint8_t Y[5] = {0x03, 0x04, 0x78, 0x04, 0x03};

  if (c >= '0' && c <= '9') {
    static const uint8_t* digits[] = {n0, n1, n2, n3, n4, n5, n6, n7, n8, n9};
    return digits[c - '0'];
  }

  switch (c) {
    case ' ': return space;
    case ':': return colon;
    case '.': return dot;
    case '-': return dash;
    case '(': return lp;
    case ')': return rp;
    case 'A': return A;
    case 'E': return E;
    case 'F': return F;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'U': return U;
    case 'X': return X;
    case 'Y': return Y;
    default: return space;
  }
}

void drawText5x7(int16_t x, int16_t y, const char* text, uint16_t fg, uint16_t bg, uint8_t scale = 1) {
  const int16_t cw = 6 * scale;
  const int16_t ch = 8 * scale;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    const uint8_t* g = glyph5x7(text[i]);
    const int16_t cx = x + static_cast<int16_t>(i) * cw;
    tft.fillRect(cx, y, cw, ch, bg);
    for (int col = 0; col < 5; ++col) {
      uint8_t bits = g[col];
      for (int row = 0; row < 7; ++row) {
        if (bits & (1 << row)) {
          tft.fillRect(cx + col * scale, y + row * scale, scale, scale, fg);
        }
      }
    }
  }
}

void drawSegmentedDigit(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg) {
  tft.fillRect(x, y, DIGIT_W, DIGIT_H, bg);
  if (c == ':') {
    tft.fillRect(x + 2, y + 12, 6, 6, color);
    tft.fillRect(x + 2, y + 28, 6, 6, color);
    return;
  }
  if (c == '.') {
    tft.fillRect(x + 1, y + DIGIT_H - 8, 6, 6, color);
    return;
  }
  if (c < '0' || c > '9') return;

  static constexpr uint8_t SEGMENT_MAP[10] = {
      0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011,
      0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011};

  const uint8_t bits = SEGMENT_MAP[c - '0'];
  auto hSeg = [&](int16_t sy) {
    tft.fillRect(x + DIGIT_THICK, y + sy, DIGIT_W - 2 * DIGIT_THICK, DIGIT_THICK, color);
  };
  auto vSegL = [&](int16_t sy, int16_t sh) { tft.fillRect(x, y + sy, DIGIT_THICK, sh, color); };
  auto vSegR = [&](int16_t sy, int16_t sh) {
    tft.fillRect(x + DIGIT_W - DIGIT_THICK, y + sy, DIGIT_THICK, sh, color);
  };

  const int16_t upperH = (DIGIT_H / 2) - DIGIT_THICK;
  const int16_t lowerY = DIGIT_H / 2;
  const int16_t lowerH = DIGIT_H / 2 - DIGIT_THICK;

  if (bits & 0b1000000) hSeg(0);
  if (bits & 0b0100000) vSegR(DIGIT_THICK, upperH);
  if (bits & 0b0010000) vSegR(lowerY, lowerH);
  if (bits & 0b0001000) hSeg(DIGIT_H - DIGIT_THICK);
  if (bits & 0b0000100) vSegL(lowerY, lowerH);
  if (bits & 0b0000010) vSegL(DIGIT_THICK, upperH);
  if (bits & 0b0000001) hSeg((DIGIT_H / 2) - (DIGIT_THICK / 2));
}

void clearTimeMemory() { memset(previousTimeText, 0, sizeof(previousTimeText)); }

void drawSevenTime(const char* text, bool force) {
  int16_t x = TIME_AREA_X;
  for (size_t i = 0; i < 8; ++i) {
    int16_t glyphW = DIGIT_W;
    if (text[i] == ':') glyphW = COLON_W;
    if (text[i] == '.') glyphW = DOT_W;

    if (!force && previousTimeText[i] == text[i]) {
      x += glyphW + CHAR_SPACING;
      continue;
    }

    drawSegmentedDigit(x, TIME_AREA_Y, text[i], fgColor(), COLOR_BLACK);
    x += glyphW + CHAR_SPACING;
  }
  strncpy(previousTimeText, text, sizeof(previousTimeText));
}

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

void drawStatusText() {
  const auto& p = OFFSETS[offsetPresetIndex];
  char offsetLine[32];
  snprintf(offsetLine, sizeof(offsetLine), "OFFSET: X=%u Y=%u (P%u)", p.x, p.y, offsetPresetIndex);
  tft.fillRect(12, TFT_HEIGHT - 56, TFT_WIDTH - 24, 28, COLOR_BLACK);
  drawText5x7(14, TFT_HEIGHT - 52, offsetLine, fgColor(), COLOR_BLACK, 1);
  drawText5x7(14, TFT_HEIGHT - 40, stopwatchRunning ? "RUN" : "STOP", fgColor(), COLOR_BLACK, 1);
}

void drawBootScreen() {
  tft.fillScreen(COLOR_BLACK);
  drawRussianTitle(5, 34, COLOR_CYAN);
  drawRectOutline(8, 78, TFT_WIDTH - 16, 44, COLOR_GRAY);
  drawText5x7(14, 88, "ST7789 170x320", COLOR_WHITE, COLOR_BLACK, 1);

  const auto& p = OFFSETS[offsetPresetIndex];
  char offsetLine[32];
  snprintf(offsetLine, sizeof(offsetLine), "OFFSET: X=%u Y=%u (P%u)", p.x, p.y, offsetPresetIndex);
  drawText5x7(14, 102, offsetLine, COLOR_WHITE, COLOR_BLACK, 1);
}

void drawMainFrame() {
  tft.fillScreen(COLOR_BLACK);
  drawRectOutline(6, 6, TFT_WIDTH - 12, TFT_HEIGHT - 12, fgColor());
  drawRectOutline(10, TIME_AREA_Y - 10, TFT_WIDTH - 20, DIGIT_H + 20, COLOR_GRAY);
  clearTimeMemory();
  char now[9];
  formatTime(now);
  drawSevenTime(now, true);
  drawStatusText();
}

void applyOffsetPreset() {
  const auto& p = OFFSETS[offsetPresetIndex];
  tft.begin(p.x, p.y);
  Serial.printf("Display re-init with OFFSET preset P%u: X=%u Y=%u\n", offsetPresetIndex, p.x, p.y);
  if (showingBoot) {
    drawBootScreen();
  } else {
    drawMainFrame();
  }
}

void toggleTheme() {
  currentTheme = (currentTheme == Theme::White) ? Theme::Red : Theme::White;
  drawMainFrame();
}

void handlePress(const char* name) {
  if (strcmp(name, "MID") == 0) {
    stopwatchRunning = !stopwatchRunning;
    Serial.println("MID pressed -> toggle");
    drawStatusText();
    return;
  }
  if (strcmp(name, "UP") == 0) {
    elapsedTenths = 0;
    Serial.println("UP pressed -> reset");
    char now[9];
    formatTime(now);
    clearTimeMemory();
    drawSevenTime(now, true);
    return;
  }
  if (strcmp(name, "LEFT") == 0) {
    Serial.println("LEFT pressed -> theme");
    toggleTheme();
    return;
  }
  if (strcmp(name, "RIGHT") == 0) {
    Serial.println("RIGHT pressed -> theme");
    toggleTheme();
    return;
  }
  if (strcmp(name, "DOWN") == 0) {
    offsetPresetIndex = (offsetPresetIndex + 1) % (sizeof(OFFSETS) / sizeof(OFFSETS[0]));
    Serial.println("DOWN pressed -> cycle offset preset");
    applyOffsetPreset();
    return;
  }
}

void pollButtons() {
  constexpr uint32_t debounceMs = 40;
  const uint32_t now = millis();
  for (auto& b : buttons) {
    const bool raw = digitalRead(b.pin);
    if (raw != b.rawPrev) {
      b.rawPrev = raw;
      b.lastChangeMs = now;
    }
    if ((now - b.lastChangeMs) >= debounceMs && b.stable != raw) {
      b.stable = raw;
      if (!b.stable) {
        handlePress(b.name);
      }
    }
  }
}

void updateStopwatch() {
  const uint32_t now = millis();
  if (stopwatchRunning && now - lastTickMs >= 100) {
    lastTickMs += 100;
    elapsedTenths++;
    char out[9];
    formatTime(out);
    drawSevenTime(out, false);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("ESP32-S3 ST7789 170x320 test firmware booting...");

  for (auto& b : buttons) {
    pinMode(b.pin, INPUT_PULLUP);
    b.rawPrev = digitalRead(b.pin);
    b.stable = b.rawPrev;
    b.lastChangeMs = millis();
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
