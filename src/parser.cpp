#include "parser.h"

#include <ArduinoJson.h>
#include <ctype.h>
#include <string.h>

#include <algorithm>

#include "big5_table.h"

namespace {

size_t utf8Encode(uint16_t u, char* out) {
  if (u < 0x80) {
    out[0] = u;
    return 1;
  }
  if (u < 0x800) {
    out[0] = 0xC0 | (u >> 6);
    out[1] = 0x80 | (u & 0x3F);
    return 2;
  }
  out[0] = 0xE0 | (u >> 12);
  out[1] = 0x80 | ((u >> 6) & 0x3F);
  out[2] = 0x80 | (u & 0x3F);
  return 3;
}

void big5ToUtf8(const char* in, char* out, size_t cap) {
  size_t o = 0;
  const uint8_t* p = (const uint8_t*)in;
  while (*p && o + 4 < cap) {
    uint8_t c = *p;
    if (c < 0x80) {
      out[o++] = c;
      p++;
      continue;
    }
    if (c >= 0x81 && c <= 0xFE && p[1]) {
      uint8_t t = p[1];
      int ti = -1;
      if (t >= 0x40 && t <= 0x7E) ti = t - 0x40;
      else if (t >= 0xA1 && t <= 0xFE) ti = t - 0xA1 + 63;
      uint16_t u = ti >= 0 ? BIG5_TABLE[(c - 0x81) * 157 + ti] : 0;
      o += utf8Encode(u ? u : '?', out + o);
      p += 2;
    } else {
      p++;
    }
  }
  out[o] = 0;
}

// 去除前後空白並複製，截斷時不切斷 UTF-8 字元
void copyTrim(char* dst, size_t cap, const char* src) {
  while (*src == ' ' || *src == '\t') src++;
  size_t n = strlen(src);
  while (n && (src[n - 1] == ' ' || src[n - 1] == '\t' || src[n - 1] == '\r')) n--;
  if (n >= cap) {
    n = cap - 1;
    while (n && (((uint8_t)src[n]) & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

// "準時ON TIME" -> zh="準時" en="ON TIME"
void splitStatus(Flight& f, const char* s) {
  while (*s == ' ') s++;
  const char* p = s;
  while (*p && ((uint8_t)*p) >= 0x80) p++;
  char zh[sizeof(f.statusZh)];
  size_t n = std::min((size_t)(p - s), sizeof(zh) - 1);
  memcpy(zh, s, n);
  zh[n] = 0;
  copyTrim(f.statusZh, sizeof(f.statusZh), zh);
  copyTrim(f.statusEn, sizeof(f.statusEn), p);
  for (char* c = f.statusEn; *c; c++) *c = toupper((uint8_t)*c);

  const char* e = f.statusEn;
  const char* z = f.statusZh;
  if (!*e && !*z) f.kind = ST_NONE;
  else if (strstr(e, "CANCEL") || strstr(z, "取消")) f.kind = ST_CANCEL;
  else if (strstr(e, "DELAY") || strstr(z, "延")) f.kind = ST_DELAY;
  else if (strstr(e, "BOARD") || strstr(e, "CALL") || strstr(z, "登機")) f.kind = ST_BOARDING;
  else if (strstr(e, "CLOSE")) f.kind = ST_CLOSED;
  else if (strstr(e, "DEPART") || strstr(e, "ARRIV") || strstr(e, "LAND") || strstr(z, "已")) f.kind = ST_DONE;
  else if (strstr(e, "CHANGE")) f.kind = ST_CHANGE;
  else if (strstr(e, "ON TIME") || strstr(z, "準時")) f.kind = ST_ONTIME;
  else f.kind = ST_OTHER;
}

time_t makeLocal(int y, int mo, int d, int h, int mi) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = 0;
  return mktime(&t);
}

// 合併共用班號：同時間、同航點、同登機門視為同一班機
void addOrMerge(std::vector<Flight>& v, Flight& f, bool isMaster) {
  for (auto& o : v) {
    if (o.sched != f.sched || strcmp(o.cityCode, f.cityCode) != 0 || strcmp(o.gate, f.gate) != 0 ||
        o.domestic != f.domestic)
      continue;
    for (int i = 0; i < o.nFlight; i++)
      if (!strcmp(o.flight[i], f.flight[0])) return;
    if (o.nFlight >= MAX_CODESHARE) return;
    if (isMaster) {  // 實際營運航班放到最前面
      for (int i = o.nFlight; i > 0; i--) strcpy(o.flight[i], o.flight[i - 1]);
      strcpy(o.flight[0], f.flight[0]);
      strcpy(o.airline, f.airline);
      if (!*o.aircraft) strcpy(o.aircraft, f.aircraft);
    } else {
      strcpy(o.flight[o.nFlight], f.flight[0]);
    }
    o.nFlight++;
    return;
  }
  v.push_back(f);
}

int hhmm(const char* s) {  // "1620" / "16:20" -> 分鐘數, -1 = 無
  int v = 0, digits = 0;
  for (; *s; s++) {
    if (isdigit((uint8_t)*s)) {
      v = v * 10 + (*s - '0');
      digits++;
    }
  }
  if (digits < 3) return -1;
  return (v / 100) * 60 + v % 100;
}

}  // namespace

// ---------- 桃園機場 ----------
// 欄位：航廈,A/D,航空公司代碼,航空公司,班次,登機門,表定日期,表定時間,預計日期,預計時間,
//       航點代碼,航點英文,航點中文,狀態,機型,經停代碼,經停英文,經停中文,行李轉盤,報到櫃台

void TpeParser::feed(const uint8_t* buf, size_t n) {
  for (size_t i = 0; i < n; i++) {
    uint8_t c = buf[i];
    if (c == '\n') {
      line_[len_] = 0;
      parseLine();
      len_ = 0;
    } else if (c != '\r' && len_ < sizeof(line_) - 1) {
      line_[len_++] = c;
    }
  }
}

void TpeParser::finish() {
  if (len_) {
    line_[len_] = 0;
    parseLine();
    len_ = 0;
  }
  sortFlights(dep);
  sortFlights(arr);
}

void TpeParser::parseLine() {
  char* fld[20];
  int n = 0;
  char* p = line_;
  fld[n++] = p;
  while (*p && n < 20) {
    if (*p == ',') {
      *p = 0;
      fld[n++] = p + 1;
    }
    p++;
  }
  if (n < 20) return;
  lines++;

  int y, mo, d, h, mi;
  if (sscanf(fld[6], "%d/%d/%d", &y, &mo, &d) != 3 || sscanf(fld[7], "%d:%d", &h, &mi) != 2) return;
  Flight f;
  f.sched = makeLocal(y, mo, d, h, mi);
  if (f.sched < from_ || f.sched > to_) return;
  if (sscanf(fld[8], "%d/%d/%d", &y, &mo, &d) == 3 && sscanf(fld[9], "%d:%d", &h, &mi) == 2)
    f.est = makeLocal(y, mo, d, h, mi);

  char code[4], num[8], tmp[96];
  copyTrim(code, sizeof(code), fld[2]);
  copyTrim(num, sizeof(num), fld[4]);
  snprintf(f.flight[0], sizeof(f.flight[0]), "%s %s", code, num);
  f.nFlight = 1;
  big5ToUtf8(fld[3], tmp, sizeof(tmp));
  copyTrim(f.airline, sizeof(f.airline), tmp);
  copyTrim(f.gate, sizeof(f.gate), fld[5]);
  copyTrim(f.cityCode, sizeof(f.cityCode), fld[10]);
  copyTrim(f.cityEn, sizeof(f.cityEn), fld[11]);
  big5ToUtf8(fld[12], tmp, sizeof(tmp));
  copyTrim(f.cityZh, sizeof(f.cityZh), tmp);
  big5ToUtf8(fld[13], tmp, sizeof(tmp));
  splitStatus(f, tmp);
  copyTrim(f.aircraft, sizeof(f.aircraft), fld[14]);
  if (!strcmp(f.aircraft, "-")) f.aircraft[0] = 0;
  copyTrim(f.viaCode, sizeof(f.viaCode), fld[15]);
  big5ToUtf8(fld[17], tmp, sizeof(tmp));
  copyTrim(f.viaZh, sizeof(f.viaZh), tmp);
  copyTrim(f.belt, sizeof(f.belt), fld[18]);
  copyTrim(f.counter, sizeof(f.counter), fld[19]);
  copyTrim(f.terminal, sizeof(f.terminal), fld[0]);

  // 檔案中實際營運航班排在共用班號前面
  addOrMerge(fld[1][0] == 'D' ? dep : arr, f, false);
}

// ---------- 松山機場 ----------

bool parseTsaJson(const char* body, size_t len, Direction dir, bool domestic, time_t now,
                  std::vector<Flight>& out) {
  JsonDocument filter;
  for (const char* k :
       {"AirFlyDate", "AirLineIATA", "AirLineName", "AirLineNum", "AirBoardingGate", "UpAirportCode",
        "UpAirportName", "UpAirportNameEn", "GoalAirportCode", "GoalAirportName", "GoalAirportNameEn",
        "ExpectDepartureTime", "RealDepartureTime", "ExpectArrivalTime", "RealArrivalTime", "AirFlyStatus",
        "AirPlaneType", "CheckInCount", "Master"})
    filter[0][k] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, len, DeserializationOption::Filter(filter))) return false;
  if (!doc.is<JsonArray>()) return false;

  struct tm lt;
  localtime_r(&now, &lt);
  int year = lt.tm_year + 1900;
  bool isDep = dir == DIR_DEP;
  const char* kCode = isDep ? "GoalAirportCode" : "UpAirportCode";
  const char* kName = isDep ? "GoalAirportName" : "UpAirportName";
  const char* kNameEn = isDep ? "GoalAirportNameEn" : "UpAirportNameEn";

  for (JsonObject o : doc.as<JsonArray>()) {
    const char* date = o["AirFlyDate"] | "";
    if (strlen(date) < 4) continue;
    int mo = (date[0] - '0') * 10 + (date[1] - '0');
    int d = (date[2] - '0') * 10 + (date[3] - '0');
    int y = year;
    if (mo - (lt.tm_mon + 1) > 6) y--;  // 跨年
    else if ((lt.tm_mon + 1) - mo > 6) y++;

    int sm = hhmm(o[isDep ? "ExpectDepartureTime" : "ExpectArrivalTime"] | "");
    if (sm < 0) continue;
    int am = hhmm(o[isDep ? "RealDepartureTime" : "RealArrivalTime"] | "");

    Flight f;
    f.domestic = domestic;
    f.sched = makeLocal(y, mo, d, sm / 60, sm % 60);
    if (am >= 0) {
      f.est = makeLocal(y, mo, d, am / 60, am % 60);
      if (f.est < f.sched - 12 * 3600) f.est += 86400;
      else if (f.est > f.sched + 12 * 3600) f.est -= 86400;
    }
    snprintf(f.flight[0], sizeof(f.flight[0]), "%s %s", (const char*)(o["AirLineIATA"] | ""),
             (const char*)(o["AirLineNum"] | ""));
    f.nFlight = 1;
    copyTrim(f.airline, sizeof(f.airline), o["AirLineName"] | "");
    copyTrim(f.cityCode, sizeof(f.cityCode), o[kCode] | "");
    copyTrim(f.cityZh, sizeof(f.cityZh), o[kName] | "");
    copyTrim(f.cityEn, sizeof(f.cityEn), o[kNameEn] | "");
    if (!*f.cityEn) strcpy(f.cityEn, f.cityCode);
    copyTrim(f.gate, sizeof(f.gate), o["AirBoardingGate"] | "");
    copyTrim(f.aircraft, sizeof(f.aircraft), o["AirPlaneType"] | "");
    copyTrim(f.counter, sizeof(f.counter), o["CheckInCount"] | "");
    strcpy(f.terminal, domestic ? "2" : "1");  // 松山：第一航廈國際線、第二航廈國內線
    splitStatus(f, o["AirFlyStatus"] | "");
    const char* master = o["Master"] | "1";
    addOrMerge(out, f, master[0] == '1');
  }
  return true;
}

void sortFlights(std::vector<Flight>& v) {
  std::stable_sort(v.begin(), v.end(), [](const Flight& a, const Flight& b) {
    if (a.sched != b.sched) return a.sched < b.sched;
    return strcmp(a.flight[0], b.flight[0]) < 0;
  });
}

bool isFlightVisible(const Flight& f, time_t now) {
  time_t actual = f.est ? f.est : f.sched;
  if (f.kind == ST_DONE) return actual >= now - 15 * 60;
  if (f.kind == ST_CANCEL) return f.sched >= now - 30 * 60;
  return std::max(f.sched, f.est) >= now - 60 * 60;
}
