#pragma once

// ---- Wi-Fi ----
// 留空則使用 WiFiManager 設定頁（手機連線到 AP「FIDS-Setup」設定）。
// 也可以直接填入，開機會先試這組。
#define WIFI_SSID ""
#define WIFI_PASS ""
#define WIFI_PORTAL_AP "FIDS-Setup"

// ---- 時區 / NTP ----
#define TZ_INFO "CST-8"
#define NTP_SERVER1 "tock.stdtime.gov.tw"
#define NTP_SERVER2 "pool.ntp.org"

// ---- 資料來源（皆為官方開放資料，免金鑰）----
// 桃園機場：兩個航廈、出發+抵達都在同一個 Big5 CSV 檔
#define TPE_URL "https://www.taoyuan-airport.com/uploads/flightx/a_flight_v4.txt"
// 松山機場：國際/國內 × 出發/抵達 四個 JSON
#define TSA_URL_BASE "https://www.tsa.gov.tw/api/publicDataArea/GetFormaterData?id="
#define TSA_ID_INTL_DEP "42879f51-f47f-4d26-8b2b-5535c652cbde"
#define TSA_ID_DOM_DEP "c0f7d5b4-ba73-46d2-8485-6595c64c4e17"
#define TSA_ID_INTL_ARR "7dc1379a-9485-4491-866d-fc4f9590ffcf"
#define TSA_ID_DOM_ARR "3057d52f-7a71-49e1-a0d4-87ffa3449a6a"

#define REFRESH_INTERVAL_MS (3UL * 60 * 1000)

// ---- 看板行為 ----
#define ROWS_PER_PAGE 6
#define AUTO_PAGE_MS 8000UL          // 自動翻頁間隔
#define AUTO_PAGE_MAX 5              // 自動翻頁只輪播前幾頁
#define AUTO_PAGE_RESUME_MS 30000UL  // 手動操作後暫停自動翻頁的時間
#define LANG_PHASE_MS 2500UL         // 中/英文切換間隔
#define DETAIL_TIMEOUT_MS 20000UL    // 航班詳細頁自動返回

// 螢幕方向：1 或 3（USB 在右/在左）
#define SCREEN_ROTATION 3

// ---- 腳位 (T-Display S3 Touch) ----
#define PIN_POWER_ON 15
#define PIN_BTN_BOOT 0
#define PIN_BTN_KEY 14
