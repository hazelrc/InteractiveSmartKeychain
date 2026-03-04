#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FluxGarage_RoboEyes.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <time.h>

// ================== BUTTON ==================
#define BUTTON_PIN 4  // pressed = HIGH

// ================== OLED ==================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// RoboEyes instance
RoboEyes eyes(display);

// ================== IDLE TIMING ==================
unsigned long lastIdleChange = 0;
const unsigned long IDLE_INTERVAL = 5000;

// ================== TAP / HOLD SETTINGS ==================
const unsigned long DEBOUNCE_MS   = 30;
const unsigned long HOLD_MS       = 600;
const unsigned long TAP_TIMEOUT   = 300;

// ================== BUTTON STATE ==================
bool lastRawPressed      = false;
bool stablePressed       = false;
unsigned long lastBounce = 0;

unsigned long pressTime   = 0;
unsigned long lastTapTime = 0;
int tapCount = 0;
bool holdTriggered = false;

// ================== MODE ==================
enum Mode { MODE_IDLE, MODE_FEEDBACK, MODE_WIFI_PORTAL, MODE_CLOCK };
Mode mode = MODE_IDLE;
unsigned long feedbackUntil = 0;

// ================== EXPRESSIONS ==================
enum Expr {
  EXPR_IDLE_SLEEP,
  EXPR_IDLE_LOVE,
  EXPR_IDLE_SMILE,
  EXPR_IDLE_SWEAT,

  EXPR_HOLD_PET,
  EXPR_WINK,
  EXPR_SHY_LOVE,
  EXPR_CRY
};

Expr currentExpr = EXPR_IDLE_SMILE;

// ================== RENDER CONTROL ==================
bool useRoboEyes = true;

// ================== LOVE PULSE SETTINGS ==================
unsigned long lovePulseStart = 0;
const unsigned long LOVE_PULSE_PERIOD = 900;
const float LOVE_SCALE_MIN = 0.85f;
const float LOVE_SCALE_MAX = 1.10f;

// ================== CLOCK SETTINGS ==================
const unsigned long CLOCK_SHOW_MS = 10000;
unsigned long clockUntil = 0;
unsigned long lastClockDraw = 0;
const unsigned long CLOCK_DRAW_INTERVAL = 200;

bool timeSynced = false;

// Asia/Jakarta (WIB) UTC+7
const long GMT_OFFSET_SEC = 7L * 3600L;
const int  DST_OFFSET_SEC = 0;

// NTP servers
const char* NTP1 = "pool.ntp.org";
const char* NTP2 = "time.google.com";
const char* NTP3 = "time.nist.gov";

// ================== WIFI MANAGER (NON-BLOCKING) ==================
WiFiManager wm;
bool portalRunning = false;
unsigned long portalStartMs = 0;
const unsigned long PORTAL_MAX_MS = 180000; // 180s

// ================== TEXT HELPERS ==================
void drawCenterText(const String &line1, const String &line2 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  int cx1 = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(cx1, 22);
  display.print(line1);

  if (line2.length() > 0) {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int cx2 = (SCREEN_WIDTH - (int)w) / 2;
    display.setCursor(cx2, 36);
    display.print(line2);
  }

  display.display();
}

void drawStartupMessage() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setCursor(10, 26);
  display.print("hallo");
  display.display();
}

// ---- WiFi portal instruction screen ----
void drawWifiPortalScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("WiFi Setup (tap 4x)");

  display.setCursor(0, 14);
  display.print("AP : RoboEyes-Setup");

  display.setCursor(0, 26);
  display.print("URL: 192.168.4.1");

  display.setCursor(0, 44);
  display.print("1 tap = Exit/Cancel");

  display.display();
}

// ================== ROBOEYES ==================
void resetEyeBehaviors() {
  eyes.setAutoblinker(false);
  eyes.setIdleMode(false);
  eyes.setHFlicker(false);
  eyes.setVFlicker(false);
  eyes.setSweat(false);
  eyes.setCuriosity(false);
  eyes.setMood(DEFAULT);
  eyes.setPosition(DEFAULT);
  eyes.open();
}

