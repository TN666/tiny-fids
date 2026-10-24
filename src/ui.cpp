// 機場航班資訊看板 (FIDS) 風格畫面
#include "ui.h"

#include "config.h"

namespace {

LGFX* g_tft;
LGFX_Sprite g_canvas;

// ---- 配色：深藍底、琥珀色標題列，與機場大螢幕相同 ----
constexpr uint32_t C_BG = 0x05122B;
constexpr uint32_t C_ROW_ALT = 0x0B1F45;
constexpr uint32_t C_HEADER = 0xFFC20E;
constexpr uint32_t C_HEADER_TXT = 0x111111;
constexpr uint32_t C_COLHDR = 0x1C3A6E;
constexpr uint32_t C_COLHDR_TXT = 0xFFE08A;
constexpr uint32_t C_TEXT = 0xFFFFFF;
constexpr uint32_t C_TIME = 0xFFD34D;
constexpr uint32_t C_CODESHARE = 0x9CC9FF;
constexpr uint32_t C_DIM = 0x8C97AD;
constexpr uint32_t C_GREEN = 0x3DDC84;
constexpr uint32_t C_AMBER = 0xFFAA00;
constexpr uint32_t C_RED = 0xFF4D4D;
constexpr uint32_t C_CYAN = 0x4FC3F7;
constexpr uint32_t C_SEAM = 0x020812;

// 欄位位置
constexpr int X_TIME = 3, W_TIME = 41;
constexpr int X_FLT = 56, W_FLT = 58;
constexpr int X_CITY = 118, W_CITY = 88;
constexpr int X_GATE = 210, W_GATE = 38;
constexpr int X_RMK = 250, W_RMK = 68;

constexpr uint32_t ROW_ANIM_MS = 45;

const lgfx::IFont* F12 = &fonts::efontTW_12;
const lgfx::IFont* F16 = &fonts::efontTW_16;
const lgfx::IFont* F16B = &fonts::efontTW_16_b;
const lgfx::IFont* F24B = &fonts::efontTW_24_b;

// 0 = 中文, 1 = 英文, 2 = 額外資訊（預計時間）
int langPhase() { return (millis() / LANG_PHASE_MS) % 3; }
bool zhPhase() { return langPhase() == 0; }

uint32_t statusColor(StatusKind k) {
  switch (k) {
    case ST_ONTIME: return C_GREEN;
    case ST_DELAY:
    case ST_CHANGE:
    case ST_CLOSED: return C_AMBER;
    case ST_CANCEL: return C_RED;
    case ST_BOARDING: return C_CYAN;
    case ST_DONE: return C_DIM;
    default: return C_TEXT;
  }
}

const char* statusEnShort(const Flight& f) {
  if (!strcmp(f.statusEn, "SCHEDULE CHANGE")) return "NEW TIME";
  return f.statusEn;
}

void fmtTime(time_t t, char* buf, size_t n) {
  struct tm lt;
  localtime_r(&t, &lt);
  snprintf(buf, n, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

int dayDiff(time_t t, time_t now) {
  struct tm a, b;
  localtime_r(&t, &a);
  localtime_r(&now, &b);
  return (a.tm_year - b.tm_year) * 366 + (a.tm_yday - b.tm_yday);
}

// 在欄位內繪製文字；太寬時自動換小字，再不行就裁切
void drawFit(const char* txt, int x, int y, int w, int h, uint32_t color, const lgfx::IFont* font = F16,
             textdatum_t datum = textdatum_t::middle_left) {
  auto& s = g_canvas;
  s.setFont(font);
  if (s.textWidth(txt) > w && font != F12) s.setFont(F12);
  s.setClipRect(x, y, w, h);
  s.setTextColor(color);
  s.setTextDatum(datum);
  int tx = x;
  if (datum == textdatum_t::middle_center) tx = x + w / 2;
  else if (datum == textdatum_t::middle_right) tx = x + w;
  s.drawString(txt, tx, y + h / 2 + 1);
  s.clearClipRect();
}

// 小飛機圖示（朝右，arrival 時往下降、departure 時往上爬）
void drawPlane(int cx, int cy, float scale, bool arrival, uint32_t color) {
  static const float pts[][2] = {
      // 機身
      {-8, -1.3f}, {8, -1.3f}, {8, 1.3f}, {-8, 1.3f},
      // 上翼
      {3, -1}, {-2, -8.5f}, {-4.2f, -8.5f}, {-2, -1},
      // 下翼
      {3, 1}, {-2, 8.5f}, {-4.2f, 8.5f}, {-2, 1},
      // 尾翼
      {-5.5f, -1}, {-8, -4.5f}, {-9.5f, -4.5f}, {-8.5f, -1},
      {-5.5f, 1}, {-8, 4.5f}, {-9.5f, 4.5f}, {-8.5f, 1},
  };
  float a = arrival ? 0.5f : -0.5f;
  float ca = cosf(a) * scale, sa = sinf(a) * scale;
  auto X = [&](int i) { return (int)roundf(cx + pts[i][0] * ca - pts[i][1] * sa); };
  auto Y = [&](int i) { return (int)roundf(cy + pts[i][0] * sa + pts[i][1] * ca); };
  for (int q = 0; q < 5; q++) {
    int i = q * 4;
    g_canvas.fillTriangle(X(i), Y(i), X(i + 1), Y(i + 1), X(i + 2), Y(i + 2), color);
    g_canvas.fillTriangle(X(i), Y(i), X(i + 2), Y(i + 2), X(i + 3), Y(i + 3), color);
  }
  // 機鼻
  g_canvas.fillCircle((int)roundf(cx + 8 * ca), (int)roundf(cy + 8 * sa), (int)ceilf(1.3f * scale), color);
}

const char* airportZh(Airport ap) { return ap == AP_TPE ? "桃園國際機場" : "臺北松山機場"; }
const char* airportEn(Airport ap) { return ap == AP_TPE ? "TAOYUAN TPE" : "SONGSHAN TSA"; }

void drawHeader(const View& v) {
  auto& s = g_canvas;
  bool zh = zhPhase();
  s.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
  drawPlane(13, HEADER_H / 2, 0.9f, v.dir == DIR_ARR, C_HEADER_TXT);

  const char* title = v.dir == DIR_DEP ? (zh ? "出發" : "DEPARTURES") : (zh ? "抵達" : "ARRIVALS");
  drawFit(title, 27, 0, 100, HEADER_H, C_HEADER_TXT, F16B);
  drawFit(zh ? airportZh(v.ap) : airportEn(v.ap), 120, 0, 140, HEADER_H, C_HEADER_TXT, F16,
          textdatum_t::middle_right);

  char clk[8];
  fmtTime(v.now, clk, sizeof(clk));
  bool colon = (millis() / 1000) % 2;
  if (!colon) clk[2] = ' ';
  drawFit(clk, 270, 0, 46, HEADER_H, C_HEADER_TXT, F16B, textdatum_t::middle_right);

  // 資料狀態燈：更新中=藍、正常=綠、失敗或過舊=紅
  uint32_t dot = C_GREEN;
  if (v.loading) dot = (millis() / 300) % 2 ? 0x2060FF : C_HEADER;
  else if (!v.fetchOk || v.updated == 0 || v.now - v.updated > 15 * 60) dot = C_RED;
  s.fillCircle(266, HEADER_H / 2, 3, dot);
}

void drawColumnHeader(const View& v) {
  bool zh = zhPhase();
  g_canvas.fillRect(0, HEADER_H, SCREEN_W, COLHDR_H, C_COLHDR);
  int y = HEADER_H;
  drawFit(zh ? "時間" : "TIME", X_TIME, y, W_TIME, COLHDR_H, C_COLHDR_TXT, F12);
  drawFit(zh ? "航班" : "FLIGHT", X_FLT, y, W_FLT, COLHDR_H, C_COLHDR_TXT, F12);
  const char* city = v.dir == DIR_DEP ? (zh ? "目的地" : "TO") : (zh ? "出發地" : "FROM");
  drawFit(city, X_CITY, y, W_CITY, COLHDR_H, C_COLHDR_TXT, F12);
  const char* gate;
  if (v.dir == DIR_DEP) gate = zh ? "登機門" : "GATE";
  else if (v.ap == AP_TPE) gate = zh ? "轉盤" : "BELT";
  else gate = zh ? "航廈" : "TERM";
  drawFit(gate, X_GATE, y, W_GATE, COLHDR_H, C_COLHDR_TXT, F12);
  drawFit(zh ? "備註" : "REMARK", X_RMK, y, W_RMK, COLHDR_H, C_COLHDR_TXT, F12);
}

void drawRow(const View& v, const Flight& f, int y, bool alt) {
  auto& s = g_canvas;
  int phase = langPhase();
  s.fillRect(0, y, SCREEN_W, ROW_H, alt ? C_ROW_ALT : C_BG);

  char buf[24];
  fmtTime(f.sched, buf, sizeof(buf));
  bool cancelled = f.kind == ST_CANCEL;
  drawFit(buf, X_TIME, y, W_TIME, ROW_H, cancelled ? C_DIM : C_TIME);
  int dd = dayDiff(f.sched, v.now);
  if (dd != 0) {
    s.setFont(&fonts::Font0);
    s.setTextDatum(textdatum_t::top_left);
    s.setTextColor(C_AMBER);
    s.drawString(dd > 0 ? "+1" : "-1", X_TIME + W_TIME, y + 2);
  }

  // 航班號：輪播共用班號
  int fi = f.nFlight > 1 ? (millis() / LANG_PHASE_MS) % f.nFlight : 0;
  drawFit(f.flight[fi], X_FLT, y, W_FLT, ROW_H, fi == 0 ? C_TEXT : C_CODESHARE, F16B);

  const char* city = (phase == 0 && *f.cityZh) ? f.cityZh : (*f.cityEn ? f.cityEn : f.cityCode);
  drawFit(city, X_CITY, y, W_CITY, ROW_H, C_TEXT);

  const char* gate = f.gate;
  if (v.dir == DIR_ARR) {
    if (v.ap == AP_TPE) gate = *f.belt ? f.belt : "";
    else gate = f.domestic ? "T2" : "T1";
  }
  drawFit(*gate ? gate : "-", X_GATE, y, W_GATE, ROW_H, C_TEXT, F16, textdatum_t::middle_left);

  // 備註：中文 / 英文 / 預計或實際時間
  uint32_t col = statusColor(f.kind);
  const char* rmk = phase == 0 ? f.statusZh : statusEnShort(f);
  if (!*rmk) rmk = *f.statusZh ? f.statusZh : statusEnShort(f);
  if (phase == 2 && f.est && f.kind != ST_CANCEL) {
    bool changed = llabs((long long)(f.est - f.sched)) >= 5 * 60;
    if (f.kind == ST_DONE || changed) {
      char t[8];
      fmtTime(f.est, t, sizeof(t));
      const char* pre = f.kind == ST_DONE ? (v.dir == DIR_DEP ? "ATD " : "ATA ") : "EST ";
      snprintf(buf, sizeof(buf), "%s%s", pre, t);
      rmk = buf;
      if (f.kind != ST_DONE) col = C_AMBER;
    }
  }
  if (f.kind == ST_BOARDING && (millis() % 1000) > 700) rmk = "";  // 登機中閃爍
  drawFit(rmk, X_RMK, y, W_RMK, ROW_H, col);
}

void drawFlapRow(int y, bool alt) {
  g_canvas.fillRect(0, y, SCREEN_W, ROW_H, alt ? C_ROW_ALT : C_BG);
  g_canvas.drawFastHLine(0, y + ROW_H / 2, SCREEN_W, C_SEAM);
}

void drawFooter(const View& v) {
  int y = ROW_Y0 + ROWS_PER_PAGE * ROW_H;
  g_canvas.fillRect(0, y, SCREEN_W, SCREEN_H - y, C_SEAM);
  if (v.pages > 1) {
    int w = SCREEN_W / v.pages;
    g_canvas.fillRect(v.page * w, y + 1, std::max(w, 6), SCREEN_H - y - 1, C_HEADER);
  }
}

void drawBoard(const View& v) {
  auto& s = g_canvas;
  s.fillScreen(C_BG);
  drawHeader(v);
  drawColumnHeader(v);

  const auto& fl = *v.flights;
  if (fl.empty()) {
    bool zh = zhPhase();
    const char* msg = (v.loading || v.updated == 0) ? (zh ? "資料載入中…" : "LOADING...")
                                                     : (zh ? "目前無航班資料" : "NO FLIGHTS");
    drawFit(msg, 0, ROW_Y0, SCREEN_W, ROWS_PER_PAGE * ROW_H, C_DIM, F16, textdatum_t::middle_center);
  } else {
    uint32_t el = millis() - v.animStart;
    for (int r = 0; r < ROWS_PER_PAGE; r++) {
      int y = ROW_Y0 + r * ROW_H;
      int idx = v.page * ROWS_PER_PAGE + r;
      if (el < (uint32_t)r * ROW_ANIM_MS + ROW_ANIM_MS) drawFlapRow(y, r & 1);
      else if (idx < (int)fl.size()) drawRow(v, fl[idx], y, r & 1);
      else s.fillRect(0, y, SCREEN_W, ROW_H, (r & 1) ? C_ROW_ALT : C_BG);
    }
  }
  drawFooter(v);
}

void drawInfoCell(int x, int y, const char* label, const char* value, uint32_t color = C_TEXT) {
  drawFit(label, x, y, 102, 14, C_DIM, F12);
  drawFit(*value ? value : "-", x, y + 14, 102, 18, color, F16B);
}

void drawDetail(const View& v) {
  auto& s = g_canvas;
  const Flight& f = *v.detail;
  bool zh = zhPhase();
  bool dep = v.dir == DIR_DEP;
  s.fillScreen(C_BG);

  // 標題列：航班號、航空公司、狀態
  s.fillRect(0, 0, SCREEN_W, 28, C_HEADER);
  drawFit(f.flight[0], 8, 0, 80, 28, C_HEADER_TXT, F16B);
  drawFit(f.airline, 84, 0, 120, 28, C_HEADER_TXT, F16);
  const char* st = zh ? f.statusZh : statusEnShort(f);
  if (!*st) st = *f.statusZh ? f.statusZh : statusEnShort(f);
  if (*st) {
    s.setFont(F16B);
    int w = std::min(110, (int)s.textWidth(st) + 12);
    s.fillRoundRect(SCREEN_W - w - 6, 4, w, 20, 4, 0x10203A);
    drawFit(st, SCREEN_W - w - 6, 4, w, 20, statusColor(f.kind), F16B, textdatum_t::middle_center);
  }

  // 航線：起點 ✈ 終點
  const char* here = v.ap == AP_TPE ? "TPE" : "TSA";
  const char* hereZh = v.ap == AP_TPE ? "桃園" : "台北松山";
  const char* fromCode = dep ? here : f.cityCode;
  const char* toCode = dep ? f.cityCode : here;
  const char* cityName = zh && *f.cityZh ? f.cityZh : f.cityEn;
  const char* fromName = dep ? (zh ? hereZh : (v.ap == AP_TPE ? "Taoyuan" : "Taipei Songshan")) : cityName;
  const char* toName = dep ? cityName : (zh ? hereZh : (v.ap == AP_TPE ? "Taoyuan" : "Taipei Songshan"));
  drawFit(fromCode, 10, 32, 110, 26, C_TIME, F24B);
  drawFit(fromName, 10, 58, 120, 16, C_TEXT, F12);
  drawPlane(SCREEN_W / 2, 48, 1.3f, !dep, C_HEADER);
  drawFit(toCode, SCREEN_W - 120, 32, 110, 26, C_TIME, F24B, textdatum_t::middle_right);
  drawFit(toName, SCREEN_W - 130, 58, 120, 16, C_TEXT, F12, textdatum_t::middle_right);
  if (*f.viaCode) {
    char via[40];
    snprintf(via, sizeof(via), "%s %s", zh ? "經" : "VIA", *f.viaZh && zh ? f.viaZh : f.viaCode);
    drawFit(via, SCREEN_W / 2 - 40, 60, 80, 14, C_DIM, F12, textdatum_t::middle_center);
  }
  s.drawFastHLine(8, 77, SCREEN_W - 16, C_COLHDR);

  // 資訊格
  char sched[16], est[16], term[16];
  fmtTime(f.sched, sched, sizeof(sched));
  if (dayDiff(f.sched, v.now) > 0) strcat(sched, " +1");
  est[0] = 0;
  if (f.est) fmtTime(f.est, est, sizeof(est));
  bool actual = f.kind == ST_DONE;
  uint32_t estCol = (f.est && llabs((long long)(f.est - f.sched)) >= 5 * 60) ? C_AMBER : C_TEXT;
  snprintf(term, sizeof(term), "%s%s", *f.terminal ? "T" : "", f.terminal);

  int y1 = 80, y2 = 113;
  drawInfoCell(8, y1, zh ? "表定時間" : "SCHEDULED", sched, C_TIME);
  drawInfoCell(112, y1, actual ? (zh ? "實際時間" : "ACTUAL") : (zh ? "預計時間" : "ESTIMATED"), est, estCol);
  if (dep) drawInfoCell(216, y1, zh ? "登機門" : "GATE", f.gate);
  else drawInfoCell(216, y1, v.ap == AP_TPE ? (zh ? "行李轉盤" : "BAGGAGE BELT") : (zh ? "機坪" : "STAND"),
                    v.ap == AP_TPE ? f.belt : f.gate);
  drawInfoCell(8, y2, zh ? "航廈" : "TERMINAL", term);
  if (dep) drawInfoCell(112, y2, zh ? "報到櫃台" : "CHECK-IN", f.counter);
  else drawInfoCell(112, y2, zh ? "停機位" : "GATE", f.gate);
  drawInfoCell(216, y2, zh ? "機型" : "AIRCRAFT", f.aircraft);

  // 共用班號
  s.fillRect(0, 148, SCREEN_W, SCREEN_H - 148, C_ROW_ALT);
  char cs[80] = "";
  for (int i = 1; i < f.nFlight; i++) {
    strcat(cs, f.flight[i]);
    if (i + 1 < f.nFlight) strcat(cs, " · ");
  }
  char line[110];
  if (*cs) snprintf(line, sizeof(line), "%s %s", zh ? "共用班號" : "CODESHARE", cs);
  else snprintf(line, sizeof(line), "%s", zh ? "輕觸或按鍵返回" : "TAP OR PRESS TO RETURN");
  drawFit(line, 8, 148, SCREEN_W - 16, SCREEN_H - 148, *cs ? C_CODESHARE : C_DIM, F12);
}

void drawToast(const char* msg) {
  auto& s = g_canvas;
  s.setFont(F16);
  int w = std::min(SCREEN_W - 20, (int)s.textWidth(msg) + 24);
  int h = 28;
  int x = (SCREEN_W - w) / 2, y = SCREEN_H - h - 14;
  s.fillRoundRect(x, y, w, h, 6, 0x000000);
  s.drawRoundRect(x, y, w, h, 6, C_HEADER);
  drawFit(msg, x, y, w, h, C_TEXT, F16, textdatum_t::middle_center);
}

}  // namespace

void uiInit(LGFX* tft) {
  g_tft = tft;
  g_canvas.setColorDepth(16);
  g_canvas.setPsram(true);
  g_canvas.createSprite(SCREEN_W, SCREEN_H);
}

bool uiAnimating(const View& v) {
  return millis() - v.animStart < (ROWS_PER_PAGE + 1) * ROW_ANIM_MS;
}

void uiRender(const View& v) {
  if (v.detail) drawDetail(v);
  else drawBoard(v);
  if (v.toast) drawToast(v.toast);
  g_canvas.pushSprite(g_tft, 0, 0);
}

void uiMessage(const char* zh, const char* en, const char* sub) {
  auto& s = g_canvas;
  s.fillScreen(C_BG);
  s.fillRect(0, 0, SCREEN_W, HEADER_H, C_HEADER);
  drawPlane(13, HEADER_H / 2, 0.9f, false, C_HEADER_TXT);
  drawFit("航班資訊 FLIGHT INFORMATION", 27, 0, 280, HEADER_H, C_HEADER_TXT, F16B);
  drawFit(zh, 0, 50, SCREEN_W, 24, C_TEXT, F16B, textdatum_t::middle_center);
  drawFit(en, 0, 76, SCREEN_W, 20, C_DIM, F16, textdatum_t::middle_center);
  if (sub) drawFit(sub, 0, 110, SCREEN_W, 40, C_TIME, F16, textdatum_t::middle_center);
  g_canvas.pushSprite(g_tft, 0, 0);
}
