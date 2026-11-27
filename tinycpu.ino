#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <pgmspace.h>

#define AP_SSID "CPU-Display"
#define AP_PASS "12345678"

#define BLACK     0x0043
#define WHITE     0xFFFF
#define GREEN     0x07F9
#define CYAN      0x065F
#define MAGENTA   0xF974
#define YELLOW    0xFD80
#define ORANGE    0xA35F
#define LIGHTGREY 0x8D18
#define DIM       0x2189
#define BLU       0x3BDF
#define BTN       9
#define PAGES     7
#define HN        36

Arduino_DataBus *bus = new Arduino_ESP32SPI(10, 1, 4, 3, GFX_NOT_DEFINED);
Arduino_GFX *g = new Arduino_GC9A01(bus, 0, 0, true);
WebServer srv(80);

const char *nm[]    = {"LDA", "LDB", "ADD", "OUT", "JMP"};
const char *micro[] = {"A = RAM", "B = RAM", "A = A + B", "OUT = A", "PC = ADDR"};
const char *lf[]    = {"%d: A <- %d", "%d: B <- %d", "%d: A <- A+B=%d", "%d: OUT <- %d", "%d: PC <- %d"};
uint8_t arg[] = {5, 3, 0, 0, 2};
uint8_t op[]  = {0, 1, 2, 3, 4};
uint8_t ia = 0, pcOld = 0;
bool paused = false, stepReq = false, resetReq = false;
float spd = 1.0f;
const uint8_t N = 5;

uint8_t pc = 0, A = 0, B = 0, outv = 0, ir = 0;
bool clk = false;
int page = 0, ph = 0, tk = 0;
const char *sT = "";
uint16_t sC = WHITE;
const char *cS = "";
char sSub[24];
char buf[24];
char b2[40];
char lg[3][20];

uint8_t hC[HN], hP[HN], hPc[HN], hA[HN], hO[HN];
int pipeS[5] = {-1, -1, -1, -1, -1};
int fpc = 0, cyc = 0, flushN = 0;
bool fl = false;
int dx = -1, dy = -1;
uint32_t seqn = 0;

const int FX = 120, FY = 112, FR = 62;
const int th[3] = {-90, 30, 150};
const uint16_t fc[3] = {GREEN, MAGENTA, CYAN};
const char *fn[3] = {"FETCH", "EXEC", "PC+1"};
const uint16_t ic[5] = {CYAN, BLU, GREEN, YELLOW, MAGENTA};
const char *sn[5] = {"IF", "ID", "EX", "MEM", "WB"};

void box(int x, int y, int w, int h, uint16_t c, bool on) {
  g->drawRoundRect(x, y, w, h, 3, on ? c : DIM);
  g->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 2, on ? c : BLACK);
}

void ln(int x1, int y1, int x2, int y2, bool on) {
  g->drawLine(x1, y1, x2, y2, on ? GREEN : DIM);
}

void t(int x, int y, uint16_t c, const char *s) {
  g->setTextSize(1);
  g->setTextColor(c, BLACK);
  g->setCursor(x, y);
  g->print(s);
}

void ct(int cx, int y, uint16_t c, const char *s, int sz) {
  g->setTextSize(sz);
  g->setTextColor(c, BLACK);
  g->setCursor(cx - (int)strlen(s) * 3 * sz, y);
  g->print(s);
}

void ring() {
  uint16_t c = ph == 0 ? GREEN : (ph == 1 ? MAGENTA : CYAN);
  g->drawCircle(120, 120, 118, c);
  g->drawCircle(120, 120, 117, c);
  g->drawCircle(120, 120, 115, DIM);
  for (int i = 0; i < PAGES; i++) g->fillCircle(90 + i * 10, 224, 2, i == page ? c : DIM);
}

void lines(bool a, bool d) {
  uint16_t ca = a ? GREEN : DIM, cd = d ? GREEN : DIM;
  g->drawFastVLine(47, 54, 20, ca);
  g->drawFastHLine(47, 54, 54, ca);
  g->drawFastVLine(100, 54, 8, ca);
  g->drawFastVLine(47, 160, 16, cd);
  g->drawFastHLine(47, 176, 98, cd);
  g->drawFastVLine(144, 170, 7, cd);
}

void alu(bool on) {
  uint16_t c = on ? GREEN : DIM;
  g->fillRect(84, 130, 32, 34, BLACK);
  g->drawLine(87, 140, 95, 140, c);
  g->drawLine(95, 140, 100, 148, c);
  g->drawLine(100, 148, 105, 140, c);
  g->drawLine(105, 140, 113, 140, c);
  g->drawLine(113, 140, 108, 156, c);
  g->drawLine(108, 156, 92, 156, c);
  g->drawLine(92, 156, 87, 140, c);
}

void alu2(bool on) {
  uint16_t c = on ? GREEN : DIM;
  g->fillRect(174, 62, 32, 50, BLACK);
  g->drawLine(176, 64, 176, 80, c);
  g->drawLine(176, 80, 184, 87, c);
  g->drawLine(184, 87, 176, 94, c);
  g->drawLine(176, 94, 176, 110, c);
  g->drawLine(176, 110, 202, 98, c);
  g->drawLine(202, 98, 202, 76, c);
  g->drawLine(202, 76, 176, 64, c);
}

void wave() {
  if (page == 0) {
    g->fillRect(80, 26, 82, 20, BLACK);
    for (int i = 0; i < 4; i++) {
      int x = 80 + i * 20;
      g->drawFastHLine(x, ((i % 2 == 0) ^ clk) ? 28 : 42, 20, GREEN);
      g->drawFastVLine(x + 20, 28, 15, GREEN);
    }
  } else if (page == 1) {
    g->fillRect(88, 18, 66, 14, BLACK);
    for (int i = 0; i < 4; i++) {
      int x = 90 + i * 15;
      g->drawFastHLine(x, ((i % 2 == 0) ^ clk) ? 20 : 30, 15, GREEN);
      g->drawFastVLine(x + 15, 20, 11, GREEN);
    }
  }
}

void status(const char *s, uint16_t c, const char *sub) {
  g->fillRect(30, 180, 180, 32, BLACK);
  g->setTextSize(2);
  g->setTextColor(c);
  g->setCursor(120 - strlen(s) * 6, 182);
  g->print(s);
  g->setTextSize(1);
  g->setTextColor(LIGHTGREY);
  g->setCursor(120 - strlen(sub) * 3, 202);
  g->print(sub);
}

void status2() {
  g->fillRect(36, 202, 168, 10, BLACK);
  snprintf(b2, sizeof(b2), "%s: %s", sT, sSub);
  g->setTextSize(1);
  g->setTextColor(sC);
  g->setCursor(120 - strlen(b2) * 3, 203);
  g->print(b2);
}

void ctrl(const char *s) {
  g->fillRect(84, 102, 80, 9, BLACK);
  g->setTextSize(1);
  g->setTextColor(ORANGE);
  g->setCursor(124 - strlen(s) * 3, 103);
  g->print(s);
}

void ram() {
  g->setTextSize(1);
  for (int i = 0; i < N; i++) {
    g->setTextColor(i == pc ? WHITE : ORANGE, BLACK);
    g->setCursor(24, 90 + i * 13);
    if (op[i] == 2 || op[i] == 3) snprintf(buf, sizeof(buf), "%d %s   ", i, nm[op[i]]);
    else snprintf(buf, sizeof(buf), "%d %s %-2d", i, nm[op[i]], arg[i]);
    g->print(buf);
  }
}

