// Flappy Bird + panneau de cheats - portage Nintendo 3DS (libctru + citro2d)
// Écran du haut : jeu (1920x1080 logiques -> 400x225)
// Écran du bas  : panneau de cheats (tactile / stylet)
#include <3ds.h>
#include <citro2d.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

// ---------------------------------------------------------------- constantes
static const float W = 1920, H = 1080;
static const float G = .38f, FL = -9.6f, PW = 100, GAP = 300, SP = 3.4f, SPACE = 560, GR = 110, R = 26, MAXV = 9.5f;
static const float S  = 400.0f / 1920.0f;   // échelle monde -> écran
static const float OY = (240.0f - 1080.0f * (400.0f / 1920.0f)) / 2.0f;

enum { C_AUTO, C_CP, C_INF, C_MAG, C_SLOW, C_FAST, NCHEAT };
static const char* CH_NAME[NCHEAT] = {"Saut automatique", "Checkpoint auto", "Pièces infinies", "Aimant à pièces", "Jeu ralenti", "Jeu accéléré"};
static bool cheat[NCHEAT];

enum { READY, PLAY, DEAD };
struct Bird  { float x, y, v; };
struct Pipe  { float x, gy; bool pass; };
struct CoinF { float x, y; bool got; };
struct Snap  { bool has = false; float y = 0; std::vector<Pipe> pipes; std::vector<CoinF> cn; };

static Bird bird;
static std::vector<Pipe> pipes;
static std::vector<CoinF> cn;
static Snap cp;
static int st = READY, score = 0, coins = 0, best = 0, cpScore = 0, deadT = 0, frame = 0, toastT = 0;
static bool hold = false;
static float acc = 0;
static int spdVal = 50;    // 20..100 (%)
static int fspdVal = 200;  // 120..500 (centièmes)
static std::string msg, msgS;

static float tscale() { return spdVal / 100.f; }
static float fscale() { return fspdVal / 100.f; }
static void say(const std::string& t) { msgS = t; }

// ---------------------------------------------------------------- sauvegarde du record
static const char* SAVE = "sdmc:/3ds/flappy_best.txt";
static void loadBest() { FILE* f = fopen(SAVE, "r"); if (f) { if (fscanf(f, "%d", &best) != 1) best = 0; fclose(f); } }
static void saveBest() { FILE* f = fopen(SAVE, "w"); if (f) { fprintf(f, "%d\n", best); fclose(f); } }

// ---------------------------------------------------------------- logique du jeu
static float frand() { return (float)rand() / (float)RAND_MAX; }

static void addPipe(float px) {
    float lo = GAP / 2 + 90, hi = H - GR - GAP / 2 - 90, gy = lo + frand() * (hi - lo);
    pipes.push_back({px, gy, false});
    cn.push_back({px + PW / 2, gy, false});
}
static void resetGame() {
    bird = {400, H / 2, 0}; pipes.clear(); cn.clear(); addPipe(1100); st = READY; hold = false;
}
static void flap() { bird.v = FL; }
static void die() {
    st = DEAD; deadT = 0;
    if (score > best) { best = score; saveBest(); }
}
static void saveCp() {
    if (st != PLAY) return;
    cpScore = score; cp.has = true; cp.y = bird.y; cp.pipes = pipes; cp.cn = cn; toastT = 90;
}
static void respawn() {
    if (!cp.has) { score = 0; cpScore = 0; resetGame(); return; }
    score = cpScore; bird = {400, cp.y, 0}; pipes = cp.pipes; cn = cp.cn;
    st = READY; hold = true; msg = "Checkpoint ! Appuie pour reprendre";
}
static void input() {
    if (st == READY) { st = PLAY; msg = ""; flap(); }
    else if (st == PLAY) flap();
    else if (st == DEAD && deadT > 15) { score = 0; cpScore = 0; cp = Snap(); msg = ""; resetGame(); }
}

