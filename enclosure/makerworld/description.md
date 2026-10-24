# 機場航班看板桌面顯示器｜LilyGO T-Display S3 Touch 外殼
# Airport FIDS Desk Display – LilyGO T-Display S3 Touch Enclosure

---

## 中文

把 LilyGO T-Display S3 **Touch** 變成一個迷你的機場航班資訊看板（FIDS）。側面是倒 T 字形，就像航廈裡立著的看板，即時顯示桃園機場、松山機場的出發／抵達航班。

**特色**
- 免螺絲：板子從背面放入前框，背蓋用 4 個卡鉤扣上
- BOOT / KEY 兩個按鍵可以從正面按（前框上的彈片）
- 觸控螢幕全區可用，USB-C 從左側接線
- 前框模組可以單獨使用，也可以嵌進看板面板

**需要的硬體**
- LilyGO T-Display S3 **Touch** 版（1.9" 170×320，有觸控玻璃的版本；非觸控版尺寸不同，裝不進去）
- USB-C 線

**零件與列印**

| 檔案 | 零件 | 列印方向 |
|---|---|---|
| 1_front_shell.stl | 前框 | 正面朝下 |
| 2_back_cover.stl | 背蓋 | 外側平面朝下，卡鉤朝上 |
| 3_panel.stl | 看板面板 100×60×20 | 正面朝下 |
| 4_base.stl | 底座 110×70×8 | 底面朝下 |

- PLA 或 PETG 都可以，0.2 mm 層高，3 層牆，15% 填充，**不需要支撐**
- 建議全部用黑色
- 面板背面擋邊有約 1.5 mm 的小懸空，一般都能印好

**組裝**
1. 前框正面朝下，板子螢幕朝下、USB-C 對準側面開口，從背面放進去
2. 壓上背蓋，聽到「喀」聲表示 4 個卡鉤都扣上了
3. 面板下緣插進底座的槽
4. USB 線從面板左側孔穿入並接上板子，再把前框模組 USB 端朝左、從正面壓進面板
5. 拆背蓋：用牙籤從前框側面的方孔把卡鉤往內推，再從缺口撬開

**韌體**
ESP32-S3 Arduino（PlatformIO），資料來自桃園機場、松山機場官方公開航班資料，每 3 分鐘更新一次。
原始碼與燒錄說明：https://github.com/TN666/tiny-fids
- 觸控：上下滑換頁、左右滑切換機場、點標題切換出發／抵達、點航班看詳細資訊、長按強制更新
- 按鍵：BOOT 切換機場（長按調亮度），KEY 換頁（長按切換出發／抵達）
- 第一次開機時，用手機連 Wi-Fi「FIDS-Setup」設定網路

**注意**
不同批次的板子厚度可能略有差異。按鍵按不到的話，把前框內側頂柱的尖端墊高一點；一直按住的話，把尖端磨短一點。

---

## English

Turn a LilyGO T-Display S3 **Touch** into a tiny airport Flight Information Display (FIDS). The inverted-T profile mimics the standing information boards in airport terminals. It shows live departures and arrivals for Taipei Taoyuan (TPE) and Taipei Songshan (TSA).

**Features**
- Screwless: the board drops in from the back, and the back cover clips on with 4 snap hooks
- BOOT / KEY buttons are pressable from the front (flexure buttons)
- Full touch area exposed; USB-C on the left side
- The front shell module works on its own, or slots into the display panel

**Hardware**
- LilyGO T-Display S3 **Touch** (1.9" 170×320 with touch glass; the non-touch version has different dimensions and will not fit)
- USB-C cable

**Printing**
- PLA or PETG, 0.2 mm layers, 3 walls, 15% infill, **no supports**
- Black filament recommended
- Orientation: shell and panel face down; back cover outer face down; base flat

**Assembly**
1. Place the shell face down, then insert the board screen-first from the back with USB-C aligned to the side opening
2. Press the back cover in until all 4 hooks click
3. Slide the panel's bottom edge into the slot in the base
4. Route the USB cable through the hole on the left of the panel and plug it in, then press the shell module into the panel from the front with the USB end on the left
5. To remove the back cover, push the hooks inward through the small side windows with a toothpick, then pry the cover out at the notch

**Firmware**
ESP32-S3 Arduino (PlatformIO), using the official open flight data from Taoyuan and Songshan airports, refreshed every 3 minutes. Source code and flashing guide: https://github.com/TN666/tiny-fids
On first boot, connect to the "FIDS-Setup" Wi-Fi network to configure Wi-Fi.

**Note**
Board thickness can vary slightly between batches. If the buttons don't register, build up the plunger tips inside the shell a little; if a button stays pressed, sand the tip down a little.

---

**Tags**: LilyGO, T-Display S3, ESP32, ESP32-S3, FIDS, airport, flight board, flight tracker, enclosure, case, desk display, Taiwan, 桃園機場, 松山機場