void regs() {
  g->setTextColor(CYAN, BLACK);
  g->setTextSize(2);
  g->setCursor(98, 69);
  snprintf(buf, sizeof(buf), "%02d", pc);
  g->print(buf);
  g->setTextSize(1);
  g->setTextColor(MAGENTA, BLACK);
  snprintf(buf, sizeof(buf), "A:%3d", A);
  g->setCursor(131, 66); g->print(buf);
  snprintf(buf, sizeof(buf), "B:%3d", B);
  g->setCursor(131, 76); g->print(buf);
  g->setTextColor(WHITE, BLACK);
  g->setCursor(126, 130);
  g->print(nm[ir]);
  g->setTextColor(YELLOW, BLACK);
  g->setCursor(126, 142);
  for (int b = 3; b >= 0; b--) g->print(((ir + 1) >> b) & 1);
  g->setTextColor(CYAN, BLACK);
  g->setCursor(126, 153);
  for (int b = 3; b >= 0; b--) g->print((ia >> b) & 1);
  g->setTextSize(2);
  g->setTextColor(YELLOW, BLACK);
  snprintf(buf, sizeof(buf), "%3d", outv);
  g->setCursor(183, 108);
  g->print(buf);
}

void show(int p) {
  bool f = p == 0, e = p == 1, pl = p == 2;
  box(20, 74, 54, 86, ORANGE, f);
  box(82, 62, 44, 24, CYAN, pl);
  box(128, 62, 38, 24, MAGENTA, e && ir <= 2);
  box(82, 90, 84, 22, ORANGE, e);
  box(82, 116, 36, 50, GREEN, e && ir == 2);
  alu(e && ir == 2);
  box(122, 116, 44, 50, BLU, f);
  box(176, 92, 50, 36, YELLOW, e && ir == 3);
  lines(f || pl, f);
}

void draw2() {
  bool f = ph == 0, e = ph == 1, p = ph == 2;
  bool imm = ir <= 1 || ir == 4;

  box(30, 70, 22, 30, CYAN, p || (e && ir == 4));
  box(36, 46, 22, 14, CYAN, p);
  box(60, 70, 34, 44, ORANGE, f);
  box(110, 50, 44, 64, MAGENTA, e && ir <= 3);
  box(96, 128, 36, 22, BLU, e && imm);
  box(158, 84, 10, 28, WHITE, e && ir != 3);
  alu2(e);
  box(190, 118, 30, 20, YELLOW, e && ir == 3);
  box(36, 172, 168, 26, ORANGE, e);

  ln(52, 85, 60, 85, f);
  ln(70, 114, 70, 172, f);
  ln(41, 70, 41, 60, p);
  ln(36, 53, 28, 53, p);
  ln(28, 53, 28, 80, p);
  ln(28, 80, 30, 80, p);
  ln(94, 80, 110, 80, e && ir <= 2);
  ln(88, 114, 88, 138, e && imm);
  ln(88, 138, 96, 138, e && imm);
  ln(154, 66, 176, 70, e && (ir == 2 || ir == 3));
  ln(154, 92, 158, 92, e && ir == 2);
  ln(132, 134, 160, 134, e && imm);
  ln(160, 134, 160, 112, e && imm);
  ln(168, 98, 176, 98, e && ir != 3);
  ln(202, 87, 206, 87, e && ir <= 3);
  ln(206, 87, 206, 42, e && ir <= 2);
  ln(206, 42, 132, 42, e && ir <= 2);
  ln(132, 42, 132, 50, e && ir <= 2);
  ln(206, 87, 206, 118, e && ir == 3);
  ln(114, 150, 114, 172, e && imm);
  ln(145, 114, 145, 172, e && ir <= 2);
  ln(166, 112, 166, 172, e && imm);
  ln(184, 110, 184, 172, e);

  t(33, 73, CYAN, "PC");
  snprintf(buf, sizeof(buf), "%02d", pc);
  t(34, 86, WHITE, buf);
  t(39, 49, CYAN, "+4");
  t(63, 73, ORANGE, "IMEM");
  if (ir == 2 || ir == 3) snprintf(buf, sizeof(buf), "%-5s", nm[ir]);
  else snprintf(buf, sizeof(buf), "%s %-2d", nm[ir], ia);
  t(63, 90, WHITE, buf);
  t(113, 53, MAGENTA, "Reg[]");
  snprintf(buf, sizeof(buf), "A:%3d", A);
  t(113, 68, WHITE, buf);
  snprintf(buf, sizeof(buf), "B:%3d", B);
  t(113, 80, WHITE, buf);
  t(180, 54, GREEN, "ALU");
  t(99, 131, BLU, "IMM");
  t(99, 140, BLU, "GEN");
  t(193, 120, YELLOW, "OUT");
  snprintf(buf, sizeof(buf), "%3d", outv);
  t(196, 129, WHITE, buf);

  uint16_t vc = e ? WHITE : LIGHTGREY;
  t(40, 181, ORANGE, "CONTROL");
  t(105, 175, ORANGE, "IMM");
  t(139, 175, ORANGE, "WE");
  t(163, 175, ORANGE, "B");
  t(175, 175, ORANGE, "ALU");
  t(111, 187, vc, ir <= 1 ? "I" : (ir == 4 ? "J" : "-"));
  t(142, 187, vc, ir <= 2 ? "1" : "0");
  t(163, 187, vc, imm ? "1" : "0");
  t(175, 187, vc, ir == 3 ? "PASS" : "ADD ");

  status2();
}

void pol(float deg, int r, int &x, int &y) {
  float a = deg * 0.0174533f;
  x = FX + lroundf(r * cosf(a));
  y = FY + lroundf(r * sinf(a));
}

void arcFsm(int i, bool on) {
  uint16_t c = on ? fc[i] : DIM;
  int x0, y0, x1 = 0, y1 = 0;
  pol(th[i] + 26, FR, x0, y0);
  for (int s = 1; s <= 12; s++) {
    pol(th[i] + 26 + 68.0f * s / 12, FR, x1, y1);
    g->drawLine(x0, y0, x1, y1, c);
    g->drawLine(x0 + 1, y0, x1 + 1, y1, c);
    x0 = x1; y0 = y1;
  }
  float a = (th[i] + 94) * 0.0174533f;
  float tx = -sinf(a), ty = cosf(a), rx = cosf(a), ry = sinf(a);
  g->fillTriangle(x1 + tx * 4, y1 + ty * 4,
                  x1 - tx * 4 + rx * 4, y1 - ty * 4 + ry * 4,
                  x1 - tx * 4 - rx * 4, y1 - ty * 4 - ry * 4, c);
}

void nodeFsm(int i, bool on) {
  int x, y;
  pol(th[i], FR, x, y);
  g->fillCircle(x, y, 22, on ? fc[i] : BLACK);
  g->drawCircle(x, y, 22, on ? fc[i] : DIM);
  g->drawCircle(x, y, 21, on ? fc[i] : DIM);
  g->setTextSize(1);
  g->setTextColor(on ? BLACK : LIGHTGREY, on ? fc[i] : BLACK);
  g->setCursor(x - strlen(fn[i]) * 3, y - 3);
  g->print(fn[i]);
}

