#pragma once
#include <Arduino.h>
#include <time.h>
#include <vector>

enum Airport : uint8_t { AP_TPE = 0, AP_TSA = 1, AP_COUNT };
enum Direction : uint8_t { DIR_DEP = 0, DIR_ARR = 1, DIR_COUNT };

enum StatusKind : uint8_t {
  ST_NONE,      // 無狀態
  ST_ONTIME,    // 準時
  ST_DELAY,     // 延誤
  ST_CHANGE,    // 時間更改
  ST_BOARDING,  // 登機 / 最後登機
  ST_CLOSED,    // 關艙
  ST_DONE,      // 已飛 / 已到
  ST_CANCEL,    // 取消
  ST_OTHER,
};

constexpr int MAX_CODESHARE = 6;

struct Flight {
  time_t sched = 0;  // 表定時間
  time_t est = 0;    // 預計/實際時間，0 = 未知
  char flight[MAX_CODESHARE][9] = {};  // [0] 為主航班，其餘為共用班號
  uint8_t nFlight = 0;
  char airline[28] = {};   // 航空公司（中文）
  char cityCode[4] = {};   // 目的地/出發地 IATA
  char cityZh[28] = {};
  char cityEn[28] = {};
  char viaCode[4] = {};    // 經停
  char viaZh[28] = {};
  char gate[6] = {};
  char terminal[3] = {};
  char belt[6] = {};       // 行李轉盤
  char counter[16] = {};   // 報到櫃台
  char aircraft[12] = {};
  char statusZh[16] = {};
  char statusEn[24] = {};
  StatusKind kind = ST_NONE;
  bool domestic = false;   // 松山國內線
};

// 由資料抓取工作 (fetcher.cpp) 提供
void fetcherStart();
void fetcherRequestRefresh();
// 取得目前看板上應顯示的航班（已過濾舊航班），回傳資料版本號
uint32_t fetcherSnapshot(Airport ap, Direction dir, time_t now, std::vector<Flight>& out,
                         time_t& updated, bool& ok);
uint32_t fetcherVersion();
bool fetcherBusy();