void drawHeartScaled(int cx, int cy, float s) {
  int r = (int)(12 * s);
  int triW = (int)(40 * s);
  int triH = (int)(22 * s);
  int dx = (int)(16 * s);

  display.fillCircle(cx - dx/2, cy, r, WHITE);
  display.fillCircle(cx + dx/2, cy, r, WHITE);

  display.fillTriangle(
    cx - triW/2, cy + (int)(4*s),
    cx + triW/2, cy + (int)(4*s),
    cx,          cy + triH + (int)(4*s),
    WHITE
  );
}

float lovePulseScale(unsigned long now) {
  unsigned long t = (now - lovePulseStart) % LOVE_PULSE_PERIOD;
  float half = LOVE_PULSE_PERIOD / 2.0f;
  float x = (t <= half) ? (t / half) : (1.0f - ((t - half) / half));
  return LOVE_SCALE_MIN + (LOVE_SCALE_MAX - LOVE_SCALE_MIN) * x;
}

void drawLoveFull(unsigned long now) {
  display.clearDisplay();

  float s = lovePulseScale(now);
  const int GAP = 18;

  int leftCx  = (SCREEN_WIDTH / 2) - GAP - 18;
  int rightCx = (SCREEN_WIDTH / 2) + GAP + 18;

  int cy = 24;

  drawHeartScaled(leftCx,  cy, s);
  drawHeartScaled(rightCx, cy, s);

  display.fillCircle(leftCx - (int)(6*s),  cy - (int)(4*s), (int)(2*s), BLACK);
  display.fillCircle(rightCx - (int)(6*s), cy - (int)(4*s), (int)(2*s), BLACK);

  display.display();
}

void applyExpression(Expr e) {
  currentExpr = e;

  useRoboEyes = true;
  resetEyeBehaviors();

  switch (e) {
    case EXPR_IDLE_SLEEP:
      useRoboEyes = true;
      eyes.setMood(TIRED);
      eyes.close();
      eyes.setAutoblinker(false);
      eyes.setIdleMode(false);
      break;

    case EXPR_IDLE_LOVE:
      useRoboEyes = false;
      lovePulseStart = millis();
      break;

    case EXPR_IDLE_SMILE:
      useRoboEyes = true;
      eyes.setMood(HAPPY);
      eyes.open();
      eyes.setAutoblinker(true, 3, 2);
      eyes.setIdleMode(true, 2, 2);
      break;

    case EXPR_IDLE_SWEAT:
      useRoboEyes = true;
      eyes.setMood(DEFAULT);
      eyes.open();
      eyes.setSweat(true);
      eyes.setCuriosity(true);
      eyes.setHFlicker(true, 2);
      eyes.setIdleMode(true, 1, 1);
      eyes.setAutoblinker(true, 2, 1);
      break;

    case EXPR_HOLD_PET:
      useRoboEyes = true;
      eyes.setMood(HAPPY);
      eyes.setAutoblinker(true, 1, 0);
      eyes.setVFlicker(true, 1);
      eyes.setIdleMode(false);
      break;

    case EXPR_WINK:
      useRoboEyes = true;
      eyes.setMood(HAPPY);
      eyes.blink(0, 1);
      eyes.setAutoblinker(false);
      eyes.setIdleMode(false);
      break;

    case EXPR_SHY_LOVE:
      useRoboEyes = true;
      eyes.setMood(TIRED);
      eyes.open();
      eyes.setIdleMode(false);
      break;

    case EXPR_CRY:
      useRoboEyes = true;
      eyes.setMood(ANGRY);
      eyes.open();
      eyes.setIdleMode(false);
      break;
  }
}

void renderEyes(unsigned long now) {
  if (!useRoboEyes) {
    drawLoveFull(now);
    return;
  }
  eyes.update();
}