void drawFsm() {
  if (dx >= 0) { g->fillCircle(dx, dy, 4, BLACK); dx = -1; }
  for (int i = 0; i < 3; i++) arcFsm(i, i == ph);
  for (int i = 0; i < 3; i++) nodeFsm(i, i == ph);
  g->fillRect(86, 80, 68, 48, BLACK);
  ct(120, 86, WHITE, nm[ir], 2);
  snprintf(buf, sizeof(buf), "PC %02d", pc);
  ct(120, 108, CYAN, buf, 1);
  snprintf(buf, sizeof(buf), "A:%d B:%d", A, B);
  ct(120, 118, MAGENTA, buf, 1);
}

void dotFsm() {
  if (dx >= 0) g->fillCircle(dx, dy, 4, BLACK);
  arcFsm(ph, true);
  pol(th[ph] + 26 + 68.0f * (tk + 1) / 5, FR, dx, dy);
  g->fillCircle(dx, dy, 3, WHITE);
}

void drawWave() {
  g->fillRect(32, 62, 184, 112, BLACK);
  ct(120, 46, LIGHTGREY, "WAVEFORM", 1);
  const char *rn[7] = {"CLK", "FET", "EXE", "PC+1", "PC", "A", "OUT"};
  const uint16_t rc[7] = {GREEN, GREEN, MAGENTA, CYAN, CYAN, MAGENTA, YELLOW};
  for (int r = 0; r < 7; r++) {
    int by = 66 + r * 16, pv = -1;
    uint16_t c = rc[r];
    t(34, by + 2, LIGHTGREY, rn[r]);
    for (int i = 0; i < HN; i++) {
      int v = r == 0 ? hC[i] : r == 1 ? (hP[i] == 0) : r == 2 ? (hP[i] == 1) : r == 3 ? (hP[i] == 2) : r == 4 ? hPc[i] : r == 5 ? hA[i] : hO[i];
      int x = 62 + i * 4;
      if (r < 4) {
        int y = v ? by : by + 10;
        g->drawFastHLine(x, y, 5, c);
        if (pv >= 0 && v != pv) g->drawFastVLine(x, by, 11, c);
      } else {
        g->drawFastHLine(x, by, 5, c);
        g->drawFastHLine(x, by + 10, 5, c);
        if (pv < 0 || v != pv) {
          if (pv >= 0) {
            g->drawLine(x, by, x + 3, by + 10, c);
            g->drawLine(x, by + 10, x + 3, by, c);
          }
          snprintf(buf, sizeof(buf), "%d", v);
          t(x + 5, by + 2, WHITE, buf);
        }
      }
      pv = v;
    }
  }
}

void pipeStep() {
  for (int i = 4; i > 0; i--) pipeS[i] = pipeS[i - 1];
  pipeS[0] = fpc;
  fpc = (fpc + 1) % N;
  fl = false;
  if (pipeS[2] >= 0 && op[pipeS[2]] == 4) {
    pipeS[0] = -1;
    pipeS[1] = -1;
    fpc = arg[pipeS[2]] % N;
    fl = true;
    flushN += 2;
  }
  cyc++;
}

void drawPipe() {
  g->fillRect(20, 66, 200, 112, BLACK);
  ct(120, 44, LIGHTGREY, "PIPELINE", 1);
  for (int s = 0; s < 5; s++) {
    int x = 22 + s * 40;
    box(x, 70, 36, 18, ORANGE, true);
    ct(x + 18, 75, ORANGE, sn[s], 1);
    int p = pipeS[s];
    if (p >= 0) {
      g->fillRoundRect(x, 94, 36, 26, 3, ic[op[p]]);
      g->setTextSize(1);
      g->setTextColor(BLACK, ic[op[p]]);
      g->setCursor(x + 9, 103);
      g->print(nm[op[p]]);
    } else {
      g->drawRoundRect(x, 94, 36, 26, 3, DIM);
      ct(x + 18, 103, DIM, "--", 1);
    }
  }
  snprintf(buf, sizeof(buf), "CYCLE %d", cyc);
  ct(120, 130, WHITE, buf, 1);
  if (fl) ct(120, 144, MAGENTA, "JMP: FLUSH IF+ID", 1);
  else ct(120, 144, LIGHTGREY, "in-order, no stall", 1);
  snprintf(buf, sizeof(buf), "bubbles: %d", flushN);
  ct(120, 158, LIGHTGREY, buf, 1);
}

void drawDbg() {
  g->fillRect(22, 48, 196, 124, BLACK);
  t(30, 52, LIGHTGREY, "MEM");
  t(122, 52, LIGHTGREY, "REG");
  for (int i = 0; i < N; i++) {
    int y = 64 + i * 12;
    bool a = (i == pc);
    if (a) g->fillRect(26, y - 2, 82, 11, ORANGE);
    g->setTextSize(1);
    g->setTextColor(a ? BLACK : WHITE, a ? ORANGE : BLACK);
    g->setCursor(30, y);
    snprintf(buf, sizeof(buf), "%d:%02X %s", i, ((op[i] + 1) << 4) | arg[i], nm[op[i]]);
    g->print(buf);
  }
  const char *rn[4] = {"PC", "A ", "B ", "O "};
  int rv[4] = {pc, A, B, outv};
  bool w[4] = {ph == 2 || (ph == 1 && ir == 4), ph == 1 && (ir == 0 || ir == 2), ph == 1 && ir == 1, ph == 1 && ir == 3};
  for (int k = 0; k < 4; k++) {
    int y = 64 + k * 14;
    if (w[k]) g->fillRect(114, y - 2, 102, 12, YELLOW);
    g->setTextSize(1);
    g->setTextColor(w[k] ? BLACK : WHITE, w[k] ? YELLOW : BLACK);
    g->setCursor(118, y);
    snprintf(buf, sizeof(buf), "%s=%02X  %3d", rn[k], rv[k], rv[k]);
    g->print(buf);
  }
  g->drawFastHLine(30, 134, 180, DIM);
  for (int k = 0; k < 3; k++) {
    snprintf(b2, sizeof(b2), "%-20s", lg[k]);
    t(32, 140 + k * 12, k == 2 ? WHITE : LIGHTGREY, b2);
  }
}

void arcR(int r, int f255, uint16_t c) {
  if (f255 > 255) f255 = 255;
  int lim = 270 * f255 / 255;
  for (int d = 0; d <= 270; d += 3) {
    float a = (135 + d) * 0.0174533f;
    g->fillCircle(120 + lroundf(r * cosf(a)), 120 + lroundf(r * sinf(a)), 2, d <= lim ? c : DIM);
  }
}

void drawRad() {
  g->fillRect(60, 70, 120, 96, BLACK);
  ct(120, 80, LIGHTGREY, "OUT", 1);
  snprintf(buf, sizeof(buf), "%d", outv);
  ct(120, 96, YELLOW, buf, 4);
  snprintf(buf, sizeof(buf), "A:%d", A);
  t(73, 148, MAGENTA, buf);
  snprintf(buf, sizeof(buf), "B:%d", B);
  t(113, 148, BLU, buf);
  snprintf(buf, sizeof(buf), "PC:%d", pc);
  t(143, 148, CYAN, buf);
  arcR(100, A, MAGENTA);
  arcR(91, B * 17, BLU);
  arcR(82, pc * 64, CYAN);
}

