// 桃園 / 松山機場航班看板 - LilyGO T-Display S3 Touch
//
// 按鍵
//   BOOT (GPIO0)  短按：切換機場 桃園 ⇄ 松山      長按：調整亮度
//   KEY  (GPIO14) 短按：下一頁                    長按：切換 出發 ⇄ 抵達
// 觸控
//   上/下滑：翻頁    左/右滑：切換機場    點標題列：切換 出發 ⇄ 抵達
//   點航班：顯示詳細資訊    長按：立即更新資料
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "flight.h"
#include "ui.h"

static LGFX tft;
static Preferences prefs;

static const uint8_t BRIGHTNESS[] = {30, 80, 160, 255};

struct App {
  Airport ap = AP_TPE;
  Direction dir = DIR_DEP;
  uint8_t bright = 2;
  int page = 0;
  uint32_t animStart = 0;
  uint32_t lastPageFlip = 0;
  uint32_t manualUntil = 0;  // 手動操作後暫停自動翻頁

  std::vector<Flight> flights;
  uint32_t dataVersion = UINT32_MAX;
  time_t lastSnapshot = 0;
  time_t updated = 0;
  bool fetchOk = true;

  bool detail = false;
  Flight detailFlight;
  uint32_t detailSince = 0;

  char toast[48] = "";
  uint32_t toastUntil = 0;
} app;

// ---------------- 狀態操作 ----------------

static int pageCount() { return std::max(1, (int)(app.flights.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE); }

static void showToast(const char* msg) {
  strlcpy(app.toast, msg, sizeof(app.toast));
  app.toastUntil = millis() + 1500;
}

static void savePrefs() {
  prefs.putUChar("ap", app.ap);
  prefs.putUChar("dir", app.dir);
  prefs.putUChar("bright", app.bright);
}

static void flipTo(int page, bool manual) {
  int n = pageCount();
  page = ((page % n) + n) % n;
  app.page = page;
  app.animStart = millis();
  app.lastPageFlip = millis();
  if (manual) app.manualUntil = millis() + AUTO_PAGE_RESUME_MS;
}

static void refreshSnapshot(bool force) {
  time_t now = time(nullptr);
  if (!force && fetcherVersion() == app.dataVersion && now - app.lastSnapshot < 30) return;
  app.dataVersion = fetcherSnapshot(app.ap, app.dir, now, app.flights, app.updated, app.fetchOk);
  app.lastSnapshot = now;
  if (app.page >= pageCount()) app.page = 0;
}

static void switchBoard(Airport ap, Direction dir) {
  app.ap = ap;
  app.dir = dir;
  app.detail = false;
  refreshSnapshot(true);
  flipTo(0, true);
  savePrefs();
  char msg[48];
  snprintf(msg, sizeof(msg), "%s %s", ap == AP_TPE ? "桃園 TPE" : "松山 TSA",
           dir == DIR_DEP ? "出發 DEP" : "抵達 ARR");
  showToast(msg);
}

static void toggleAirport() { switchBoard(app.ap == AP_TPE ? AP_TSA : AP_TPE, app.dir); }
static void toggleDirection() { switchBoard(app.ap, app.dir == DIR_DEP ? DIR_ARR : DIR_DEP); }

static void cycleBrightness() {
  app.bright = (app.bright + 1) % sizeof(BRIGHTNESS);
  tft.setBrightness(BRIGHTNESS[app.bright]);
  savePrefs();
  char msg[32];
  snprintf(msg, sizeof(msg), "亮度 %d/%d", app.bright + 1, (int)sizeof(BRIGHTNESS));
  showToast(msg);
}

static void forceRefresh() {
  fetcherRequestRefresh();
  showToast("更新中 Refreshing");
}

static void openDetail(int row) {
  int idx = app.page * ROWS_PER_PAGE + row;
  if (row < 0 || row >= ROWS_PER_PAGE || idx >= (int)app.flights.size()) return;
  app.detailFlight = app.flights[idx];
  app.detail = true;
  app.detailSince = millis();
}

static void closeDetail() {
  app.detail = false;
  app.animStart = millis();
  app.manualUntil = millis() + AUTO_PAGE_RESUME_MS;
}

// ---------------- 按鍵 ----------------

struct Button {
  uint8_t pin;
  void (*onShort)();
  void (*onLong)();
  bool down = false;
  bool longFired = false;
  uint32_t t0 = 0;

  Button(uint8_t p, void (*s)(), void (*l)()) : pin(p), onShort(s), onLong(l) {}

  void poll() {
    bool pressed = digitalRead(pin) == LOW;
    uint32_t now = millis();
    if (pressed && !down) {
      down = true;
      longFired = false;
      t0 = now;
    } else if (pressed && down && !longFired && now - t0 > 600) {
      longFired = true;
      if (app.detail) closeDetail();
      else onLong();
    } else if (!pressed && down) {
      down = false;
      if (!longFired && now - t0 > 30) {
        if (app.detail) closeDetail();
        else onShort();
      }
    }
  }
};

static Button btnBoot{PIN_BTN_BOOT, toggleAirport, cycleBrightness};
static Button btnKey{PIN_BTN_KEY, [] { flipTo(app.page + 1, true); }, toggleDirection};

// ---------------- 觸控 ----------------

static struct {
  bool down = false;
  bool longFired = false;
  int sx, sy, lx, ly;
  uint32_t t0;
} touch;

static void onTap(int x, int y) {
  if (app.detail) {
    closeDetail();
    return;
  }
  if (y < ROW_Y0) {
    toggleDirection();
    return;
  }
  openDetail((y - ROW_Y0) / ROW_H);
}

static void pollTouch() {
  lgfx::touch_point_t tp;
  bool pressed = tft.getTouch(&tp, 1) > 0;
  uint32_t now = millis();
  if (pressed) {
    if (!touch.down) {
      touch.down = true;
      touch.longFired = false;
      touch.sx = touch.lx = tp.x;
      touch.sy = touch.ly = tp.y;
      touch.t0 = now;
    }
    touch.lx = tp.x;
    touch.ly = tp.y;
    int moved = abs(touch.lx - touch.sx) + abs(touch.ly - touch.sy);
    if (!touch.longFired && moved < 15 && now - touch.t0 > 900) {
      touch.longFired = true;
      if (!app.detail) forceRefresh();
    }
    return;
  }
  if (!touch.down) return;
  touch.down = false;
  if (touch.longFired) return;

  int dx = touch.lx - touch.sx, dy = touch.ly - touch.sy;
  if (abs(dx) > 50 && abs(dx) > abs(dy)) {
    if (app.detail) closeDetail();
    else toggleAirport();
  } else if (abs(dy) > 30 && abs(dy) > abs(dx)) {
    if (app.detail) closeDetail();
    else flipTo(app.page + (dy < 0 ? 1 : -1), true);
  } else if (now - touch.t0 < 600) {
    onTap(touch.sx, touch.sy);
  }
}

// ---------------- 網路 ----------------

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  if (strlen(WIFI_SSID)) {
    uiMessage("連線 Wi-Fi 中…", "Connecting to Wi-Fi", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    for (int i = 0; i < 30 && !WiFi.isConnected(); i++) delay(500);
    if (WiFi.isConnected()) return;
  }
  WiFiManager wm;
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(300);
  wm.setAPCallback([](WiFiManager*) {
    uiMessage("請用手機連線 Wi-Fi 設定", "Connect phone to Wi-Fi AP", WIFI_PORTAL_AP "  →  192.168.4.1");
  });
  uiMessage("連線 Wi-Fi 中…", "Connecting to Wi-Fi");
  if (!wm.autoConnect(WIFI_PORTAL_AP)) {
    uiMessage("Wi-Fi 設定逾時，重新啟動", "Wi-Fi setup timeout, restarting");
    delay(3000);
    ESP.restart();
  }
}

static void syncTime() {
  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  uiMessage("校正時間中…", "Syncing clock", WiFi.localIP().toString().c_str());
  for (int i = 0; i < 40 && time(nullptr) < 1700000000; i++) delay(250);
  Serial.printf("[main] WiFi %s IP %s, time %ld\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(),
                (long)time(nullptr));
}

// ---------------- 主程式 ----------------

void setup() {
  pinMode(PIN_POWER_ON, OUTPUT);  // 使用電池時需要打開螢幕電源
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_KEY, INPUT_PULLUP);
  Serial.begin(115200);

  prefs.begin("fids", false);
  app.ap = (Airport)std::min<uint8_t>(prefs.getUChar("ap", AP_TPE), AP_TSA);
  app.dir = (Direction)std::min<uint8_t>(prefs.getUChar("dir", DIR_DEP), DIR_ARR);
  app.bright = std::min<uint8_t>(prefs.getUChar("bright", 2), sizeof(BRIGHTNESS) - 1);

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setBrightness(BRIGHTNESS[app.bright]);
  uiInit(&tft);
  uiMessage("航班資訊看板", "Flight Information Display", "桃園 TPE · 松山 TSA");
  delay(800);

  connectWiFi();
  syncTime();
  fetcherStart();
  app.flights.reserve(600);
  refreshSnapshot(true);
  flipTo(0, false);
}