static void update() {
    frame++;
    if (st == READY) {
        if (!hold) bird.y = H / 2 + sinf(frame / 10.f) * 14;
        if (cheat[C_AUTO]) { st = PLAY; msg = ""; flap(); }
        return;
    }
    if (st == DEAD) { deadT++; if (cheat[C_CP] && deadT > 40) respawn(); return; }

    if (cheat[C_AUTO]) {
        const Pipe* tp = nullptr;
        for (auto& p : pipes) if (p.x + PW > bird.x - R) { tp = &p; break; }
        float t = tp ? tp->gy + 34 : H / 2;
        if (bird.y > t && bird.v >= 0) flap();
    }
    bird.v = std::min(bird.v + G, MAXV); bird.y += bird.v;
    if (bird.y < R) { bird.y = R; bird.v = 0; }
    if (bird.y + R > H - GR) { die(); return; }

    for (auto& p : pipes) p.x -= SP;
    for (auto& c : cn) c.x -= SP;
    if (pipes.empty() || pipes.back().x < W + 20 - SPACE) addPipe(W + 20);
    pipes.erase(std::remove_if(pipes.begin(), pipes.end(), [](const Pipe& p) { return p.x <= -PW - 20; }), pipes.end());
    cn.erase(std::remove_if(cn.begin(), cn.end(), [](const CoinF& c) { return c.x <= -50 || c.got; }), cn.end());

    for (size_t i = 0; i < pipes.size(); i++) {
        Pipe& p = pipes[i];
        if (bird.x + R > p.x && bird.x - R < p.x + PW && (bird.y - R < p.gy - GAP / 2 || bird.y + R > p.gy + GAP / 2)) { die(); return; }
        if (!p.pass && p.x + PW < bird.x - R) { p.pass = true; score++; if (score % 5 == 0) saveCp(); }
    }
    for (auto& c : cn) {
        if (cheat[C_MAG]) {
            float dx = bird.x - c.x, dy = bird.y - c.y, d = hypotf(dx, dy);
            if (d < 300 && d > 1) { c.x += dx / d * 12; c.y += dy / d * 12; }
        }
        if (hypotf(bird.x - c.x, bird.y - c.y) < R + 18) { c.got = true; coins++; }
    }
}

// ---------------------------------------------------------------- rendu : helpers
static C3D_RenderTarget *topT, *botT;
static C2D_TextBuf tbuf;
static float gz = 0;
static inline float Z() { gz += 0.0001f; return gz; }
static inline u32 C(u32 hex, u8 a = 255) { return C2D_Color32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, a); }

static void R_(float x, float y, float w, float h, u32 c) { C2D_DrawRectSolid(x * S, y * S + OY, Z(), w * S, h * S, c); }
static void CIR(float cx, float cy, float r, u32 c) { C2D_DrawCircleSolid(cx * S, cy * S + OY, Z(), r * S, c); }
static void TRI(float x0, float y0, float x1, float y1, float x2, float y2, u32 c) {
    C2D_DrawTriangle(x0 * S, y0 * S + OY, c, x1 * S, y1 * S + OY, c, x2 * S, y2 * S + OY, c, Z());
}
static void BOX(float x, float y, float w, float h, u32 fill, u32 stroke) {
    R_(x - 2, y - 2, w + 4, h + 4, stroke); R_(x + 2, y + 2, w - 4, h - 4, fill);
}
static void TX(const std::string& s, float x, float y, float sc, u32 c, int al = 0, bool outline = false) {
    C2D_Text t; C2D_TextParse(&t, tbuf, s.c_str()); C2D_TextOptimize(&t);
    float w, h; C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    if (al == 1) x -= w / 2; else if (al == 2) x -= w;
    if (outline) {
        static const int OFF[8][2] = {{-1,0},{1,0},{0,-1},{0,1},{-1,-1},{1,1},{-1,1},{1,-1}};
        for (auto& o : OFF) C2D_DrawText(&t, C2D_WithColor, x + o[0], y + o[1], Z(), sc, sc, C(0x000000));
    }
    C2D_DrawText(&t, C2D_WithColor, x, y, Z(), sc, sc, c);
}