void static1() {
  g->setTextSize(1);
  g->setTextColor(LIGHTGREY);
  g->setCursor(100, 14); g->print("CLOCK");
  g->setCursor(125, 46); g->print("CPU");
  g->setCursor(49, 45);  g->print("ADDR");
  g->setCursor(50, 165); g->print("DATA");
  g->drawRoundRect(78, 58, 92, 112, 4, CYAN);
  g->setTextColor(ORANGE);
  g->setCursor(36, 77);  g->print("RAM");
  g->setCursor(88, 93);  g->print("CONTROL UNIT");
  g->setTextColor(CYAN);
  g->setCursor(85, 65);  g->print("PC");
  g->setTextColor(GREEN);
  g->setCursor(85, 119); g->print("ALU");
  g->setTextColor(BLU);
  g->setCursor(125, 119); g->print("IR");
  g->setTextColor(YELLOW);
  g->setCursor(190, 95);  g->print("OUT");
}

void static2() {
  g->setTextSize(1);
  g->setTextColor(LIGHTGREY);
  g->setCursor(105, 8);
  g->print("CLOCK");
}

void render() {
  switch (page) {
    case 0: ram(); regs(); show(ph); status(sT, sC, sSub); ctrl(cS); break;
    case 1: draw2(); break;
    case 2: drawFsm(); status(sT, sC, sSub); break;
    case 3: drawWave(); status(sT, sC, sSub); break;
    case 4: drawPipe(); break;
    case 5: drawDbg(); status(sT, sC, sSub); break;
    case 6: status(sT, sC, sSub); drawRad(); break;
  }
  ring();
}

