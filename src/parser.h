#pragma once
// 航班資料解析（不含網路），可在電腦上用 sim/ 測試
#include "flight.h"

// 桃園機場 a_flight_v4.txt (Big5 CSV)，逐段餵入資料
class TpeParser {
 public:
  std::vector<Flight> dep, arr;
  int lines = 0;

  TpeParser(time_t from, time_t to) : from_(from), to_(to) {}
  void feed(const uint8_t* buf, size_t n);
  void finish();

 private:
  time_t from_, to_;
  char line_[512];
  size_t len_ = 0;
  void parseLine();
};

// 松山機場 GetFormaterData JSON（國際或國內，出發或抵達）
bool parseTsaJson(const char* body, size_t len, Direction dir, bool domestic, time_t now,
                  std::vector<Flight>& out);

void sortFlights(std::vector<Flight>& v);
bool isFlightVisible(const Flight& f, time_t now);
