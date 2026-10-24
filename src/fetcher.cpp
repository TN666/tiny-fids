// 航班資料抓取：在 core 0 背景工作執行，避免卡住畫面
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "config.h"
#include "flight.h"
#include "parser.h"

namespace {

struct BoardData {
  std::vector<Flight> flights;
  time_t updated = 0;
  bool ok = false;
};

BoardData g_boards[AP_COUNT][DIR_COUNT];
SemaphoreHandle_t g_mtx;
TaskHandle_t g_task;
volatile uint32_t g_version = 0;
volatile bool g_busy = false;

// ok=false 時保留舊資料，只標記失敗
void publish(Airport ap, Direction dir, std::vector<Flight>& v, bool ok) {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  BoardData& b = g_boards[ap][dir];
  if (ok) {
    b.flights.swap(v);
    b.updated = time(nullptr);
  }
  b.ok = ok;
  g_version++;
  xSemaphoreGive(g_mtx);
}

bool httpGet(HTTPClient& http, WiFiClientSecure& client, const String& url) {
  client.setInsecure();  // 公開唯讀資料，不驗證憑證
  http.useHTTP10(true);
  http.setConnectTimeout(10000);
  http.setTimeout(15000);
  http.setUserAgent("Mozilla/5.0 (FIDS T-Display-S3)");
  http.begin(client, url);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[fetch] HTTP %d: %s\n", code, url.c_str());
    http.end();
    return false;
  }
  return true;
}

// HTTPClient::writeToStream() 需要 Stream，把收到的資料轉給解析器
class ParserStream : public Stream {
 public:
  explicit ParserStream(TpeParser& p) : p_(p) {}
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t c) override {
    p_.feed(&c, 1);
    return 1;
  }
  size_t write(const uint8_t* buf, size_t n) override {
    p_.feed(buf, n);
    return n;
  }

 private:
  TpeParser& p_;
};

bool fetchTPE() {
  WiFiClientSecure client;
  HTTPClient http;
  std::vector<Flight> none;
  if (!httpGet(http, client, TPE_URL)) {
    publish(AP_TPE, DIR_DEP, none, false);
    publish(AP_TPE, DIR_ARR, none, false);
    return false;
  }
  time_t now = time(nullptr);
  TpeParser parser(now - 12 * 3600, now + 36 * 3600);
  parser.dep.reserve(800);
  parser.arr.reserve(800);
  ParserStream sink(parser);
  int n = http.writeToStream(&sink);
  http.end();
  parser.finish();
  Serial.printf("[fetch] TPE: %d bytes, %d lines, dep=%u arr=%u\n", n, parser.lines, parser.dep.size(), parser.arr.size());
  bool ok = n > 0 && parser.lines > 0;
  publish(AP_TPE, DIR_DEP, parser.dep, ok);
  publish(AP_TPE, DIR_ARR, parser.arr, ok);
  return ok;
}

bool fetchTsaOne(const char* id, Direction dir, bool domestic, std::vector<Flight>& out) {
  WiFiClientSecure client;
  HTTPClient http;
  if (!httpGet(http, client, String(TSA_URL_BASE) + id)) return false;
  String body = http.getString();
  http.end();
  return parseTsaJson(body.c_str(), body.length(), dir, domestic, time(nullptr), out);
}

bool fetchTSA() {
  bool okAll = true;
  for (Direction dir : {DIR_DEP, DIR_ARR}) {
    std::vector<Flight> v;
    v.reserve(200);
    bool ok = fetchTsaOne(dir == DIR_DEP ? TSA_ID_INTL_DEP : TSA_ID_INTL_ARR, dir, false, v);
    ok &= fetchTsaOne(dir == DIR_DEP ? TSA_ID_DOM_DEP : TSA_ID_DOM_ARR, dir, true, v);
    sortFlights(v);
    Serial.printf("[fetch] TSA %s: %u flights ok=%d\n", dir == DIR_DEP ? "dep" : "arr", v.size(), ok);
    publish(AP_TSA, dir, v, ok);
    okAll &= ok;
  }
  return okAll;
}

void fetchTask(void*) {
  for (;;) {
    if (WiFi.isConnected() && time(nullptr) > 1700000000) {
      g_busy = true;
      g_version++;
      fetchTSA();  // 小檔先抓，畫面較快有內容
      fetchTPE();
      g_busy = false;
      g_version++;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
  }
}

}  // namespace

void fetcherStart() {
  g_mtx = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(fetchTask, "fetch", 16384, nullptr, 1, &g_task, 0);
}

void fetcherRequestRefresh() {
  if (g_task) xTaskNotifyGive(g_task);
}

uint32_t fetcherVersion() { return g_version; }
bool fetcherBusy() { return g_busy; }

uint32_t fetcherSnapshot(Airport ap, Direction dir, time_t now, std::vector<Flight>& out, time_t& updated,
                         bool& ok) {
  xSemaphoreTake(g_mtx, portMAX_DELAY);
  const BoardData& b = g_boards[ap][dir];
  out.clear();
  for (const auto& f : b.flights)
    if (isFlightVisible(f, now)) out.push_back(f);
  updated = b.updated;
  ok = b.ok;
  uint32_t v = g_version;
  xSemaphoreGive(g_mtx);
  return v;
}
