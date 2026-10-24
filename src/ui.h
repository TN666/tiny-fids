#pragma once
#include "flight.h"
#include "lgfx_tdisplay_s3.h"

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 170;
constexpr int HEADER_H = 24;
constexpr int COLHDR_H = 16;
constexpr int ROW_Y0 = HEADER_H + COLHDR_H;
constexpr int ROW_H = 21;

struct View {
  Airport ap = AP_TPE;
  Direction dir = DIR_DEP;
  const std::vector<Flight>* flights = nullptr;
  int page = 0;
  int pages = 1;
  uint32_t animStart = 0;  // 翻頁動畫開始時間
  const Flight* detail = nullptr;
  time_t now = 0;
  time_t updated = 0;
  bool fetchOk = true;
  bool loading = false;
  const char* toast = nullptr;
};

void uiInit(LGFX* tft);
void uiRender(const View& v);
void uiMessage(const char* zh, const char* en, const char* sub = nullptr);
bool uiAnimating(const View& v);