// ================== IDLE RANDOM PICK ==================
void pickNewIdle() {
  int r = random(0, 5); // 0..4

  if (r == 0) {
    applyExpression(EXPR_IDLE_SLEEP);
  } else if (r == 1) {
    applyExpression(EXPR_IDLE_LOVE);
  } else if (r == 2) {
    applyExpression(EXPR_IDLE_SMILE);
  } else if (r == 3) {
    applyExpression(EXPR_IDLE_SWEAT);
  } else {
    applyExpression(EXPR_IDLE_SMILE);
    eyes.setMood(DEFAULT);
    eyes.setAutoblinker(true, 4, 2);
    eyes.setIdleMode(false);
  }
}

// ================== NTP TIME ==================
bool ensureTimeSynced(unsigned long timeoutMs = 8000) {
  if (WiFi.status() != WL_CONNECTED) return false;

  if (!timeSynced) {
    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP1, NTP2, NTP3);
  }

  unsigned long start = millis();
  struct tm tminfo;
  while (millis() - start < timeoutMs) {
    if (getLocalTime(&tminfo, 500)) {
      timeSynced = true;
      return true;
    }
    delay(50);
  }
  return false;
}

void drawClockScreen() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 200)) {
    drawCenterText("Sync time...", "");
    return;
  }

  char hhmmss[16];
  char dateLine[24];
  strftime(hhmmss, sizeof(hhmmss), "%H:%M:%S", &timeinfo);
  strftime(dateLine, sizeof(dateLine), "%a, %d %b %Y", &timeinfo);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  display.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(String(hhmmss), 0, 0, &x1, &y1, &w, &h);
  int cx = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(cx, 16);
  display.print(hhmmss);

  display.setTextSize(1);
  display.getTextBounds(String(dateLine), 0, 0, &x1, &y1, &w, &h);
  int cx2 = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(cx2, 44);
  display.print(dateLine);

  display.display();
}

void startClockMode(unsigned long now) {
  if (WiFi.status() != WL_CONNECTED) {
    drawCenterText("No WiFi", "Tap 4x for setup");
    delay(900);
    mode = MODE_IDLE;
    pickNewIdle();
    lastIdleChange = millis();
    return;
  }

  if (!ensureTimeSynced(8000)) {
    drawCenterText("Time sync fail", "Check internet");
    delay(900);
    mode = MODE_IDLE;
    pickNewIdle();
    lastIdleChange = millis();
    return;
  }

  mode = MODE_CLOCK;
  clockUntil = now + CLOCK_SHOW_MS;
  lastClockDraw = 0;
}

// ================== WIFI PORTAL (NON-BLOCKING) ==================
void startWifiManagerPortal() {
  mode = MODE_WIFI_PORTAL;

  drawWifiPortalScreen();

  WiFi.mode(WIFI_STA);

  wm.setConfigPortalTimeout(180);
  wm.setConnectTimeout(20);
  wm.setCaptivePortalEnable(true);
  wm.setCleanConnect(true);

  // non-blocking biar tombol masih kebaca
  wm.setConfigPortalBlocking(false);

  portalRunning = true;
  portalStartMs = millis();

  wm.startConfigPortal("RoboEyes-Setup");
}

void exitWifiPortalToIdle() {
  wm.stopConfigPortal();

  portalRunning = false;
  mode = MODE_IDLE;

  drawCenterText("Exit WiFi Setup", "");
  delay(600);

  pickNewIdle();
  lastIdleChange = millis();
}

void handleWifiPortal(unsigned long now) {
  if (!portalRunning) return;

  wm.process();

  if (WiFi.status() == WL_CONNECTED) {
    portalRunning = false;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.setTextSize(1);

    display.setCursor(0, 10);
    display.print("Connected!");
    display.setCursor(0, 26);
    display.print("IP: ");
    display.print(WiFi.localIP());

    display.setCursor(0, 46);
    display.print("Restarting...");
    display.display();

    timeSynced = false;
    ensureTimeSynced(8000);

    delay(900);
    ESP.restart();
  }

  if (now - portalStartMs >= PORTAL_MAX_MS) {
    drawCenterText("WiFi Timeout", "");
    delay(600);
    exitWifiPortalToIdle();
  }
}