void initPage() {
  g->fillScreen(BLACK);
  dx = -1;
  if (page == 0) static1();
  else if (page == 1) static2();
  render();
  wave();
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Tiny CPU</title>
<style>
:root{--bg:#f1efe9;--panel:#fff;--fg:#1b2230;--mute:#667085;--line:#d9d5ca;--acc:#0a8f6a;--glow:rgba(10,143,106,.25);--bez:#252b36;
box-sizing:border-box;padding-top:env(safe-area-inset-top,0px);padding-bottom:env(safe-area-inset-bottom,0px)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#0b1220;--panel:#121a2b;--fg:#e6ecf5;--mute:#8a97ad;--line:#243049;--acc:#2de2a6;--glow:rgba(45,226,166,.3);--bez:#05070c}}
:root[data-theme="dark"]{--bg:#0b1220;--panel:#121a2b;--fg:#e6ecf5;--mute:#8a97ad;--line:#243049;--acc:#2de2a6;--glow:rgba(45,226,166,.3);--bez:#05070c}
*{box-sizing:border-box}html{scroll-padding-top:env(safe-area-inset-top,0px)}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 "Space Grotesk",system-ui,sans-serif}
header{padding:28px 24px 8px;max-width:1100px;margin:auto}
h1{margin:0;font-size:clamp(26px,5vw,44px);letter-spacing:-.02em}
header p{margin:6px 0 0;color:var(--mute);max-width:60ch}
main{max-width:1100px;margin:auto;padding:16px 24px 40px;display:grid;gap:24px;grid-template-columns:minmax(300px,500px) 1fr;align-items:start}
@media(max-width:820px){main{grid-template-columns:1fr}}
.dev{display:flex;flex-direction:column;align-items:center;gap:16px}
.bezel{padding:18px;border-radius:50%;background:var(--bez);box-shadow:0 0 0 2px var(--line),0 0 60px var(--glow);width:min(100%,520px)}
#lcd{display:block;width:100%;aspect-ratio:1;border-radius:50%;cursor:pointer;background:#000}
.tabs{display:flex;flex-wrap:wrap;gap:6px;justify-content:center}
button,select,input{font:inherit;color:var(--fg)}
.tabs button,.ctl button{background:var(--panel);border:1px solid var(--line);border-radius:999px;padding:6px 14px;cursor:pointer}
.tabs button[aria-pressed=true],.ctl button.pri{background:var(--acc);color:#04130d;border-color:var(--acc);font-weight:600}
button:focus-visible,select:focus-visible,input:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
#boot{width:64px;height:64px;border-radius:50%;border:3px solid var(--line);background:var(--panel);font-weight:700;cursor:pointer;box-shadow:0 5px 0 var(--line)}
#boot:active{transform:translateY(4px);box-shadow:0 1px 0 var(--line)}
.side{display:grid;gap:16px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px}
.card h2{margin:0 0 10px;font-size:16px}
.ctl{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.ctl label{display:flex;gap:8px;align-items:center;color:var(--mute)}
.regs{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;text-align:center}
.regs div{background:var(--bg);border-radius:10px;padding:8px 4px}
.regs b{display:block;font:600 22px ui-monospace,monospace;color:var(--acc)}
.regs span{color:var(--mute);font-size:12px}
.prog{display:grid;gap:6px}
.row{display:grid;grid-template-columns:28px 1fr 70px;gap:8px;align-items:center;padding:4px 6px;border-radius:8px;border:1px solid transparent}
.row.on{border-color:var(--acc);background:var(--glow)}
.row select,.row input{background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:5px 8px;width:100%}
#log{font:13px ui-monospace,monospace;color:var(--mute);min-height:120px;margin:0;padding:0;list-style:none}
#log li:last-child{color:var(--fg)}
</style></head><body>
<header><h1>Tiny CPU, live in your browser</h1>
<p>A browser twin of the ESP32-C3 + GC9A01 display. Edit the program, press BOOT or click the screen to flip pages.</p></header>
<main>
<section class="dev">
 <div class="bezel"><canvas id="lcd" width="480" height="480" aria-label="Simulated round display"></canvas></div>
 <div class="tabs" id="tabs" role="group" aria-label="Pages"></div>
 <button id="boot" aria-label="BOOT button, next page">BOOT</button>
</section>
<section class="side">
 <div class="card"><h2>Controls</h2><div class="ctl">
  <button id="play" class="pri">Pause</button><button id="step">Step</button><button id="reset">Reset</button><span id="cs" style="color:var(--mute)"></span>
  <label>Speed <input id="spd" type="range" min="0.25" max="4" step="0.25" value="1"></label></div></div>
 <div class="card"><h2>Registers</h2><div class="regs">
  <div><b id="rPC">0</b><span>PC</span></div><div><b id="rA">0</b><span>A</span></div>
  <div><b id="rB">0</b><span>B</span></div><div><b id="rO">0</b><span>OUT</span></div></div></div>
 <div class="card"><h2>Program (RAM)</h2><div class="prog" id="prog"></div></div>
 <div class="card"><h2>Execution log</h2><ul id="log"></ul></div>
</section>
</main>
<script>
const $=id=>document.getElementById(id),N=5,PAGES=7,HN=36;
const X=$('lcd').getContext('2d');X.scale(2,2);
const BLACK=0x0043,WHITE=0xFFFF,GREEN=0x07F9,CYAN=0x065F,MAGENTA=0xF974,YELLOW=0xFD80,ORANGE=0xA35F,LGREY=0x8D18,DIM=0x2189,BLU=0x3BDF;
const rgb=v=>`rgb(${(v>>11&31)*255/31|0},${(v>>5&63)*255/63|0},${(v&31)*255/31|0})`;
const fr=(a,b,w,h,c)=>{X.fillStyle=rgb(c);X.fillRect(a,b,w,h)};
const hl=(a,b,w,c)=>fr(a,b,w,1,c),vl=(a,b,h,c)=>fr(a,b,1,h,c);
const L=(a,b,c2,d,c)=>{X.strokeStyle=rgb(c);X.lineWidth=1;X.beginPath();X.moveTo(a+.5,b+.5);X.lineTo(c2+.5,d+.5);X.stroke()};
const ci=(a,b,r,c)=>{X.strokeStyle=rgb(c);X.beginPath();X.arc(a+.5,b+.5,r,0,7);X.stroke()};
const fci=(a,b,r,c)=>{X.fillStyle=rgb(c);X.beginPath();X.arc(a+.5,b+.5,r,0,7);X.fill()};
const rr=(a,b,w,h,r,c)=>{X.strokeStyle=rgb(c);X.beginPath();X.roundRect(a+.5,b+.5,w-1,h-1,r);X.stroke()};
const frr=(a,b,w,h,r,c)=>{X.fillStyle=rgb(c);X.beginPath();X.roundRect(a,b,w,h,r);X.fill()};
const tri=(a,b,c,d,e,f,col)=>{X.fillStyle=rgb(col);X.beginPath();X.moveTo(a,b);X.lineTo(c,d);X.lineTo(e,f);X.fill()};
const T=(a,b,c,s,sz=1,bg=BLACK)=>{s=String(s);if(bg!=null)fr(a,b,s.length*6*sz,8*sz,bg);X.fillStyle=rgb(c);X.font=`${10*sz}px ui-monospace,Menlo,monospace`;X.textBaseline='top';X.fillText(s,a,b-.5*sz)};
const CT=(cx,y,c,s,sz=1,bg=BLACK)=>T(cx-String(s).length*3*sz,y,c,s,sz,bg);
const box=(a,b,w,h,c,on)=>{rr(a,b,w,h,3,on?c:DIM);rr(a+1,b+1,w-2,h-2,2,on?c:BLACK)};
const W=(a,b,c,d,on)=>L(a,b,c,d,on?GREEN:DIM);

const nm=['LDA','LDB','ADD','OUT','JMP'],micro=['A = RAM','B = RAM','A = A + B','OUT = A','PC = ADDR'];
const ic=[CYAN,BLU,GREEN,YELLOW,MAGENTA],sn=['IF','ID','EX','MEM','WB'];
let op=[0,1,2,3,4],ag=[5,3,0,0,2];
let pc,A,B,outv,ir,ia,clk,ph,tk,sub,page=0,sT='',sC=WHITE,cS='',sSub='',lg,hC,hP,hPc,hA,hO,pipeS,fpc,cyc,flushN,fl,dx,dy,run=true,spd=1,pcOld=0;
const fmt=i=>op[i]==2||op[i]==3?nm[op[i]]:nm[op[i]]+' '+ag[i];
function reset(){pc=A=B=outv=ir=ia=ph=tk=sub=cyc=flushN=fpc=0;clk=fl=false;dx=dy=-1;lg=[];
 hC=Array(HN).fill(0);hP=Array(HN).fill(0);hPc=Array(HN).fill(0);hA=Array(HN).fill(0);hO=Array(HN).fill(0);pipeS=[-1,-1,-1,-1,-1];
 sT='';sSub='';$('log').innerHTML='';initPage();ui()}

const FX=120,FY=112,FR=62,th=[-90,30,150],fc=[GREEN,MAGENTA,CYAN],fn=['FETCH','EXEC','PC+1'];
function ring(){const c=ph==0?GREEN:ph==1?MAGENTA:CYAN;ci(120,120,118,c);ci(120,120,117,c);ci(120,120,115,DIM);
 for(let i=0;i<PAGES;i++)fci(90+i*10,224,2,i==page?c:DIM)}
function lines(a,d){const ca=a?GREEN:DIM,cd=d?GREEN:DIM;vl(47,54,20,ca);hl(47,54,54,ca);vl(100,54,8,ca);vl(47,160,16,cd);hl(47,176,98,cd);vl(144,170,7,cd)}
function poly(p,c){for(let i=0;i<p.length;i++){const a=p[i],b=p[(i+1)%p.length];L(a[0],a[1],b[0],b[1],c)}}
function alu(on){fr(84,130,32,34,BLACK);poly([[87,140],[95,140],[100,148],[105,140],[113,140],[108,156],[92,156]],on?GREEN:DIM)}
function alu2(on){fr(174,62,32,50,BLACK);poly([[176,64],[176,80],[184,87],[176,94],[176,110],[202,98],[202,76]],on?GREEN:DIM)}
function wave(){if(page==0){fr(80,26,82,20,BLACK);for(let i=0;i<4;i++){const x=80+i*20;hl(x,((i%2==0)^clk)?28:42,20,GREEN);vl(x+20,28,15,GREEN)}}
 else if(page==1){fr(88,18,66,14,BLACK);for(let i=0;i<4;i++){const x=90+i*15;hl(x,((i%2==0)^clk)?20:30,15,GREEN);vl(x+15,20,11,GREEN)}}}
function status(){fr(30,180,180,32,BLACK);T(120-sT.length*6,182,sC,sT,2,null);T(120-sSub.length*3,202,LGREY,sSub,1,null)}
function status2(){fr(36,202,168,10,BLACK);const s=sT+': '+sSub;T(120-s.length*3,203,sC,s,1,null)}
function ctrl(){fr(84,102,80,9,BLACK);T(124-cS.length*3,103,ORANGE,cS,1,null)}
function ram(){for(let i=0;i<N;i++)T(24,90+i*13,i==pc?WHITE:ORANGE,(i+' '+fmt(i)).padEnd(9))}
function regs(){T(98,69,CYAN,String(pc).padStart(2,'0'),2);T(131,66,MAGENTA,'A:'+String(A).padStart(3));T(131,76,MAGENTA,'B:'+String(B).padStart(3));
 T(126,130,WHITE,nm[ir]);T(126,142,YELLOW,(ir+1).toString(2).padStart(4,'0').slice(-4));T(126,153,CYAN,(ia&15).toString(2).padStart(4,'0'));
 T(183,108,YELLOW,String(outv).padStart(3),2)}
function show(p){const f=p==0,e=p==1,pl=p==2;box(20,74,54,86,ORANGE,f);box(82,62,44,24,CYAN,pl);box(128,62,38,24,MAGENTA,e&&ir<=2);box(82,90,84,22,ORANGE,e);
 box(82,116,36,50,GREEN,e&&ir==2);alu(e&&ir==2);box(122,116,44,50,BLU,f);box(176,92,50,36,YELLOW,e&&ir==3);lines(f||pl,f)}
function static1(){const g=(x,y,c,s)=>T(x,y,c,s,1,null);g(100,14,LGREY,'CLOCK');g(125,46,LGREY,'CPU');g(49,45,LGREY,'ADDR');g(50,165,LGREY,'DATA');
 rr(78,58,92,112,4,CYAN);g(36,77,ORANGE,'RAM');g(88,93,ORANGE,'CONTROL UNIT');g(85,65,CYAN,'PC');g(85,119,GREEN,'ALU');g(125,119,BLU,'IR');g(190,95,YELLOW,'OUT')}
function draw2(){const f=ph==0,e=ph==1,p=ph==2,imm=ir<=1||ir==4;
 box(30,70,22,30,CYAN,p||(e&&ir==4));box(36,46,22,14,CYAN,p);box(60,70,34,44,ORANGE,f);box(110,50,44,64,MAGENTA,e&&ir<=3);
 box(96,128,36,22,BLU,e&&imm);box(158,84,10,28,WHITE,e&&ir!=3);alu2(e);box(190,118,30,20,YELLOW,e&&ir==3);box(36,172,168,26,ORANGE,e);
 [[52,85,60,85,f],[70,114,70,172,f],[41,70,41,60,p],[36,53,28,53,p],[28,53,28,80,p],[28,80,30,80,p],[94,80,110,80,e&&ir<=2],[88,114,88,138,e&&imm],[88,138,96,138,e&&imm],
 [154,66,176,70,e&&(ir==2||ir==3)],[154,92,158,92,e&&ir==2],[132,134,160,134,e&&imm],[160,134,160,112,e&&imm],[168,98,176,98,e&&ir!=3],[202,87,206,87,e&&ir<=3],
 [206,87,206,42,e&&ir<=2],[206,42,132,42,e&&ir<=2],[132,42,132,50,e&&ir<=2],[206,87,206,118,e&&ir==3],[114,150,114,172,e&&imm],[145,114,145,172,e&&ir<=2],
 [166,112,166,172,e&&imm],[184,110,184,172,e]].forEach(a=>W(...a));
 T(33,73,CYAN,'PC');T(34,86,WHITE,String(pc).padStart(2,'0'));T(39,49,CYAN,'+4');T(63,73,ORANGE,'IMEM');T(63,90,WHITE,fmt(pcOld).padEnd(5));
 T(113,53,MAGENTA,'Reg[]');T(113,68,WHITE,'A:'+String(A).padStart(3));T(113,80,WHITE,'B:'+String(B).padStart(3));T(180,54,GREEN,'ALU');
 T(99,131,BLU,'IMM');T(99,140,BLU,'GEN');T(193,120,YELLOW,'OUT');T(196,129,WHITE,String(outv).padStart(3));
 const vc=e?WHITE:LGREY;T(40,181,ORANGE,'CONTROL');[[105,'IMM'],[139,'WE'],[163,'B'],[175,'ALU']].forEach(a=>T(a[0],175,ORANGE,a[1]));
 T(111,187,vc,ir<=1?'I':ir==4?'J':'-');T(142,187,vc,ir<=2?'1':'0');T(163,187,vc,imm?'1':'0');T(175,187,vc,ir==3?'PASS':'ADD ');status2()}
const pol=(d,r)=>[FX+Math.round(r*Math.cos(d*Math.PI/180)),FY+Math.round(r*Math.sin(d*Math.PI/180))];
function arcFsm(i,on){const c=on?fc[i]:DIM;let [x0,y0]=pol(th[i]+26,FR),x1=0,y1=0;
 for(let s=1;s<=12;s++){[x1,y1]=pol(th[i]+26+68*s/12,FR);L(x0,y0,x1,y1,c);L(x0+1,y0,x1+1,y1,c);x0=x1;y0=y1}
 const a=(th[i]+94)*Math.PI/180,tx=-Math.sin(a),ty=Math.cos(a),rx=Math.cos(a),ry=Math.sin(a);
 tri(x1+tx*4,y1+ty*4,x1-tx*4+rx*4,y1-ty*4+ry*4,x1-tx*4-rx*4,y1-ty*4-ry*4,c)}
function nodeFsm(i,on){const [x,y]=pol(th[i],FR);fci(x,y,22,on?fc[i]:BLACK);ci(x,y,22,on?fc[i]:DIM);ci(x,y,21,on?fc[i]:DIM);
 T(x-fn[i].length*3,y-3,on?BLACK:LGREY,fn[i],1,on?fc[i]:BLACK)}
function drawFsm(){if(dx>=0){fci(dx,dy,4,BLACK);dx=-1}for(let i=0;i<3;i++)arcFsm(i,i==ph);for(let i=0;i<3;i++)nodeFsm(i,i==ph);
 fr(86,80,68,48,BLACK);CT(120,86,WHITE,nm[ir],2);CT(120,108,CYAN,'PC '+String(pc).padStart(2,'0'));CT(120,118,MAGENTA,`A:${A} B:${B}`)}
function dotFsm(){if(dx>=0)fci(dx,dy,4,BLACK);arcFsm(ph,true);[dx,dy]=pol(th[ph]+26+68*(tk+1)/5,FR);fci(dx,dy,3,WHITE)}
function drawWave(){fr(32,62,184,112,BLACK);CT(120,46,LGREY,'WAVEFORM');
 const rn=['CLK','FET','EXE','PC+1','PC','A','OUT'],rc=[GREEN,GREEN,MAGENTA,CYAN,CYAN,MAGENTA,YELLOW];
 for(let r=0;r<7;r++){const by=66+r*16,c=rc[r];let pv=-1;T(34,by+2,LGREY,rn[r]);
  for(let i=0;i<HN;i++){const v=[hC[i],hP[i]==0?1:0,hP[i]==1?1:0,hP[i]==2?1:0,hPc[i],hA[i],hO[i]][r],x=62+i*4;
   if(r<4){const y=v?by:by+10;hl(x,y,5,c);if(pv>=0&&v!=pv)vl(x,by,11,c)}
   else{hl(x,by,5,c);hl(x,by+10,5,c);if(pv<0||v!=pv){if(pv>=0){L(x,by,x+3,by+10,c);L(x,by+10,x+3,by,c)}T(x+5,by+2,WHITE,v)}}
   pv=v}}}
function pipeStep(){for(let i=4;i>0;i--)pipeS[i]=pipeS[i-1];pipeS[0]=fpc;fpc=(fpc+1)%N;fl=false;
 const e=pipeS[2];if(e>=0&&op[e]==4){pipeS[0]=pipeS[1]=-1;fpc=ag[e]%N;fl=true;flushN+=2}cyc++}
function drawPipe(){fr(20,66,200,112,BLACK);CT(120,44,LGREY,'PIPELINE');
 for(let s=0;s<5;s++){const x=22+s*40;box(x,70,36,18,ORANGE,true);CT(x+18,75,ORANGE,sn[s]);const p=pipeS[s];
  if(p>=0){frr(x,94,36,26,3,ic[op[p]]);T(x+9,103,BLACK,nm[op[p]],1,ic[op[p]])}else{rr(x,94,36,26,3,DIM);CT(x+18,103,DIM,'--')}}
 CT(120,130,WHITE,'CYCLE '+cyc);fl?CT(120,144,MAGENTA,'JMP: FLUSH IF+ID'):CT(120,144,LGREY,'in-order, no stall');CT(120,158,LGREY,'bubbles: '+flushN)}
function drawDbg(){fr(22,48,196,124,BLACK);T(30,52,LGREY,'MEM');T(122,52,LGREY,'REG');
 for(let i=0;i<N;i++){const y=64+i*12,a=i==pc;if(a)fr(26,y-2,82,11,ORANGE);
  T(30,y,a?BLACK:WHITE,`${i}:${(((op[i]+1)<<4)|(ag[i]&15)).toString(16).toUpperCase().padStart(2,'0')} ${nm[op[i]]}`,1,a?ORANGE:BLACK)}
 const rn=['PC','A ','B ','O '],rv=[pc,A,B,outv],w=[ph==2||(ph==1&&ir==4),ph==1&&(ir==0||ir==2),ph==1&&ir==1,ph==1&&ir==3];
 for(let k=0;k<4;k++){const y=64+k*14;if(w[k])fr(114,y-2,102,12,YELLOW);
  T(118,y,w[k]?BLACK:WHITE,`${rn[k]}=${rv[k].toString(16).toUpperCase().padStart(2,'0')}  ${String(rv[k]).padStart(3)}`,1,w[k]?YELLOW:BLACK)}
 hl(30,134,180,DIM);for(let k=0;k<3;k++)T(32,140+k*12,k==2?WHITE:LGREY,(lg[k]||'').padEnd(20))}
function arcR(r,f,c){const lim=270*Math.min(f,255)/255;for(let d=0;d<=270;d+=3){const a=(135+d)*Math.PI/180;fci(120+Math.round(r*Math.cos(a)),120+Math.round(r*Math.sin(a)),2,d<=lim?c:DIM)}}
function drawRad(){fr(60,70,120,96,BLACK);CT(120,80,LGREY,'OUT');CT(120,96,YELLOW,outv,4);T(73,148,MAGENTA,'A:'+A);T(113,148,BLU,'B:'+B);T(143,148,CYAN,'PC:'+pc);
 arcR(100,A,MAGENTA);arcR(91,B*17,BLU);arcR(82,pc*64,CYAN)}
function render(){switch(page){case 0:ram();regs();show(ph);status();ctrl();break;case 1:draw2();break;case 2:drawFsm();status();break;case 3:drawWave();status();break;
 case 4:drawPipe();break;case 5:drawDbg();status();break;case 6:status();drawRad()}ring()}
function initPage(){fr(0,0,240,240,BLACK);dx=-1;if(page==0)static1();else if(page==1)T(105,8,LGREY,'CLOCK',1,null);render();wave()}

function doPhase(p){
 if(p==0){pipeStep();pcOld=pc;ir=op[pc];ia=ag[pc];ph=0;sT='FETCH';sC=GREEN;cS='';sSub='RAM -> IR: '+fmt(pc)}
 else if(p==1){let jump=false;switch(ir){case 0:A=ia;break;case 1:B=ia;break;case 2:A=(A+B)&255;break;case 3:outv=A;break;case 4:pc=ia%N;jump=true}
  doPhase.j=jump;const v=[A,B,A,outv,pc][ir],t=[`A <- ${v}`,`B <- ${v}`,`A <- A+B=${v}`,`OUT <- ${v}`,`PC <- ${v}`][ir];
  lg.push(`${pcOld}: ${t}`);lg=lg.slice(-3);addLog(`${pcOld}: ${nm[ir]}  ${t}`);ph=1;sT='EXECUTE';sC=MAGENTA;cS=micro[ir];sSub=micro[ir]}
 else{if(!doPhase.j)pc=(pc+1)%N;ph=2;sT='PC + 1';sC=CYAN;cS='';sSub='next instruction'}
 render()}
function tick(){clk=!clk;hC.shift();hP.shift();hPc.shift();hA.shift();hO.shift();hC.push(+clk);hP.push(ph);hPc.push(pc);hA.push(A);hO.push(outv);
 if(page<2)wave();else if(page==2)dotFsm();else if(page==3)drawWave()}
function stepSub(){if(sub%4==0)doPhase(sub/4);tk=sub%4;tick();sub=(sub+1)%12;ui()}
let live=false;
function loop(){if(live){if(run){tk=(tk+1)%4;tick()}}else if(run)stepSub();setTimeout(loop,80/spd)}

function addLog(s){const l=$('log'),li=document.createElement('li');li.textContent=s;l.appendChild(li);while(l.children.length>7)l.firstChild.remove()}
function ui(){$('rPC').textContent=pc;$('rA').textContent=A;$('rB').textContent=B;$('rO').textContent=outv;
 [...$('prog').children].forEach((r,i)=>r.classList.toggle('on',i==pc))}
let last=-1,lastP=-1;
function setPage(p){page=p;initPage();if(live&&p!==lastP){lastP=p;send('a=page&v='+p)}[...$('tabs').children].forEach((b,i)=>b.setAttribute('aria-pressed',i==p))}
['Blocks','Datapath','FSM','Waveform','Pipeline','Debug','Radial'].forEach((n,i)=>{const b=document.createElement('button');b.textContent=n;b.onclick=()=>setPage(i);$('tabs').appendChild(b)});
for(let i=0;i<N;i++){const r=document.createElement('div');r.className='row';
 r.innerHTML=`<span>${i}</span><select>${nm.map((n,k)=>`<option value="${k}">${n}</option>`).join('')}</select><input type="number" min="0" max="15">`;
 const s=r.children[1],n=r.children[2];s.value=op[i];n.value=ag[i];
 const sp=()=>send(`a=prog&i=${i}&o=${op[i]}&g=${ag[i]}`);s.onchange=()=>{op[i]=+s.value;render();sp()};n.oninput=()=>{ag[i]=Math.max(0,Math.min(15,+n.value||0));render();sp()};$('prog').appendChild(r)}
$('boot').onclick=$('lcd').onclick=()=>setPage((page+1)%PAGES);
$('play').onclick=()=>{run=!run;$('play').textContent=run?'Pause':'Play';send('a=pause&v='+(run?0:1))};
$('step').onclick=()=>{run=false;$('play').textContent='Play';if(live)send('a=step');else stepSub()};
$('reset').onclick=()=>{send('a=reset');reset()};$('spd').oninput=e=>spd=+e.target.value;$('spd').onchange=e=>send('a=speed&v='+e.target.value);
addEventListener('keydown',e=>{if(e.target.tagName=='INPUT'||e.target.tagName=='SELECT')return;
 if(e.key==' '){e.preventDefault();$('play').click()}else if(e.key=='ArrowRight')setPage((page+1)%PAGES);else if(e.key=='ArrowLeft')setPage((page+PAGES-1)%PAGES)});
function applyLive(p,pcv,av,bv,ov,irv,iav){
 pc=pcv;A=av;B=bv;outv=ov;ir=irv;ia=iav;ph=p;
 if(p==0){pipeStep();pcOld=pc;sT='FETCH';sC=GREEN;cS='';sSub='RAM -> IR: '+fmt(pc)}
 else if(p==1){const v=[A,B,A,outv,pc][ir],t=[`A <- ${v}`,`B <- ${v}`,`A <- A+B=${v}`,`OUT <- ${v}`,`PC <- ${v}`][ir];
  lg.push(`${pcOld}: ${t}`);lg=lg.slice(-3);addLog(`${pcOld}: ${nm[ir]}  ${t}`);sT='EXECUTE';sC=MAGENTA;cS=micro[ir];sSub=micro[ir]}
 else{sT='PC + 1';sC=CYAN;cS='';sSub='next instruction'}
 render();ui()}
function parse(s){const a=s.split(',');
 if(a[0]=='R'){ag=a.slice(1,6).map(Number);op=a.slice(6,11).map(Number);if(!$('prog').contains(document.activeElement))[...$('prog').children].forEach((r,i)=>{r.children[1].value=op[i];r.children[2].value=ag[i]})}
 else if(a[0]=='S'&&a.length>=8)applyLive(...a.slice(1,8).map(Number));
 else if(a[0]=='P')setPage(+a[1]||0)}
const send=q=>{if(live)fetch('/c?'+q,{cache:'no-store'}).catch(()=>{})};
async function poll(){
 try{const t=(await(await fetch('/s',{cache:'no-store'})).text()).trim().split(',').map(Number);
  if(!live){reset();live=true;$('cs').textContent=' Live (WiFi)'}
  run=!t[9];$('play').textContent=run?'Pause':'Play';
  if(t[0]!==last){last=t[0];parse('R,'+t.slice(10,20).join(','));
   if(t[8]!==lastP){lastP=t[8];setPage(t[8])}
   applyLive(...t.slice(1,8))}
 }catch(e){live=false;last=-1;$('cs').textContent=' Offline, reconnecting'}
 setTimeout(poll,100)}
poll();
reset();setPage(0);loop();
</script></body></html>
)rawliteral";

// ---------- WiFi / web ----------
void handleRoot() {
  srv.sendHeader("Cache-Control", "no-store");
  srv.send_P(200, "text/html", PAGE);
}

// n,ph,pc,A,B,out,ir,ia,page,paused,a0..a4,o0..o4
void handleState() {
  char s[110];
  snprintf(s, sizeof(s), "%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
           (unsigned long)seqn, ph, pc, A, B, outv, ir, ia, page, paused ? 1 : 0,
           arg[0], arg[1], arg[2], arg[3], arg[4], op[0], op[1], op[2], op[3], op[4]);
  srv.sendHeader("Cache-Control", "no-store");
  srv.send(200, "text/plain", s);
}

// commands from the web UI: /c?a=page&v=3 | pause&v=1 | step | reset | speed&v=1.5 | prog&i=2&o=1&g=7
void handleCmd() {
  String a = srv.arg("a");
  if (a == "page") { page = constrain(srv.arg("v").toInt(), 0, PAGES - 1); initPage(); }
  else if (a == "pause") paused = srv.arg("v") == "1";
  else if (a == "step") { paused = true; stepReq = true; }
  else if (a == "reset") resetReq = true;
  else if (a == "speed") spd = constrain(srv.arg("v").toFloat(), 0.25f, 4.0f);
  else if (a == "prog") {
    int i = constrain(srv.arg("i").toInt(), 0, N - 1);
    op[i] = constrain(srv.arg("o").toInt(), 0, 4);
    arg[i] = constrain(srv.arg("g").toInt(), 0, 15);
  }
  srv.send(200, "text/plain", "ok");
}

void tx() { seqn++; }

void btn() {
  delay(30);
  if (digitalRead(BTN) != LOW) return;
  page = (page + 1) % PAGES;
  initPage();
  while (digitalRead(BTN) == LOW) { srv.handleClient(); delay(10); }
}

void tick() {
  clk = !clk;
  for (int i = 0; i < HN - 1; i++) {
    hC[i] = hC[i + 1]; hP[i] = hP[i + 1]; hPc[i] = hPc[i + 1];
    hA[i] = hA[i + 1]; hO[i] = hO[i + 1];
  }
  hC[HN - 1] = clk; hP[HN - 1] = ph; hPc[HN - 1] = pc;
  hA[HN - 1] = A; hO[HN - 1] = outv;
  if (page < 2) wave();
  else if (page == 2) dotFsm();
  else if (page == 3) drawWave();
}

void wait(int ms) {
  for (int k = 0; k < ms / 80; k++) {
    while (paused && !stepReq && !resetReq) {
      srv.handleClient();
      if (digitalRead(BTN) == LOW) btn();
      delay(5);
    }
    if (stepReq || resetReq) { stepReq = false; return; }
    tk = k;
    tick();
    int n = constrain((int)(8 / spd), 1, 32);
    for (int j = 0; j < n; j++) {
      delay(10);
      srv.handleClient();
      if (digitalRead(BTN) == LOW) btn();
      if (resetReq) return;
    }
  }
}

void doReset() {
  pc = A = B = outv = ir = ia = 0;
  ph = 0; cyc = 0; flushN = 0; fpc = 0; fl = false; clk = false;
  memset(hC, 0, HN); memset(hP, 0, HN); memset(hPc, 0, HN); memset(hA, 0, HN); memset(hO, 0, HN);
  memset(lg, 0, sizeof(lg));
  for (int i = 0; i < 5; i++) pipeS[i] = -1;
  resetReq = false;
  initPage();
  tx();
}

void setup() {
  pinMode(BTN, INPUT_PULLUP);
  g->begin(40000000);
  initPage();
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  delay(500);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // C3 Super Mini antenna fix
  srv.on("/", handleRoot);
  srv.on("/s", handleState);
  srv.on("/c", handleCmd);
  srv.on("/generate_204", []() { srv.send(204); });  // Android connectivity check
  srv.on("/hotspot-detect.html", []() { srv.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });  // iOS
  srv.onNotFound([]() { srv.send(404, "text/plain", "not found"); });
  srv.begin();
  g->fillScreen(BLACK);
  ct(120, 80, WHITE, ok ? "WiFi ready" : "WiFi FAILED", 2);
  ct(120, 115, CYAN, AP_SSID, 1);
  ct(120, 130, CYAN, AP_PASS, 1);
  ct(120, 145, YELLOW, "192.168.4.1", 1);
  delay(5000);
  initPage();
}

void loop() {
  if (resetReq) doReset();
  pipeStep();
  pcOld = pc;
  ir = op[pc];
  ia = arg[pc];
  ph = 0;
  sT = "FETCH"; sC = GREEN; cS = "";
  if (ir == 2 || ir == 3) snprintf(sSub, sizeof(sSub), "RAM -> IR: %s", nm[ir]);
  else snprintf(sSub, sizeof(sSub), "RAM -> IR: %s %d", nm[ir], ia);
  render();
  tx();
  wait(320);
  if (resetReq) return;

  bool jump = false;
  switch (ir) {
    case 0: A = ia; break;
    case 1: B = ia; break;
    case 2: A = A + B; break;
    case 3: outv = A; break;
    case 4: pc = ia % N; jump = true; break;
  }
  memcpy(lg[0], lg[1], 20);
  memcpy(lg[1], lg[2], 20);
  int val[5] = {A, B, A, outv, pc};
  snprintf(lg[2], 20, lf[ir], pcOld, val[ir]);
  ph = 1;
  sT = "EXECUTE"; sC = MAGENTA; cS = micro[ir];
  snprintf(sSub, sizeof(sSub), "%s", micro[ir]);
  render();
  tx();
  wait(320);
  if (resetReq) return;

  if (!jump) pc = (pc + 1) % N;
  ph = 2;
  sT = "PC + 1"; sC = CYAN; cS = "";
  snprintf(sSub, sizeof(sSub), "next instruction");
  render();
  tx();
  wait(320);
}
