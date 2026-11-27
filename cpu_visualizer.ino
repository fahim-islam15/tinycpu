#include <Arduino_GFX_Library.h>

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

const char *nm[]    = {"LDA", "LDB", "ADD", "OUT", "JMP"};
const char *micro[] = {"A = RAM", "B = RAM", "A = A + B", "OUT = A", "PC = ADDR"};
const char *lf[]    = {"%d: A <- %d", "%d: B <- %d", "%d: A <- A+B=%d", "%d: OUT <- %d", "%d: PC <- %d"};
const uint8_t arg[] = {5, 3, 0, 0, 2};
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
    if (i == 2 || i == 3) snprintf(buf, sizeof(buf), "%d %s  ", i, nm[i]);
    else snprintf(buf, sizeof(buf), "%d %s %d", i, nm[i], arg[i]);
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
  for (int b = 3; b >= 0; b--) g->print((arg[ir] >> b) & 1);
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
  else snprintf(buf, sizeof(buf), "%s %d", nm[ir], arg[ir]);
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
  if (pipeS[2] == 4) {
    pipeS[0] = -1;
    pipeS[1] = -1;
    fpc = arg[4];
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
      g->fillRoundRect(x, 94, 36, 26, 3, ic[p]);
      g->setTextSize(1);
      g->setTextColor(BLACK, ic[p]);
      g->setCursor(x + 9, 103);
      g->print(nm[p]);
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
    snprintf(buf, sizeof(buf), "%d:%02X %s", i, ((i + 1) << 4) | arg[i], nm[i]);
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

void btn() {
  delay(30);
  if (digitalRead(BTN) != LOW) return;
  page = (page + 1) % PAGES;
  initPage();
  while (digitalRead(BTN) == LOW) delay(10);
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
    tk = k;
    tick();
    for (int j = 0; j < 8; j++) {
      delay(10);
      if (digitalRead(BTN) == LOW) btn();
    }
  }
}

void setup() {
  pinMode(BTN, INPUT_PULLUP);
  g->begin(40000000);
  initPage();
}

void loop() {
  pipeStep();
  ir = pc;
  ph = 0;
  sT = "FETCH"; sC = GREEN; cS = "";
  if (ir == 2 || ir == 3) snprintf(sSub, sizeof(sSub), "RAM -> IR: %s", nm[ir]);
  else snprintf(sSub, sizeof(sSub), "RAM -> IR: %s %d", nm[ir], arg[ir]);
  render();
  wait(320);

  bool jump = false;
  switch (ir) {
    case 0: A = arg[ir]; break;
    case 1: B = arg[ir]; break;
    case 2: A = A + B; break;
    case 3: outv = A; break;
    case 4: pc = arg[ir]; jump = true; break;
  }
  memcpy(lg[0], lg[1], 20);
  memcpy(lg[1], lg[2], 20);
  int val[5] = {A, B, A, outv, pc};
  snprintf(lg[2], 20, lf[ir], ir, val[ir]);
  ph = 1;
  sT = "EXECUTE"; sC = MAGENTA; cS = micro[ir];
  snprintf(sSub, sizeof(sSub), "%s", micro[ir]);
  render();
  wait(320);

  if (!jump) pc = (pc + 1) % N;
  ph = 2;
  sT = "PC + 1"; sC = CYAN; cS = "";
  snprintf(sSub, sizeof(sSub), "next instruction");
  render();
  wait(320);
}