// ================== FEEDBACK START ==================
void startFeedbackTap(int taps, unsigned long now) {
  // kalau lagi di WiFi setup: 1 tap = keluar
  if (mode == MODE_WIFI_PORTAL) {
    if (taps == 1) exitWifiPortalToIdle();
    return;
  }

  if (taps == 4) { startWifiManagerPortal(); return; }
  if (taps == 5) { startClockMode(now); return; }

  mode = MODE_FEEDBACK;

  if (taps == 1) {
    applyExpression(EXPR_WINK);
    feedbackUntil = now + 900;
  } else if (taps == 2) {
    applyExpression(EXPR_SHY_LOVE);
    feedbackUntil = now + 2000;
  } else {
    applyExpression(EXPR_CRY);
    feedbackUntil = now + 2000;
  }
}

void startFeedbackHold(unsigned long now) {
  if (mode == MODE_WIFI_PORTAL) return;

  mode = MODE_FEEDBACK;
  applyExpression(EXPR_HOLD_PET);
  feedbackUntil = now + 300;
}

// ================== SETUP ==================
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLDOWN); // pressed = HIGH (ESP32 aman)

  randomSeed(micros());

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;) {}
  }

  drawStartupMessage();
  delay(2000);

  eyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 100);

  // auto connect kalau sudah pernah simpan WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 1200) {
    delay(50);
  }
  if (WiFi.status() == WL_CONNECTED) {
    timeSynced = false;
    ensureTimeSynced(6000);
  }

  pickNewIdle();
  lastIdleChange = millis();
}

// ================== LOOP ==================
void loop() {
  unsigned long now = millis();

  // ===== CLOCK MODE =====
  if (mode == MODE_CLOCK) {
    if (now - lastClockDraw >= CLOCK_DRAW_INTERVAL) {
      lastClockDraw = now;
      drawClockScreen();
    }
    if (now >= clockUntil) {
      mode = MODE_IDLE;
      pickNewIdle();
      lastIdleChange = now;
    }
    return;
  }

  // ===== WIFI PORTAL MODE =====
  if (mode == MODE_WIFI_PORTAL) {
    handleWifiPortal(now);
    // tetap lanjut baca tombol (biar 1 tap exit kebaca)
  }

  // ===== READ + DEBOUNCE =====
  bool rawPressed = (digitalRead(BUTTON_PIN) == HIGH);

  if (rawPressed != lastRawPressed) lastBounce = now;

  if (now - lastBounce > DEBOUNCE_MS) {
    if (rawPressed != stablePressed) {
      stablePressed = rawPressed;

      if (stablePressed) {
        pressTime = now;
        holdTriggered = false;
      } else {
        if (!holdTriggered) {
          tapCount++;
          lastTapTime = now;
        }
      }
    }
  }
  lastRawPressed = rawPressed;

  // ===== HOLD DETECT =====
  if (stablePressed && !holdTriggered && (now - pressTime >= HOLD_MS)) {
    holdTriggered = true;
    tapCount = 0;
    startFeedbackHold(now);
  }

  // ===== TAP FINALIZE =====
  if (!stablePressed && !holdTriggered && tapCount > 0 && (now - lastTapTime >= TAP_TIMEOUT)) {
    int taps = tapCount;
    tapCount = 0;
    startFeedbackTap(taps, now);
  }

  // ===== MODE HANDLER =====
  if (mode == MODE_FEEDBACK) {
    if (now >= feedbackUntil) {
      mode = MODE_IDLE;
      holdTriggered = false;
      pickNewIdle();
      lastIdleChange = now;
    } else {
      renderEyes(now);
      return;
    }
  }

  // ===== IDLE MODE =====
  if (mode == MODE_IDLE && (now - lastIdleChange >= IDLE_INTERVAL)) {
    lastIdleChange = now;
    pickNewIdle();
  }

  if (mode == MODE_IDLE) renderEyes(now);
}
