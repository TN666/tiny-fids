#pragma once
// 電腦模擬用的最小 Arduino 介面
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <algorithm>

extern uint32_t g_simMillis;
inline uint32_t millis() { return g_simMillis; }
#define log_i(...)
#define log_e(...)