void loop() {
  static uint32_t lastFrame = 0;
  static bool wasBusy = false;

  btnBoot.poll();
  btnKey.poll();
  pollTouch();

  uint32_t now = millis();
  if (!WiFi.isConnected()) {
    static uint32_t lastTry = 0;
    if (now - lastTry > 10000) {
      lastTry = now;
      WiFi.reconnect();
    }
  }

  bool busy = fetcherBusy();
  if (wasBusy && !busy && !app.detail) app.animStart = now;  // 資料更新後播放翻牌動畫
  wasBusy = busy;
  refreshSnapshot(false);

  // 自動翻頁
  int autoPages = std::min(pageCount(), AUTO_PAGE_MAX);
  if (!app.detail && now > app.manualUntil && now - app.lastPageFlip > AUTO_PAGE_MS) {
    flipTo(app.page + 1 >= autoPages ? 0 : app.page + 1, false);
  }
  if (app.detail && now - app.detailSince > DETAIL_TIMEOUT_MS) closeDetail();

  View v;
  v.ap = app.ap;
  v.dir = app.dir;
  v.flights = &app.flights;
  v.page = app.page;
  v.pages = pageCount();
  v.animStart = app.animStart;
  v.detail = app.detail ? &app.detailFlight : nullptr;
  v.now = time(nullptr);
  v.updated = app.updated;
  v.fetchOk = app.fetchOk;
  v.loading = busy;
  v.toast = now < app.toastUntil ? app.toast : nullptr;

  uint32_t frameMs = uiAnimating(v) ? 16 : 100;
  if (now - lastFrame >= frameMs) {
    lastFrame = now;
    uiRender(v);
  }
  delay(5);
}
