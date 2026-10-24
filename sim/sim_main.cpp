// 在電腦上用真實資料渲染看板畫面，輸出 PNG 檢查介面
// 用法：sim/build.sh 會下載資料並執行
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include <string>

#include "config.h"
#include "parser.h"
#include "ui.h"

uint32_t g_simMillis = 100000;

static std::string readFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static void savePPM(LGFX& scr, const std::string& path, int scale = 2) {
  FILE* fp = fopen(path.c_str(), "wb");
  fprintf(fp, "P6\n%d %d\n255\n", SCREEN_W * scale, SCREEN_H * scale);
  for (int y = 0; y < SCREEN_H * scale; y++)
    for (int x = 0; x < SCREEN_W * scale; x++) {
      auto c = scr.readPixelRGB(x / scale, y / scale);
      uint8_t px[3] = {c.r, c.g, c.b};
      fwrite(px, 1, 3, fp);
    }
  fclose(fp);
}

int main(int argc, char** argv) {
  setenv("TZ", "CST-8", 1);
  tzset();
  std::string dir = argc > 1 ? argv[1] : ".";
  std::string out = argc > 2 ? argv[2] : ".";
  time_t now = argc > 3 ? (time_t)atoll(argv[3]) : time(nullptr);

  std::vector<Flight> boards[AP_COUNT][DIR_COUNT];
  std::string tpe = readFile(dir + "/tpe.txt");
  TpeParser tp(now - 12 * 3600, now + 36 * 3600);
  tp.feed((const uint8_t*)tpe.data(), tpe.size());
  tp.finish();
  boards[AP_TPE][DIR_DEP] = tp.dep;
  boards[AP_TPE][DIR_ARR] = tp.arr;
  printf("TPE lines=%d dep=%zu arr=%zu\n", tp.lines, tp.dep.size(), tp.arr.size());

  const char* tsaFiles[2][2] = {{"tsa_id.json", "tsa_dd.json"}, {"tsa_ia.json", "tsa_da.json"}};
  for (int d = 0; d < 2; d++) {
    for (int dom = 0; dom < 2; dom++) {
      std::string body = readFile(dir + "/" + tsaFiles[d][dom]);
      bool ok = parseTsaJson(body.data(), body.size(), (Direction)d, dom, now, boards[AP_TSA][d]);
      printf("TSA %s ok=%d\n", tsaFiles[d][dom], ok);
    }
    sortFlights(boards[AP_TSA][d]);
  }

  LGFX scr;
  scr.setColorDepth(16);
  scr.createSprite(SCREEN_W, SCREEN_H);
  uiInit(&scr);

  const char* names[2][2] = {{"tpe_dep", "tpe_arr"}, {"tsa_dep", "tsa_arr"}};
  for (int a = 0; a < 2; a++)
    for (int d = 0; d < 2; d++) {
      std::vector<Flight> vis;
      for (auto& f : boards[a][d])
        if (isFlightVisible(f, now)) vis.push_back(f);
      printf("%s visible=%zu\n", names[a][d], vis.size());
      View v;
      v.ap = (Airport)a;
      v.dir = (Direction)d;
      v.flights = &vis;
      v.pages = std::max<int>(1, (vis.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
      v.now = now;
      v.updated = now;
      v.animStart = 0;
      for (int phase = 0; phase < 3; phase++) {
        g_simMillis = 100000 + phase * LANG_PHASE_MS + 100;
        g_simMillis = (g_simMillis / (3 * LANG_PHASE_MS)) * 3 * LANG_PHASE_MS + phase * LANG_PHASE_MS + 100;
        uiRender(v);
        savePPM(scr, out + "/" + names[a][d] + "_p" + std::to_string(phase) + ".ppm");
      }
      // 詳細頁：挑一班有共用班號的
      const Flight* sel = nullptr;
      for (auto& f : vis)
        if (f.nFlight > 1) { sel = &f; break; }
      if (!sel && !vis.empty()) sel = &vis[0];
      if (sel) {
        v.detail = sel;
        for (int phase = 0; phase < 2; phase++) {
          g_simMillis = 300000 - (300000 % (3 * LANG_PHASE_MS)) + phase * LANG_PHASE_MS + 100;
          uiRender(v);
          savePPM(scr, out + "/" + names[a][d] + "_detail_p" + std::to_string(phase) + ".ppm");
        }
      }
    }

  // 翻牌動畫中途 + 提示訊息
  {
    std::vector<Flight> vis;
    for (auto& f : boards[AP_TPE][DIR_DEP])
      if (isFlightVisible(f, now)) vis.push_back(f);
    View v;
    v.flights = &vis;
    v.now = now;
    v.updated = now;
    v.pages = 5;
    v.page = 1;
    g_simMillis = 500000;
    v.animStart = g_simMillis - 140;
    uiRender(v);
    savePPM(scr, out + "/anim.ppm");
    v.animStart = 0;
    v.toast = "松山 TSA 出發 DEP";
    uiRender(v);
    savePPM(scr, out + "/toast.ppm");
  }
  uiMessage("請用手機連線 Wi-Fi 設定", "Connect phone to Wi-Fi AP", "FIDS-Setup  →  192.168.4.1");
  savePPM(scr, out + "/portal.ppm");
  return 0;
}