// ---------------------------------------------------------------- écran du haut
static void drawTop() {
    gz = 0;
    C2D_TargetClear(topT, C(0x000000));
    C2D_SceneBegin(topT);

    // ciel
    C2D_DrawRectangle(0, OY, Z(), 400, H * S, C(0x4fc3f7), C(0x4fc3f7), C(0xc5ecf8), C(0xc5ecf8));
    for (int i = 0; i < 6; i++) {
        float m = W + 300;
        float cx = fmodf(fmodf(i * 420.f - frame * .4f, m) + m, m) - 150, cy = 150.f + (i % 3) * 120;
        u32 w = C(0xffffff, 178);
        CIR(cx, cy, 50, w); CIR(cx + 60, cy + 10, 40, w); CIR(cx - 55, cy + 14, 36, w);
    }

    // tuyaux
    for (auto& p : pipes) {
        float top = p.gy - GAP / 2, bot = p.gy + GAP / 2;
        u32 f = C(0x4cae4f), s = C(0x2d6e30);
        BOX(p.x, -10, PW, top + 10, f, s);
        BOX(p.x, bot, PW, H - GR - bot, f, s);
        BOX(p.x - 8, top - 44, PW + 16, 44, f, s);
        BOX(p.x - 8, bot, PW + 16, 44, f, s);
    }
    // pièces
    for (auto& c : cn) { CIR(c.x, c.y, 20, C(0xb8860b)); CIR(c.x, c.y, 16, C(0xffc82c)); }

    // sol
    R_(0, H - GR, W, GR, C(0xd9c27a)); R_(0, H - GR, W, 20, C(0x6cbf3a));

    // anneau de l'aimant
    if (cheat[C_MAG] && st != DEAD)
        for (int i = 0; i < 40; i += 2) {
            float a = i * 2 * 3.14159265f / 40;
            R_(bird.x + cosf(a) * 300 - 7, bird.y + sinf(a) * 300 - 7, 14, 14, C(0xffc82c, 140));
        }

    // oiseau (tourne selon sa vitesse)
    {
        float a = std::max(-.5f, std::min(1.1f, bird.v / 10)), ca = cosf(a), sa = sinf(a);
        auto rp = [&](float px, float py, float& ox, float& oy) { ox = bird.x + px * ca - py * sa; oy = bird.y + px * sa + py * ca; };
        CIR(bird.x, bird.y, R + 2, C(0xb8860b)); CIR(bird.x, bird.y, R - 2, C(0xffd23f));
        float ex, ey; rp(10, -8, ex, ey); CIR(ex, ey, 8, C(0xffffff));
        rp(12, -8, ex, ey); CIR(ex, ey, 4, C(0x000000));
        float x0, y0, x1, y1, x2, y2;
        rp(R - 4, 0, x0, y0); rp(R + 16, 6, x1, y1); rp(R - 4, 12, x2, y2);
        TRI(x0, y0, x1, y1, x2, y2, C(0xff7043));
    }

    // HUD (coordonnées écran)
    TX(std::to_string(score), 200, -2, 1.0f, C(0xffffff), 1, true);
    TX(std::string("Pièces : ") + (cheat[C_INF] ? "inf" : std::to_string(coins)), 5, 2, 0.45f, C(0xffffff), 0, true);
    TX("Record " + std::to_string(best), 395, 2, 0.45f, C(0xffffff), 2, true);
    if (cheat[C_CP]) TX("Checkpoint : " + std::to_string(cpScore), 5, 20, 0.38f, C(0xffffff), 0, true);
    if (toastT > 0) { toastT--; TX("Checkpoint sauvegardé", 200, 48, 0.5f, C(0xffffff), 1, true); }
    if (st == READY) TX(msg.empty() ? "A / B pour voler" : msg, 200, 175, 0.55f, C(0xffffff), 1, true);
    if (st == DEAD) TX(cheat[C_CP] ? "Retour au checkpoint..." : "Perdu ! Appuie sur A pour rejouer", 200, 105, 0.55f, C(0xffffff), 1, true);
}

// ---------------------------------------------------------------- écran du bas (tactile)
struct Btn { int x, y, w, h; };
static Btn tBtn[NCHEAT], sl1, sl2, cpBtn, addBtn;
static int activeSlider = -1;

static void layoutButtons() {
    for (int i = 0; i < NCHEAT; i++) tBtn[i] = {4 + (i % 2) * 160, 22 + (i / 2) * 29, 152, 26};
    sl1 = {4, 125, 312, 16}; sl2 = {4, 161, 312, 16};
    cpBtn = {4, 184, 152, 26}; addBtn = {164, 184, 152, 26};
}
static bool inBtn(const Btn& b, int x, int y, int pad = 0) { return x >= b.x && x < b.x + b.w && y >= b.y - pad && y < b.y + b.h + pad; }

static void drawBtn(const Btn& b, const char* label, bool on, bool toggle) {
    u32 fill = toggle ? (on ? C(0x3ddbb0) : C(0x3a3470)) : C(0x4a4390);
    C2D_DrawRectSolid(b.x - 1, b.y - 1, Z(), b.w + 2, b.h + 2, C(0x4a4390));
    C2D_DrawRectSolid(b.x, b.y, Z(), b.w, b.h, fill);
    u32 tc = (toggle && on) ? C(0x1b1838) : C(0xf6efe6);
    if (toggle) { TX(label, b.x + 7, b.y + 5, 0.42f, tc); TX(on ? "ON" : "OFF", b.x + b.w - 6, b.y + 7, 0.33f, tc, 2); }
    else TX(label, b.x + b.w / 2, b.y + 5, 0.42f, tc, 1);
}
static void drawSlider(const Btn& b, float frac, bool on, const std::string& label) {
    TX(label, b.x, b.y - 15, 0.4f, C(0xf6efe6));
    C2D_DrawRectSolid(b.x, b.y, Z(), b.w, b.h, C(0x3a3470));
    C2D_DrawRectSolid(b.x, b.y, Z(), b.w * frac, b.h, on ? C(0x3ddbb0) : C(0x6a63b8));
    C2D_DrawRectSolid(b.x + b.w * frac - 3, b.y - 2, Z(), 6, b.h + 4, C(0xf6efe6));
}
static std::string fmtG(float v) { char b[16]; snprintf(b, sizeof(b), "%g", v); return b; }

