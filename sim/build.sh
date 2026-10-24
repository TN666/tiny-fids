#!/bin/bash
# 在 macOS/Linux 上編譯並執行模擬器：sim/build.sh [輸出資料夾]
set -e
cd "$(dirname "$0")/.."
LIB=.pio/libdeps/tdisplay-s3-touch
[ -d "$LIB/LovyanGFX" ] || ~/.platformio/penv/bin/pio pkg install
OUT=${1:-sim/out}
DATA=$OUT/data
mkdir -p "$DATA" sim/build

# 下載最新資料
curl -s -m 60 -A "Mozilla/5.0" -o "$DATA/tpe.txt" "https://www.taoyuan-airport.com/uploads/flightx/a_flight_v4.txt"
B="https://www.tsa.gov.tw/api/publicDataArea/GetFormaterData?id="
curl -s -m 30 -o "$DATA/tsa_id.json" "${B}42879f51-f47f-4d26-8b2b-5535c652cbde"
curl -s -m 30 -o "$DATA/tsa_dd.json" "${B}c0f7d5b4-ba73-46d2-8485-6595c64c4e17"
curl -s -m 30 -o "$DATA/tsa_ia.json" "${B}7dc1379a-9485-4491-866d-fc4f9590ffcf"
curl -s -m 30 -o "$DATA/tsa_da.json" "${B}3057d52f-7a71-49e1-a0d4-87ffa3449a6a"

# LovyanGFX 函式庫只需編譯一次
L=$LIB/LovyanGFX/src
if [ ! -f sim/build/liblgfx.a ]; then
  for f in $L/lgfx/v1/*.cpp $L/lgfx/v1/misc/*.cpp $L/lgfx/v1/panel/Panel_Device.cpp $L/lgfx/v1/platforms/sdl/*.cpp; do
    clang++ -std=c++17 -O2 -c -I$L $(sdl2-config --cflags) "$f" -o "sim/build/$(basename "$f").o"
  done
  for f in $L/lgfx/utility/*.c $L/lgfx/Fonts/efont/*.c $L/lgfx/Fonts/IPA/*.c; do
    clang -O2 -c -I$L "$f" -o "sim/build/$(basename "$f").o"
  done
  ar rcs sim/build/liblgfx.a sim/build/*.o
fi

clang++ -std=c++17 -O1 -DFIDS_SIM -Isim -Isrc -I$L -I$LIB/ArduinoJson/src $(sdl2-config --cflags) \
  src/ui.cpp src/parser.cpp sim/sim_main.cpp sim/build/liblgfx.a $(sdl2-config --libs) -o sim/build/fids_sim
sim/build/fids_sim "$DATA" "$OUT" $2

for f in "$OUT"/*.ppm; do sips -s format png "$f" --out "${f%.ppm}.png" >/dev/null && rm "$f"; done
echo "screenshots in $OUT"