static void drawBottom() {
    gz = 0;
    C2D_TargetClear(botT, C(0x1b1838));
    C2D_SceneBegin(botT);
    TX("Panneau de cheats", 6, 1, 0.55f, C(0xffc857));
    TX("X: checkpoint  Y: saut auto", 314, 7, 0.3f, C(0xa9a3d1), 2);
    for (int i = 0; i < NCHEAT; i++) drawBtn(tBtn[i], CH_NAME[i], cheat[i], true);
    drawSlider(sl1, (spdVal - 20) / 80.f, cheat[C_SLOW], "Vitesse ralenti : " + std::to_string(spdVal) + "%");
    drawSlider(sl2, (fspdVal - 120) / 380.f, cheat[C_FAST], "Vitesse accéléré : x" + fmtG(fscale()));
    drawBtn(cpBtn, "Checkpoint (X)", false, false);
    drawBtn(addBtn, "+100 pièces", false, false);
    TX(msgS, 6, 218, 0.4f, C(0x3ddbb0));
}

// ---------------------------------------------------------------- cheats
static void setSlow(bool on) { cheat[C_SLOW] = on; if (on) cheat[C_FAST] = false; }
static void setFast(bool on) { cheat[C_FAST] = on; if (on) cheat[C_SLOW] = false; }

static void toggleCheat(int i) {
    if (i == C_SLOW) { setSlow(!cheat[C_SLOW]); if (cheat[C_SLOW] && tscale() >= 1) spdVal = 50; }
    else if (i == C_FAST) setFast(!cheat[C_FAST]);
    else cheat[i] = !cheat[i];
    say(std::string(CH_NAME[i]) + (cheat[i] ? " activé" : " désactivé"));
}
static void doCheckpointKey() {
    if (st == PLAY) saveCp();
    else if ((st == DEAD || st == READY) && cp.has) respawn();
}

// ---------------------------------------------------------------- main
int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    topT = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    botT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    tbuf = C2D_TextBufNew(4096);
    srand((unsigned)osGetTime());
    layoutButtons();
    loadBest();
    resetGame();
    msgS = "Touche un bouton pour tricher.";

    while (aptMainLoop()) {
        hidScanInput();
        u32 kd = hidKeysDown(), kh = hidKeysHeld();
        if (kd & KEY_START) break;

        if (kd & (KEY_A | KEY_B | KEY_DUP | KEY_L | KEY_R)) input();
        if (kd & KEY_X) doCheckpointKey();
        if (kd & KEY_Y) toggleCheat(C_AUTO);

        if (kd & KEY_TOUCH) {
            touchPosition tp; hidTouchRead(&tp);
            for (int i = 0; i < NCHEAT; i++) if (inBtn(tBtn[i], tp.px, tp.py)) toggleCheat(i);
            if (inBtn(cpBtn, tp.px, tp.py)) { doCheckpointKey(); say("Checkpoint"); }
            if (inBtn(addBtn, tp.px, tp.py)) { coins += 100; say("+100 pièces"); }
            if (inBtn(sl1, tp.px, tp.py, 8)) activeSlider = 1;
            else if (inBtn(sl2, tp.px, tp.py, 8)) activeSlider = 2;
        }
        if (!(kh & KEY_TOUCH)) activeSlider = -1;
        if (activeSlider > 0 && (kh & KEY_TOUCH)) {
            touchPosition tp; hidTouchRead(&tp);
            float t = std::max(0.f, std::min(1.f, (tp.px - 4) / 312.f));
            if (activeSlider == 1) { spdVal = 20 + (int)lroundf(t * 80); setSlow(spdVal < 100); }
            else { fspdVal = 120 + (int)lroundf(t * 19) * 20; setFast(true); }
        }

        acc += cheat[C_SLOW] ? tscale() : cheat[C_FAST] ? fscale() : 1.f;
        while (acc >= 1.f) { update(); acc -= 1.f; }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(tbuf);
        drawTop();
        drawBottom();
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(tbuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
