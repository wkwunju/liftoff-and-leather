/*
 * Liftoff & Leather
 * A fan-made parody shmup. Cartoon pilots versus alien bugs and drones.
 * Original procedural art only: no photos, no company logos, no trademarks.
 *
 * Fan-made parody. Not affiliated with or endorsed by Elon Musk,
 * Jensen Huang, or their companies.
 */
#include "raylib.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define SCREEN_W 480
#define SCREEN_H 720

#define MAX_ENEMIES 28
#define MAX_PSHOTS 80
#define MAX_ESHOTS 140
#define MAX_PICKUPS 8
#define MAX_PARTICLES 240
#define MAX_POPUPS 16
#define MAX_STARS 100
#define MAX_NEBULA 4

enum { HERO_ELON = 0, HERO_JENSEN = 1 };
enum { MODE_ELON = 0, MODE_JENSEN = 1, MODE_COOP = 2 };
enum { EK_SKITTER = 0, EK_DIVER = 1, EK_POD = 2, EK_BOSS = 3 };
enum { SH_ROCKET = 0, SH_CHIP = 1, SH_BLOB = 2, SH_BOSS = 3 };
enum { PK_FAN = 0, PK_ZIP = 1, PK_HULL = 2 };
enum { ST_TITLE = 0, ST_PLAY = 1, ST_OVER = 2, ST_WIN = 3 };

static const Color COL_BG = {8, 10, 28, 255};
static const Color COL_INK = {236, 238, 246, 255};
static const Color COL_MUTED = {168, 176, 204, 255};
static const Color COL_GOLD = {255, 206, 84, 255};
static const Color COL_HAIR = {32, 30, 36, 255};
static const Color COL_SILVER = {214, 216, 222, 255};
static const Color COL_SKIN = {244, 198, 162, 255};
static const Color COL_SKIN2 = {214, 152, 118, 255};
static const Color COL_SHIRT = {18, 18, 22, 255};
static const Color COL_ROCKET = {236, 240, 246, 255};
static const Color COL_BAND = {255, 122, 48, 255};
static const Color COL_LASER = {64, 255, 156, 255};
static const Color COL_BUG = {132, 214, 96, 255};
static const Color COL_BUG_DARK = {36, 92, 48, 255};
static const Color COL_HULL = {120, 170, 214, 255};
static const Color COL_HULL_DARK = {28, 48, 72, 255};
static const Color COL_POD = {186, 120, 230, 255};
static const Color COL_POD_DARK = {78, 36, 110, 255};

typedef struct {
    bool inGame, alive, shield;
    int hero, lives;
    float x, y, cooldown, invuln, spread, rapid, flash;
} Pilot;

typedef struct {
    bool on;
    int kind, hp, maxHp, phase;
    float x, y, vx, vy, baseX, t, timer, timer2, r;
} Enemy;

typedef struct {
    bool on;
    int style, damage;
    float x, y, vx, vy, r;
} Shot;

typedef struct {
    bool on;
    int kind;
    float x, y, t, life;
} Pickup;

typedef struct {
    bool on;
    float x, y, vx, vy, life, maxLife, r;
    Color c;
} Particle;

typedef struct {
    bool on;
    float x, y, life;
    Color c;
    char text[24];
} Popup;

typedef struct {
    float x, y, v, r;
    unsigned char a;
} Star;

typedef struct {
    float x, y, r, v;
    Color c;
} Nebula;

typedef struct {
    int state, sel, mode, wave, spawned, quota, score, best;
    int countsBest, muted, touchMode, paused;
    int killsSinceDrop, sfxBudget;
    float t, waveDelay, spawnTimer, spawnInterval;
    float bannerT, shake, hurtFlash, winTimer, downTimer, controlGrace;
    char bannerA[48];
    char bannerB[64];
    Pilot pilots[2];
    Enemy enemies[MAX_ENEMIES];
    Shot pshots[MAX_PSHOTS];
    Shot eshots[MAX_ESHOTS];
    Pickup pickups[MAX_PICKUPS];
    Particle parts[MAX_PARTICLES];
    Popup popups[MAX_POPUPS];
    Star stars[MAX_STARS];
    Nebula nebulas[MAX_NEBULA];
} Game;

typedef struct {
    float ax, ay, bx, by;
    bool aShoot, bShoot, shootEdge;
    bool confirm, cancel, mute, pause, boss, restart;
    bool up, down, left, right;
    bool k1, k2, k3, k4;
    bool mouseEdge;
    Vector2 mouse;
    int touches;
    bool touchBegan;
    Vector2 touch[4];
} Input;

static Game g;
static double g_prevTime = 0.0;
static int g_primed = 0;
static Vector2 g_lastMouse;

static bool prevConfirm, prevCancel, prevMute, prevPause, prevBoss, prevRestart;
static bool prevUp, prevDown, prevLeft, prevRight;
static bool prevK1, prevK2, prevK3, prevK4, prevMouse, prevShoot;
static int prevTouchCount = 0;

static float Clampf(float v, float a, float b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

static float Len2(float x, float y) { return x * x + y * y; }

static void Norm(float *x, float *y) {
    float l = sqrtf((*x) * (*x) + (*y) * (*y));
    if (l > 0.0001f) {
        *x /= l;
        *y /= l;
    }
}

static int Edge(bool down, bool *prev) {
    int e = down && !*prev;
    *prev = down;
    return e;
}

#if defined(PLATFORM_WEB)
static void SfxUnlock(void) {
    EM_ASM({
        var AC = window.AudioContext || window.webkitAudioContext;
        if (!AC) return;
        if (!Module._actx) Module._actx = new AC();
        if (Module._actx.state === "suspended") Module._actx.resume();
    });
}

static void SfxTone(float freq, float dur, float vol, int wave) {
    if (g.muted || g.sfxBudget > 10) return;
    g.sfxBudget++;
    EM_ASM({
        var ctx = Module._actx;
        if (!ctx) return;
        var o = ctx.createOscillator();
        var gain = ctx.createGain();
        var wave = $3 | 0;
        o.type = wave === 1 ? "triangle" : wave === 2 ? "sawtooth" : wave === 3 ? "sine" : "square";
        o.frequency.setValueAtTime($0, ctx.currentTime);
        var t = ctx.currentTime;
        var v = Math.max(0.0002, $2);
        gain.gain.setValueAtTime(v, t);
        gain.gain.exponentialRampToValueAtTime(0.0002, t + Math.max(0.03, $1));
        o.connect(gain);
        gain.connect(ctx.destination);
        o.start(t);
        o.stop(t + $1 + 0.02);
    }, freq, dur, vol, wave);
}
#else
static void SfxUnlock(void) {}
static void SfxTone(float freq, float dur, float vol, int wave) {
    (void)freq; (void)dur; (void)vol; (void)wave;
}
#endif

static void AddParticle(float x, float y, float vx, float vy, float life, float r, Color c) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (g.parts[i].on) continue;
        g.parts[i].on = true;
        g.parts[i].x = x;
        g.parts[i].y = y;
        g.parts[i].vx = vx;
        g.parts[i].vy = vy;
        g.parts[i].life = life;
        g.parts[i].maxLife = life;
        g.parts[i].r = r;
        g.parts[i].c = c;
        return;
    }
}

static void Burst(float x, float y, int n, Color c, float speed) {
    for (int i = 0; i < n; i++) {
        float a = ((float)GetRandomValue(0, 628)) / 100.0f;
        float s = speed * (0.4f + (float)GetRandomValue(0, 100) / 100.0f);
        AddParticle(x, y, cosf(a) * s, sinf(a) * s, 0.25f + (float)GetRandomValue(0, 40) / 100.0f,
                    (float)GetRandomValue(2, 4), c);
    }
}

static void AddPopup(float x, float y, const char *text, Color c) {
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (g.popups[i].on) continue;
        g.popups[i].on = true;
        g.popups[i].x = x;
        g.popups[i].y = y;
        g.popups[i].life = 0.85f;
        g.popups[i].c = c;
        snprintf(g.popups[i].text, sizeof g.popups[i].text, "%s", text);
        return;
    }
}

static Shot *AllocShot(Shot *arr, int n) {
    for (int i = 0; i < n; i++) {
        if (!arr[i].on) return &arr[i];
    }
    return NULL;
}

static void SpawnPShot(float x, float y, float vx, float vy, float r, int dmg, int style) {
    Shot *s = AllocShot(g.pshots, MAX_PSHOTS);
    if (!s) return;
    s->on = true;
    s->style = style;
    s->damage = dmg;
    s->x = x;
    s->y = y;
    s->vx = vx;
    s->vy = vy;
    s->r = r;
}

static void SpawnEShot(float x, float y, float vx, float vy, float r, int style) {
    Shot *s = AllocShot(g.eshots, MAX_ESHOTS);
    if (!s) return;
    s->on = true;
    s->style = style;
    s->damage = 1;
    s->x = x;
    s->y = y;
    s->vx = vx;
    s->vy = vy;
    s->r = r;
}

static int AnyEnemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (g.enemies[i].on) return 1;
    return 0;
}

static int CountKind(int kind) {
    int n = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (g.enemies[i].on && g.enemies[i].kind == kind) n++;
    return n;
}

static Pilot *NearestPilot(float x, float y) {
    Pilot *best = NULL;
    float bestD = 1e12f;
    for (int i = 0; i < 2; i++) {
        if (!g.pilots[i].inGame || !g.pilots[i].alive) continue;
        float d = Len2(g.pilots[i].x - x, g.pilots[i].y - y);
        if (d < bestD) {
            bestD = d;
            best = &g.pilots[i];
        }
    }
    return best;
}

static int AnyPilotAlive(void) {
    for (int i = 0; i < 2; i++)
        if (g.pilots[i].inGame && g.pilots[i].alive) return 1;
    return 0;
}

static void RememberBest(void) {
    if (g.countsBest && g.score > g.best) g.best = g.score;
}

static Enemy *AllocEnemy(void) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (!g.enemies[i].on) return &g.enemies[i];
    return NULL;
}

static void SpawnPickup(float x, float y) {
    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (g.pickups[i].on) continue;
        g.pickups[i].on = true;
        g.pickups[i].kind = g.killsSinceDrop % 3;
        g.pickups[i].x = x;
        g.pickups[i].y = y;
        g.pickups[i].t = 0;
        g.pickups[i].life = 9.0f;
        g.killsSinceDrop = 0;
        return;
    }
}

static void KillEnemy(Enemy *e) {
    int points = 100;
    Color c = COL_BUG;
    if (e->kind == EK_DIVER) { points = 220; c = COL_HULL; }
    else if (e->kind == EK_POD) { points = 340; c = COL_POD; }
    else if (e->kind == EK_BOSS) { points = 5000; c = COL_GOLD; }

    char buf[24];
    snprintf(buf, sizeof buf, "+%d", points);
    AddPopup(e->x, e->y - 10, buf, COL_INK);
    g.score += points;
    Burst(e->x, e->y, e->kind == EK_BOSS ? 48 : 14, c, e->kind == EK_BOSS ? 180.f : 90.f);
    Burst(e->x, e->y, 8, COL_GOLD, 70.f);
    SfxTone(e->kind == EK_BOSS ? 196.f : 140.f, e->kind == EK_BOSS ? 0.28f : 0.12f, 0.06f, 2);

    if (e->kind == EK_BOSS) {
        g.winTimer = 1.7f;
        for (int i = 0; i < MAX_ENEMIES; i++) g.enemies[i].on = false;
        for (int i = 0; i < MAX_ESHOTS; i++) g.eshots[i].on = false;
        g.shake = 0.45f;
        return;
    }

    e->on = false;
    g.killsSinceDrop++;
    if (GetRandomValue(0, 99) < 24 || g.killsSinceDrop >= 5) SpawnPickup(e->x, e->y);
}

static void Hurt(Pilot *p) {
    if (!p->alive || p->invuln > 0.f) return;
    if (p->shield) {
        p->shield = false;
        p->invuln = 0.55f;
        Burst(p->x, p->y, 16, COL_GOLD, 120.f);
        SfxTone(520.f, 0.12f, 0.05f, 0);
        return;
    }
    p->lives--;
    g.shake = 0.32f;
    g.hurtFlash = 0.18f;
    Burst(p->x, p->y, 18, COL_GOLD, 110.f);
    SfxTone(96.f, 0.18f, 0.07f, 2);
    if (p->lives <= 0) {
        p->lives = 0;
        p->alive = false;
        Burst(p->x, p->y, 28, COL_INK, 160.f);
    } else {
        p->invuln = 1.65f;
    }
}

static void SpawnEnemy(int kind, int index) {
    Enemy *e = AllocEnemy();
    if (!e) return;
    memset(e, 0, sizeof *e);
    e->on = true;
    e->kind = kind;
    e->t = (float)GetRandomValue(0, 30) / 10.0f;
    e->baseX = 48.f + (float)((index * 83) % 384);
    e->x = e->baseX;
    e->y = -30.f;
    if (kind == EK_SKITTER) {
        e->hp = e->maxHp = 2;
        e->r = 14.f;
        e->vy = 76.f + (float)(g.wave * 4);
        e->timer = 1.2f + (float)GetRandomValue(0, 80) / 100.0f;
    } else if (kind == EK_DIVER) {
        e->hp = e->maxHp = 3;
        e->r = 14.f;
        e->x = (index % 2) ? 40.f : (float)(SCREEN_W - 40);
        e->baseX = e->x;
        e->phase = 0;
    } else if (kind == EK_POD) {
        e->hp = e->maxHp = 6;
        e->r = 16.f;
        e->timer = 1.0f;
        e->y = -40.f;
    } else {
        e->hp = e->maxHp = 300;
        e->r = 48.f;
        e->x = SCREEN_W / 2.0f;
        e->y = -80.f;
        e->phase = 0;
        e->timer = 0.6f;
        e->timer2 = 1.4f;
    }
}

static int KindFor(int wave, int index) {
    if (wave <= 1) return EK_SKITTER;
    if (wave == 2) return (index % 2 == 0) ? EK_DIVER : EK_SKITTER;
    if (wave == 3) {
        int m = index % 5;
        if (m == 0 || m == 1) return EK_POD;
        if (m == 2 || m == 3) return EK_DIVER;
        return EK_SKITTER;
    }
    return EK_BOSS;
}

static void BeginWave(void) {
    g.spawned = 0;
    g.spawnTimer = 0.15f;
    g.bannerT = 1.7f;
    g.waveDelay = 1.15f;
    if (g.wave <= 1) {
        g.quota = 8;
        g.spawnInterval = 0.78f;
        snprintf(g.bannerA, sizeof g.bannerA, "WAVE 1");
        snprintf(g.bannerB, sizeof g.bannerB, "Skitter patrol");
    } else if (g.wave == 2) {
        g.quota = 10;
        g.spawnInterval = 0.68f;
        snprintf(g.bannerA, sizeof g.bannerA, "WAVE 2");
        snprintf(g.bannerB, sizeof g.bannerB, "Dive drones");
    } else if (g.wave == 3) {
        g.quota = 12;
        g.spawnInterval = 0.6f;
        snprintf(g.bannerA, sizeof g.bannerA, "WAVE 3");
        snprintf(g.bannerB, sizeof g.bannerB, "Pod weather");
    } else {
        g.quota = 1;
        g.spawnInterval = 0.2f;
        g.waveDelay = 1.3f;
        snprintf(g.bannerA, sizeof g.bannerA, "QUEEN BEETLE");
        snprintf(g.bannerB, sizeof g.bannerB, "A very large bug would like a word");
        SfxTone(196.f, 0.16f, 0.05f, 3);
        SfxTone(262.f, 0.18f, 0.05f, 3);
    }
}

static void ClearField(void) {
    memset(g.pilots, 0, sizeof g.pilots);
    memset(g.enemies, 0, sizeof g.enemies);
    memset(g.pshots, 0, sizeof g.pshots);
    memset(g.eshots, 0, sizeof g.eshots);
    memset(g.pickups, 0, sizeof g.pickups);
    memset(g.parts, 0, sizeof g.parts);
    memset(g.popups, 0, sizeof g.popups);
    g.killsSinceDrop = 0;
    g.shake = 0;
    g.hurtFlash = 0;
    g.winTimer = 0;
    g.downTimer = 0;
    g.paused = 0;
    g.bannerT = 0;
}

static void StartRun(int mode, int practice) {
    ClearField();
    g.mode = mode;
    g.score = 0;
    g.countsBest = !practice;
    g.wave = practice ? 4 : 1;
    g.controlGrace = 0.35f;
    g.state = ST_PLAY;

    if (mode == MODE_COOP) {
        g.pilots[0].inGame = g.pilots[1].inGame = true;
        g.pilots[0].hero = HERO_ELON;
        g.pilots[1].hero = HERO_JENSEN;
        g.pilots[0].x = 170;
        g.pilots[1].x = 310;
    } else {
        g.pilots[0].inGame = true;
        g.pilots[0].hero = (mode == MODE_JENSEN) ? HERO_JENSEN : HERO_ELON;
        g.pilots[0].x = SCREEN_W / 2.0f;
    }
    for (int i = 0; i < 2; i++) {
        if (!g.pilots[i].inGame) continue;
        g.pilots[i].alive = true;
        g.pilots[i].lives = 3;
        g.pilots[i].y = 610;
        g.pilots[i].invuln = 1.8f;
    }
    BeginWave();
    SfxUnlock();
    SfxTone(523.f, 0.07f, 0.04f, 3);
    SfxTone(784.f, 0.1f, 0.04f, 3);
}

static void Fire(Pilot *p) {
    int n = (p->spread > 0.f) ? 3 : 1;
    float speed = (p->hero == HERO_ELON) ? 380.f : 700.f;
    int dmg = (p->hero == HERO_ELON) ? 3 : 1;
    float radius = (p->hero == HERO_ELON) ? 6.5f : 4.2f;
    int style = (p->hero == HERO_ELON) ? SH_ROCKET : SH_CHIP;
    for (int i = 0; i < n; i++) {
        float ang = (n == 1) ? 0.f : (-0.28f + (float)i * 0.28f);
        float side = (i == 1 || n == 1) ? (float)dmg : (float)(dmg > 1 ? dmg - 1 : 1);
        SpawnPShot(p->x + sinf(ang) * 10.f, p->y - 32.f, sinf(ang) * speed, -cosf(ang) * speed,
                   radius, (int)side, style);
    }
    p->flash = 0.045f;
    if (p->hero == HERO_ELON) SfxTone(210.f, 0.07f, 0.045f, 1);
    else SfxTone(740.f, 0.035f, 0.03f, 0);
    AddParticle(p->x, p->y - 34.f, 0, -20.f, 0.08f, p->hero == HERO_ELON ? 6.f : 3.f,
                p->hero == HERO_ELON ? COL_BAND : COL_LASER);
}

static void UpdateShots(Shot *arr, int n, float dt) {
    for (int i = 0; i < n; i++) {
        if (!arr[i].on) continue;
        arr[i].x += arr[i].vx * dt;
        arr[i].y += arr[i].vy * dt;
        if (arr[i].x < -40 || arr[i].x > SCREEN_W + 40 || arr[i].y < -50 || arr[i].y > SCREEN_H + 40)
            arr[i].on = false;
        if (arr[i].on && arr[i].style == SH_ROCKET && GetRandomValue(0, 2) == 0) {
            AddParticle(arr[i].x, arr[i].y + 6, 0, 30, 0.18f, 2.2f, COL_BAND);
        }
    }
}

static void UpdateEnemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g.enemies[i];
        if (!e->on) continue;
        e->t += dt;
        if (e->kind == EK_SKITTER) {
            e->x = e->baseX + sinf(e->t * 2.3f) * 36.f;
            e->y += e->vy * dt;
            e->timer -= dt;
            if (e->timer <= 0.f && e->y > 20.f && e->y < SCREEN_H - 160.f) {
                e->timer = 2.15f;
                SpawnEShot(e->x, e->y + 8, 0, 150.f, 5.f, SH_BLOB);
            }
        } else if (e->kind == EK_DIVER) {
            if (e->phase == 0) {
                e->y += 110.f * dt;
                if (e->y > 100.f) {
                    e->phase = 1;
                    e->timer = 0.42f;
                }
            } else if (e->phase == 1) {
                e->timer -= dt;
                if (e->timer <= 0.f) {
                    Pilot *p = NearestPilot(e->x, e->y);
                    float dx = p ? p->x - e->x : 0.f;
                    float dy = p ? p->y - e->y : 1.f;
                    Norm(&dx, &dy);
                    e->vx = dx * 255.f;
                    e->vy = dy * 255.f;
                    e->phase = 2;
                }
            } else {
                e->x += e->vx * dt;
                e->y += e->vy * dt;
            }
        } else if (e->kind == EK_POD) {
            e->x = e->baseX + sinf(e->t * 1.25f) * 46.f;
            if (e->y < 196.f) e->y += 52.f * dt;
            e->timer -= dt;
            if (e->timer <= 0.f && e->y > 30.f) {
                e->timer = 1.65f;
                for (int k = -1; k <= 1; k++) {
                    float a = (float)k * 0.5f;
                    SpawnEShot(e->x, e->y + 12, sinf(a) * 130.f, cosf(a) * 130.f, 5.f, SH_BLOB);
                }
            }
        } else {
            if (e->phase == 0) {
                e->y += 150.f * dt;
                if (e->y >= 188.f) {
                    e->y = 188.f;
                    e->phase = 1;
                }
            } else {
                int phase2 = e->hp < e->maxHp / 2;
                float span = phase2 ? 150.f : 130.f;
                float freq = phase2 ? 1.05f : 0.7f;
                e->x = SCREEN_W / 2.0f + sinf(e->t * freq) * span;
                e->y = 188.f + sinf(e->t * 1.35f) * 14.f;
                e->timer -= dt;
                e->timer2 -= dt;
                if (e->timer <= 0.f) {
                    e->timer = phase2 ? 0.42f : 0.64f;
                    Pilot *p = NearestPilot(e->x, e->y);
                    if (p) {
                        float dx = p->x - e->x;
                        float dy = p->y - e->y;
                        Norm(&dx, &dy);
                        float sp = phase2 ? 185.f : 160.f;
                        SpawnEShot(e->x, e->y + 20, dx * sp, dy * sp, 6.f, SH_BOSS);
                    }
                }
                if (e->timer2 <= 0.f) {
                    e->timer2 = phase2 ? 1.85f : 2.7f;
                    int n = phase2 ? 14 : 10;
                    float sp = 108.f;
                    for (int k = 0; k < n; k++) {
                        float a = e->t + (float)k * (2.f * PI / (float)n);
                        SpawnEShot(e->x, e->y, cosf(a) * sp, sinf(a) * sp, 5.2f, SH_BOSS);
                    }
                    if (phase2 && CountKind(EK_SKITTER) < 2) {
                        SpawnEnemy(EK_SKITTER, GetRandomValue(0, 5));
                    }
                }
            }
        }
        if (e->on && e->kind != EK_BOSS && (e->y > SCREEN_H + 50 || e->x < -80 || e->x > SCREEN_W + 80))
            e->on = false;
    }
}

static void Collide(void) {
    for (int s = 0; s < MAX_PSHOTS; s++) {
        Shot *b = &g.pshots[s];
        if (!b->on) continue;
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *e = &g.enemies[i];
            if (!e->on) continue;
            float rr = b->r + e->r;
            if (Len2(b->x - e->x, b->y - e->y) <= rr * rr) {
                e->hp -= b->damage;
                b->on = false;
                Burst(b->x, b->y, 5, b->style == SH_ROCKET ? COL_BAND : COL_LASER, 60.f);
                if (e->hp <= 0) KillEnemy(e);
                break;
            }
        }
    }

    for (int s = 0; s < MAX_ESHOTS; s++) {
        Shot *b = &g.eshots[s];
        if (!b->on) continue;
        for (int i = 0; i < 2; i++) {
            Pilot *p = &g.pilots[i];
            if (!p->inGame || !p->alive) continue;
            float rr = b->r + 8.f;
            if (Len2(b->x - p->x, b->y - p->y) <= rr * rr) {
                b->on = false;
                Hurt(p);
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g.enemies[i];
        if (!e->on) continue;
        for (int p = 0; p < 2; p++) {
            Pilot *pl = &g.pilots[p];
            if (!pl->inGame || !pl->alive) continue;
            float rr = e->r * 0.72f + 8.f;
            if (Len2(e->x - pl->x, e->y - pl->y) <= rr * rr) Hurt(pl);
        }
    }

    for (int i = 0; i < MAX_PICKUPS; i++) {
        Pickup *pk = &g.pickups[i];
        if (!pk->on) continue;
        for (int p = 0; p < 2; p++) {
            Pilot *pl = &g.pilots[p];
            if (!pl->inGame || !pl->alive) continue;
            if (Len2(pk->x - pl->x, pk->y - pl->y) <= 22.f * 22.f) {
                pk->on = false;
                if (pk->kind == PK_FAN) {
                    pl->spread = 8.5f;
                    AddPopup(pl->x, pl->y - 28, "FAN", COL_GOLD);
                } else if (pk->kind == PK_ZIP) {
                    pl->rapid = 8.5f;
                    AddPopup(pl->x, pl->y - 28, "ZIP", (Color){120, 230, 255, 255});
                } else {
                    pl->shield = true;
                    AddPopup(pl->x, pl->y - 28, "HULL", COL_GOLD);
                }
                SfxTone(660.f, 0.06f, 0.05f, 3);
                SfxTone(880.f, 0.09f, 0.05f, 3);
                Burst(pk->x, pk->y, 10, COL_GOLD, 80.f);
                break;
            }
        }
    }
}

static void MovePilot(Pilot *p, float ax, float ay, float dt) {
    float speed = (p->hero == HERO_ELON) ? 236.f : 280.f;
    float mag = sqrtf(ax * ax + ay * ay);
    if (mag > 1.f) {
        ax /= mag;
        ay /= mag;
    }
    p->x += ax * speed * dt;
    p->y += ay * speed * dt;
}

static void ClampPilot(Pilot *p) {
    p->x = Clampf(p->x, 18.f, SCREEN_W - 18.f);
    p->y = Clampf(p->y, 86.f, SCREEN_H - 64.f);
}

static Rectangle CardRect(int i) {
    return (Rectangle){28, 118.f + (float)i * 108.f, 424, 100};
}

static Rectangle PauseRect(void) {
    return (Rectangle){SCREEN_W - 48.f, 8, 38, 26};
}

static void UpdatePlay(float dt, const Input *in) {
    if (in->pause) g.paused = !g.paused;
    if (in->cancel) {
        g.state = ST_TITLE;
        SfxTone(300.f, 0.06f, 0.03f, 1);
        return;
    }
    if (in->restart) {
        StartRun(g.mode, !g.countsBest);
        return;
    }

    Rectangle pauseR = PauseRect();
    int follow = 0;
    Vector2 followAt = {0};
    if (g.touchMode || in->touches > 0) {
        if (in->touches > 0) g.touchMode = 1;
    }
    if (in->mouseEdge && CheckCollisionPointRec(in->mouse, pauseR)) {
        g.paused = !g.paused;
    }
    for (int i = 0; i < in->touches; i++) {
        if (CheckCollisionPointRec(in->touch[i], pauseR)) {
            if (in->touchBegan) g.paused = !g.paused;
        } else if (!follow) {
            follow = 1;
            followAt = in->touch[i];
        }
    }

    if (g.paused) return;

    if (g.controlGrace > 0.f) g.controlGrace -= dt;
    if (g.bannerT > 0.f) g.bannerT -= dt;
    if (g.shake > 0.f) g.shake = fmaxf(0.f, g.shake - dt);
    if (g.hurtFlash > 0.f) g.hurtFlash = fmaxf(0.f, g.hurtFlash - dt);

    if (g.winTimer > 0.f) {
        g.winTimer -= dt;
        for (int i = 0; i < 2; i++) {
            Pilot *p = &g.pilots[i];
            if (!p->inGame || !p->alive) continue;
            if (!(follow && i == 0 && g.controlGrace <= 0.f)) {
                float ax = (i == 0) ? in->ax : in->bx;
                float ay = (i == 0) ? in->ay : in->by;
                MovePilot(p, ax, ay, dt);
            }
            ClampPilot(p);
        }
        if (follow && g.pilots[0].alive && g.controlGrace <= 0.f) {
            Pilot *p = &g.pilots[0];
            float k = fminf(1.f, dt * 14.f);
            p->x += (followAt.x - p->x) * k;
            p->y += (followAt.y - p->y) * k;
            ClampPilot(p);
        }
        if (g.winTimer <= 0.f) {
            RememberBest();
            g.state = ST_WIN;
            SfxTone(523.f, 0.1f, 0.05f, 3);
            SfxTone(659.f, 0.1f, 0.05f, 3);
            SfxTone(784.f, 0.16f, 0.05f, 3);
        }
        return;
    }

    for (int i = 0; i < 2; i++) {
        Pilot *p = &g.pilots[i];
        if (!p->inGame || !p->alive) continue;
        if (p->spread > 0.f) p->spread -= dt;
        if (p->rapid > 0.f) p->rapid -= dt;
        if (p->invuln > 0.f) p->invuln -= dt;
        if (p->flash > 0.f) p->flash -= dt;
        p->cooldown -= dt;

        int useFollow = follow && i == 0 && g.controlGrace <= 0.f;
        if (useFollow) {
            float k = fminf(1.f, dt * 14.f);
            p->x += (followAt.x - p->x) * k;
            p->y += (followAt.y - p->y) * k;
        } else {
            float ax = (i == 0) ? in->ax : in->bx;
            float ay = (i == 0) ? in->ay : in->by;
            MovePilot(p, ax, ay, dt);
        }
        ClampPilot(p);

        int want = (i == 0) ? in->aShoot : in->bShoot;
        if (useFollow) want = 1;
        if (want && p->cooldown <= 0.f) {
            Fire(p);
            float rate = (p->hero == HERO_ELON) ? 0.28f : 0.09f;
            if (p->rapid > 0.f) rate *= 0.48f;
            p->cooldown = rate;
        }
    }

    if (g.waveDelay > 0.f) g.waveDelay -= dt;
    else if (g.spawned < g.quota) {
        g.spawnTimer -= dt;
        if (g.spawnTimer <= 0.f) {
            g.spawnTimer = g.spawnInterval;
            SpawnEnemy(KindFor(g.wave, g.spawned), g.spawned);
            g.spawned++;
        }
    }

    UpdateEnemies(dt);
    UpdateShots(g.pshots, MAX_PSHOTS, dt);
    UpdateShots(g.eshots, MAX_ESHOTS, dt);

    for (int i = 0; i < MAX_PICKUPS; i++) {
        if (!g.pickups[i].on) continue;
        g.pickups[i].t += dt;
        g.pickups[i].y += 68.f * dt;
        g.pickups[i].x += sinf(g.pickups[i].t * 3.f) * 18.f * dt;
        g.pickups[i].life -= dt;
        if (g.pickups[i].life <= 0.f || g.pickups[i].y > SCREEN_H - 10)
            g.pickups[i].on = false;
    }

    Collide();

    if (g.winTimer <= 0.f && !AnyPilotAlive()) {
        if (g.downTimer <= 0.f) g.downTimer = 1.05f;
        g.downTimer -= dt;
        if (g.downTimer <= 0.f) {
            RememberBest();
            g.state = ST_OVER;
            SfxTone(160.f, 0.2f, 0.05f, 2);
        }
    } else if (g.wave < 4 && g.waveDelay <= 0.f && g.spawned >= g.quota && !AnyEnemy()) {
        g.score += 500;
        AddPopup(SCREEN_W / 2.0f, 300, "+500", COL_GOLD);
        g.wave++;
        BeginWave();
        SfxTone(440.f, 0.08f, 0.04f, 3);
    }
}

static void UpdateBackdrop(float dt) {
    for (int i = 0; i < MAX_STARS; i++) {
        g.stars[i].y += g.stars[i].v * dt;
        if (g.stars[i].y > SCREEN_H) {
            g.stars[i].y = 0;
            g.stars[i].x = (float)GetRandomValue(0, SCREEN_W);
        }
    }
    for (int i = 0; i < MAX_NEBULA; i++) {
        g.nebulas[i].y += g.nebulas[i].v * dt;
        if (g.nebulas[i].y - g.nebulas[i].r > SCREEN_H) {
            g.nebulas[i].y = -g.nebulas[i].r;
            g.nebulas[i].x = (float)GetRandomValue(40, SCREEN_W - 40);
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g.parts[i].on) continue;
        g.parts[i].x += g.parts[i].vx * dt;
        g.parts[i].y += g.parts[i].vy * dt;
        g.parts[i].life -= dt;
        if (g.parts[i].life <= 0.f) g.parts[i].on = false;
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g.popups[i].on) continue;
        g.popups[i].y -= 28.f * dt;
        g.popups[i].life -= dt;
        if (g.popups[i].life <= 0.f) g.popups[i].on = false;
    }
}

static void DrawTextC(const char *text, float y, int size, Color c) {
    int w = MeasureText(text, size);
    DrawText(text, (SCREEN_W - w) / 2, (int)y, size, c);
}

static void DrawTextOutlined(const char *text, int x, int y, int size, Color c) {
    Color o = {6, 8, 18, 230};
    DrawText(text, x - 1, y, size, o);
    DrawText(text, x + 1, y, size, o);
    DrawText(text, x, y - 1, size, o);
    DrawText(text, x, y + 1, size, o);
    DrawText(text, x, y, size, c);
}

static void DrawTextCOutlined(const char *text, float y, int size, Color c) {
    int w = MeasureText(text, size);
    DrawTextOutlined(text, (SCREEN_W - w) / 2, (int)y, size, c);
}

static void DrawDisclaimer(void) {
    DrawRectangle(0, SCREEN_H - 44, SCREEN_W, 44, (Color){5, 7, 16, 235});
    Color c = {198, 204, 220, 255};
    DrawTextC("Fan-made parody.", (float)(SCREEN_H - 40), 10, c);
    DrawTextC("Not affiliated with or endorsed by", (float)(SCREEN_H - 28), 10, c);
    DrawTextC("Elon Musk, Jensen Huang, or their companies.", (float)(SCREEN_H - 16), 10, c);
}

static void DrawElon(float x, float y) {
    float flick = 0.62f + 0.38f * (0.5f + 0.5f * sinf(g.t * 28.f + x * 0.2f));
    DrawEllipse((int)x, (int)(y + 30), 12, 4, (Color){0, 0, 0, 50});
    DrawEllipse((int)x, (int)(y + 36), 7.f * flick, 11.f * flick, (Color){255, 138, 36, 230});
    DrawEllipse((int)x, (int)(y + 32), 3.4f * flick, 6.f * flick, (Color){255, 230, 160, 240});
    DrawLineEx((Vector2){x - 12, y + 4}, (Vector2){x - 24, y + 30}, 7, COL_BAND);
    DrawLineEx((Vector2){x + 12, y + 4}, (Vector2){x + 24, y + 30}, 7, COL_BAND);
    DrawPoly((Vector2){x, y - 24}, 3, 15, -90, COL_ROCKET);
    DrawRectangleRounded((Rectangle){x - 13, y - 10, 26, 42}, 0.38f, 6, COL_ROCKET);
    DrawRectangle((int)(x - 13), (int)(y + 12), 26, 5, COL_BAND);
    DrawRectangle((int)(x - 3), (int)(y - 2), 6, 20, (Color){190, 198, 210, 255});
    DrawCircleV((Vector2){x, y + 24}, 3.6f, (Color){150, 214, 230, 255});

    DrawCircleV((Vector2){x - 14, y + 4}, 5.2f, COL_SHIRT);
    DrawCircleV((Vector2){x + 14, y + 4}, 5.2f, COL_SHIRT);
    DrawRectangleRounded((Rectangle){x - 12, y - 4, 24, 16}, 0.32f, 4, COL_SHIRT);

    DrawCircleV((Vector2){x - 11, y - 12}, 3.1f, COL_SKIN2);
    DrawCircleV((Vector2){x + 11, y - 12}, 3.1f, COL_SKIN2);
    DrawCircleV((Vector2){x, y - 13}, 11.2f, COL_SKIN);
    DrawCircleV((Vector2){x, y - 19}, 11.6f, COL_HAIR);
    DrawCircleV((Vector2){x, y - 11}, 9.4f, COL_SKIN);
    DrawRectangle((int)(x - 12), (int)(y - 18), 4, 8, COL_HAIR);
    DrawRectangle((int)(x + 8), (int)(y - 18), 4, 8, COL_HAIR);

    DrawCircleV((Vector2){x - 3.7f, y - 13}, 1.55f, WHITE);
    DrawCircleV((Vector2){x + 3.7f, y - 13}, 1.55f, WHITE);
    DrawCircleV((Vector2){x - 3.4f, y - 12.6f}, 0.75f, COL_HAIR);
    DrawCircleV((Vector2){x + 4.0f, y - 12.6f}, 0.75f, COL_HAIR);
    DrawLineEx((Vector2){x - 6.2f, y - 17}, (Vector2){x - 1.4f, y - 17.4f}, 1.5f, COL_HAIR);
    DrawLineEx((Vector2){x + 1.4f, y - 17.4f}, (Vector2){x + 6.2f, y - 17}, 1.5f, COL_HAIR);
    DrawLineEx((Vector2){x - 4.2f, y - 8.2f}, (Vector2){x - 0.6f, y - 6.4f}, 1.5f, (Color){164, 78, 78, 255});
    DrawLineEx((Vector2){x - 0.6f, y - 6.4f}, (Vector2){x + 4.4f, y - 8.2f}, 1.5f, (Color){164, 78, 78, 255});
}

static void DrawJensen(float x, float y) {
    float pulse = 0.5f + 0.5f * sinf(g.t * 5.5f);
    DrawEllipse((int)x, (int)(y + 28), 16, 5, (Color){0, 0, 0, 45});
    DrawCircleV((Vector2){x, y + 30}, 11.f + pulse * 3.f, (Color){48, 255, 150, (unsigned char)(40 + 50 * pulse)});
    DrawRectangleRounded((Rectangle){x - 22, y + 16, 44, 9}, 0.55f, 4, (Color){20, 22, 28, 255});
    DrawRectangle((int)(x - 16), (int)(y + 23), 32, 3, (Color){48, 255, 150, 220});

    DrawCircleV((Vector2){x - 16, y + 2}, 6.2f, COL_SHIRT);
    DrawCircleV((Vector2){x + 16, y + 2}, 6.2f, COL_SHIRT);
    DrawRectangleRounded((Rectangle){x - 15, y - 8, 30, 24}, 0.3f, 5, COL_SHIRT);
    DrawLineEx((Vector2){x - 10, y - 4}, (Vector2){x + 8, y - 4}, 1.7f, (Color){78, 80, 92, 255});
    DrawLineEx((Vector2){x, y - 6}, (Vector2){x, y + 12}, 1.3f, (Color){90, 92, 104, 255});
    DrawLineEx((Vector2){x - 7, y - 6}, (Vector2){x - 1, y + 2}, 2.f, (Color){48, 50, 60, 255});
    DrawLineEx((Vector2){x + 7, y - 6}, (Vector2){x + 1, y + 2}, 2.f, (Color){48, 50, 60, 255});

    DrawRectangleRounded((Rectangle){x - 20, y - 30, 6, 16}, 0.4f, 3, (Color){24, 26, 32, 255});
    DrawRectangleRounded((Rectangle){x + 14, y - 30, 6, 16}, 0.4f, 3, (Color){24, 26, 32, 255});
    DrawRectangle((int)(x - 18), (int)(y - 30), 3, 4, COL_LASER);
    DrawRectangle((int)(x + 16), (int)(y - 30), 3, 4, COL_LASER);

    DrawCircleV((Vector2){x - 11, y - 13}, 3.f, COL_SKIN2);
    DrawCircleV((Vector2){x + 11, y - 13}, 3.f, COL_SKIN2);
    DrawCircleV((Vector2){x, y - 13}, 11.2f, COL_SKIN);
    DrawEllipse((int)x, (int)(y - 19), 14, 9, COL_SILVER);
    DrawCircleV((Vector2){x, y - 11.5f}, 9.3f, COL_SKIN);
    DrawRectangle((int)(x - 14), (int)(y - 18), 5, 7, COL_SILVER);
    DrawRectangle((int)(x + 9), (int)(y - 18), 5, 7, COL_SILVER);
    DrawLineEx((Vector2){x - 4, y - 22}, (Vector2){x + 5, y - 23}, 1.6f, (Color){248, 248, 252, 255});

    DrawCircleV((Vector2){x - 3.8f, y - 13.2f}, 1.45f, WHITE);
    DrawCircleV((Vector2){x + 3.8f, y - 13.2f}, 1.45f, WHITE);
    DrawCircleV((Vector2){x - 3.5f, y - 12.8f}, 0.72f, (Color){24, 24, 28, 255});
    DrawCircleV((Vector2){x + 4.1f, y - 12.8f}, 0.72f, (Color){24, 24, 28, 255});
    DrawRectangleRoundedLinesEx((Rectangle){x - 8.6f, y - 17.2f, 7.6f, 6.4f}, 0.45f, 4, 1.4f, (Color){52, 56, 68, 255});
    DrawRectangleRoundedLinesEx((Rectangle){x + 1.1f, y - 17.2f, 7.6f, 6.4f}, 0.45f, 4, 1.4f, (Color){52, 56, 68, 255});
    DrawLineEx((Vector2){x - 1.0f, y - 14.2f}, (Vector2){x + 1.1f, y - 14.2f}, 1.3f, (Color){52, 56, 68, 255});
    DrawLineEx((Vector2){x - 4.2f, y - 8.4f}, (Vector2){x, y - 6.6f}, 1.5f, (Color){164, 78, 78, 255});
    DrawLineEx((Vector2){x, y - 6.6f}, (Vector2){x + 4.4f, y - 8.4f}, 1.5f, (Color){164, 78, 78, 255});
}

static void DrawHero(int hero, float x, float y) {
    if (hero == HERO_JENSEN) DrawJensen(x, y);
    else DrawElon(x, y);
}

static void DrawHeroScaled(int hero, float x, float y, float sc) {
    if (fabsf(sc - 1.f) < 0.01f) {
        DrawHero(hero, x, y);
        return;
    }
    rlPushMatrix();
    rlTranslatef(x, y, 0);
    rlScalef(sc, sc, 1.f);
    DrawHero(hero, 0, 0);
    rlPopMatrix();
}

static void DrawSkitter(const Enemy *e) {
    float x = e->x, y = e->y;
    float wing = sinf(e->t * 16.f) * 4.f;
    DrawEllipse((int)(x - 14), (int)(y - 2 + wing), 11, 5, (Color){176, 236, 150, 150});
    DrawEllipse((int)(x + 14), (int)(y - 2 - wing), 11, 5, (Color){176, 236, 150, 150});
    DrawLineEx((Vector2){x - 4, y - 8}, (Vector2){x - 11, y - 20}, 1.6f, COL_BUG_DARK);
    DrawLineEx((Vector2){x + 4, y - 8}, (Vector2){x + 11, y - 20}, 1.6f, COL_BUG_DARK);
    DrawCircleV((Vector2){x - 11, y - 20}, 2.3f, COL_GOLD);
    DrawCircleV((Vector2){x + 11, y - 20}, 2.3f, COL_GOLD);
    for (int i = -1; i <= 1; i++) {
        float ly = y + (float)i * 5.f;
        DrawLineEx((Vector2){x - 8, ly}, (Vector2){x - 16, ly + 4}, 1.5f, COL_BUG_DARK);
        DrawLineEx((Vector2){x + 8, ly}, (Vector2){x + 16, ly + 4}, 1.5f, COL_BUG_DARK);
    }
    DrawEllipse((int)x, (int)y, 13, 10, COL_BUG_DARK);
    DrawEllipse((int)x, (int)y, 11, 8, COL_BUG);
    DrawCircleV((Vector2){x - 4, y - 2}, 3.1f, WHITE);
    DrawCircleV((Vector2){x + 4, y - 2}, 3.1f, WHITE);
    DrawCircleV((Vector2){x - 3.4f, y - 1.4f}, 1.45f, COL_BUG_DARK);
    DrawCircleV((Vector2){x + 4.6f, y - 1.4f}, 1.45f, COL_BUG_DARK);
}

static void DrawDiver(const Enemy *e) {
    float x = e->x, y = e->y;
    Color optic = COL_LASER;
    if (e->phase == 1 && ((int)(e->t * 18.f) & 1)) optic = (Color){255, 96, 84, 255};
    float flick = 0.55f + 0.45f * sinf(g.t * 32.f);
    DrawEllipse((int)x, (int)(y + 16), 4.2f, 6.f * flick, (Color){130, 220, 255, 210});
    DrawRectangleRounded((Rectangle){x - 24, y - 4, 10, 8}, 0.4f, 3, COL_HULL_DARK);
    DrawRectangleRounded((Rectangle){x + 14, y - 4, 10, 8}, 0.4f, 3, COL_HULL_DARK);
    DrawPoly((Vector2){x, y}, 6, 16, 30, COL_HULL_DARK);
    DrawPoly((Vector2){x, y}, 6, 13, 30, COL_HULL);
    DrawCircleV((Vector2){x, y}, 5.4f, (Color){10, 16, 28, 255});
    DrawCircleV((Vector2){x, y}, 3.3f, optic);
}

static void DrawPod(const Enemy *e) {
    float x = e->x, y = e->y;
    DrawCircleV((Vector2){x - 14, y - 6}, 7, COL_POD_DARK);
    DrawCircleV((Vector2){x + 14, y - 2}, 6, COL_POD_DARK);
    DrawCircleV((Vector2){x, y + 12}, 6, COL_POD_DARK);
    DrawCircleV((Vector2){x, y}, 16, COL_POD_DARK);
    DrawCircleV((Vector2){x, y}, 13.5f, COL_POD);
    DrawCircleV((Vector2){x - 5, y - 3}, 2.2f, (Color){230, 196, 255, 255});
    DrawCircleV((Vector2){x + 6, y + 3}, 2.5f, (Color){230, 196, 255, 255});
    DrawCircleV((Vector2){x - 4, y - 2}, 2.7f, WHITE);
    DrawCircleV((Vector2){x + 4, y - 2}, 2.7f, WHITE);
    DrawCircleV((Vector2){x - 4, y - 1.6f}, 1.25f, COL_POD_DARK);
    DrawCircleV((Vector2){x + 4, y - 1.6f}, 1.25f, COL_POD_DARK);
    DrawCircleV((Vector2){x, y + 6}, 2.4f, COL_POD_DARK);
}

static void DrawBoss(const Enemy *e) {
    float x = e->x, y = e->y;
    for (int i = 0; i < 3; i++) {
        float ly = y - 18.f + (float)i * 16.f;
        float kick = sinf(e->t * 7.f + (float)i) * 6.f;
        DrawLineEx((Vector2){x - 28, ly}, (Vector2){x - 52, ly + 8 + kick}, 3.2f, COL_BUG_DARK);
        DrawLineEx((Vector2){x - 52, ly + 8 + kick}, (Vector2){x - 64, ly + 16}, 3.2f, COL_BUG_DARK);
        DrawLineEx((Vector2){x + 28, ly}, (Vector2){x + 52, ly + 8 - kick}, 3.2f, COL_BUG_DARK);
        DrawLineEx((Vector2){x + 52, ly + 8 - kick}, (Vector2){x + 64, ly + 16}, 3.2f, COL_BUG_DARK);
    }
    DrawLineEx((Vector2){x - 14, y - 26}, (Vector2){x - 32, y - 58}, 2.6f, COL_BUG_DARK);
    DrawLineEx((Vector2){x + 14, y - 26}, (Vector2){x + 32, y - 58}, 2.6f, COL_BUG_DARK);
    DrawCircleV((Vector2){x - 32, y - 58}, 5, COL_GOLD);
    DrawCircleV((Vector2){x + 32, y - 58}, 5, COL_GOLD);
    DrawEllipse((int)x, (int)(y + 8), 60, 42, COL_BUG_DARK);
    DrawEllipse((int)x, (int)y, 56, 38, (Color){72, 176, 104, 255});
    DrawEllipse((int)x, (int)(y + 12), 26, 16, (Color){214, 216, 150, 255});
    DrawCircleV((Vector2){x - 22, y - 6}, 6.5f, COL_BUG_DARK);
    DrawCircleV((Vector2){x + 20, y - 2}, 5.5f, COL_BUG_DARK);
    DrawCircleV((Vector2){x + 2, y + 10}, 4.5f, COL_BUG_DARK);
    DrawCircleV((Vector2){x - 16, y - 10}, 10, WHITE);
    DrawCircleV((Vector2){x + 16, y - 10}, 10, WHITE);
    DrawCircleV((Vector2){x - 14, y - 8}, 4.8f, (Color){24, 48, 32, 255});
    DrawCircleV((Vector2){x + 18, y - 8}, 4.8f, (Color){24, 48, 32, 255});
    DrawCircleV((Vector2){x - 12.4f, y - 10}, 2.f, WHITE);
    DrawCircleV((Vector2){x + 19.6f, y - 10}, 2.f, WHITE);
    DrawLineEx((Vector2){x - 10, y + 8}, (Vector2){x - 2, y + 12}, 2.f, COL_BUG_DARK);
    DrawLineEx((Vector2){x - 2, y + 12}, (Vector2){x + 10, y + 8}, 2.f, COL_BUG_DARK);
}

static void DrawEnemy(const Enemy *e) {
    if (e->maxHp >= 4 && e->kind != EK_BOSS && e->hp < e->maxHp) {
        float w = e->r * 2.f;
        float f = Clampf((float)e->hp / (float)e->maxHp, 0.f, 1.f);
        DrawRectangle((int)(e->x - w / 2), (int)(e->y - e->r - 8), (int)w, 3, (Color){20, 24, 40, 255});
        DrawRectangle((int)(e->x - w / 2), (int)(e->y - e->r - 8), (int)(w * f), 3, COL_GOLD);
    }
    if (e->kind == EK_SKITTER) DrawSkitter(e);
    else if (e->kind == EK_DIVER) DrawDiver(e);
    else if (e->kind == EK_POD) DrawPod(e);
    else DrawBoss(e);
}

static void DrawShot(const Shot *s) {
    if (s->style == SH_ROCKET) {
        float a = atan2f(s->vy, s->vx) * RAD2DEG + 90.f;
        DrawPoly((Vector2){s->x, s->y - 2}, 3, 7, a, COL_BAND);
        DrawRectangleRounded((Rectangle){s->x - 3, s->y - 2, 6, 10}, 0.4f, 3, COL_ROCKET);
        DrawCircleV((Vector2){s->x, s->y + 7}, 2.4f, (Color){255, 210, 120, 255});
    } else if (s->style == SH_CHIP) {
        float len = 14.f;
        float mag = sqrtf(s->vx * s->vx + s->vy * s->vy);
        float nx = (mag > 1.f) ? s->vx / mag : 0.f;
        float ny = (mag > 1.f) ? s->vy / mag : -1.f;
        DrawLineEx((Vector2){s->x, s->y}, (Vector2){s->x - nx * len, s->y - ny * len}, 3.f,
                   (Color){80, 255, 170, 160});
        DrawRectangleRounded((Rectangle){s->x - 6, s->y - 4, 12, 8}, 0.25f, 3, (Color){36, 210, 120, 255});
        DrawRectangle((int)(s->x - 4), (int)(s->y - 2), 2, 2, (Color){8, 40, 24, 255});
        DrawRectangle((int)(s->x - 1), (int)(s->y - 2), 2, 2, (Color){8, 40, 24, 255});
        DrawRectangle((int)(s->x + 2), (int)(s->y - 2), 2, 2, (Color){8, 40, 24, 255});
        DrawRectangle((int)(s->x - 5), (int)(s->y - 6), 2, 2, (Color){210, 255, 220, 255});
        DrawRectangle((int)(s->x + 3), (int)(s->y - 6), 2, 2, (Color){210, 255, 220, 255});
    } else {
        Color c = (s->style == SH_BOSS) ? (Color){255, 186, 72, 255} : (Color){170, 255, 120, 255};
        DrawCircleV((Vector2){s->x, s->y}, s->r + 1.5f, (Color){c.r, c.g, c.b, 70});
        DrawCircleV((Vector2){s->x, s->y}, s->r, c);
        DrawCircleV((Vector2){s->x, s->y}, s->r * 0.4f, WHITE);
    }
}

static void DrawPickup(const Pickup *pk) {
    if (pk->life < 2.4f && ((int)(pk->life * 10.f) % 2 == 0)) return;
    Color c = COL_GOLD;
    const char *label = "F";
    if (pk->kind == PK_ZIP) {
        c = (Color){90, 220, 255, 255};
        label = "Z";
    } else if (pk->kind == PK_HULL) {
        c = COL_GOLD;
        label = "H";
    }
    float bob = sinf(pk->t * 4.f) * 2.f;
    DrawCircleV((Vector2){pk->x, pk->y + bob}, 13, (Color){8, 10, 22, 255});
    DrawCircleV((Vector2){pk->x, pk->y + bob}, 11, c);
    int w = MeasureText(label, 16);
    DrawText(label, (int)(pk->x - (float)w / 2), (int)(pk->y + bob - 7), 16, (Color){16, 18, 28, 255});
}

static void DrawHeart(float x, float y, float s, Color c) {
    DrawCircleV((Vector2){x - s * 0.34f, y}, s * 0.4f, c);
    DrawCircleV((Vector2){x + s * 0.34f, y}, s * 0.4f, c);
    Vector2 a = {x - s * 0.72f, y + s * 0.08f};
    Vector2 b = {x + s * 0.72f, y + s * 0.08f};
    Vector2 d = {x, y + s * 1.05f};
    /* Both windings: raylib only fills the counter-clockwise one. */
    DrawTriangle(a, b, d, c);
    DrawTriangle(a, d, b, c);
}

static void DrawBackdrop(void) {
    for (int i = 0; i < MAX_NEBULA; i++) {
        DrawCircleV((Vector2){g.nebulas[i].x, g.nebulas[i].y}, g.nebulas[i].r, g.nebulas[i].c);
    }
    for (int i = 0; i < MAX_STARS; i++) {
        Color c = {230, 236, 255, g.stars[i].a};
        DrawCircleV((Vector2){g.stars[i].x, g.stars[i].y}, g.stars[i].r, c);
    }
}

static void DrawWorld(void) {
    DrawBackdrop();
    for (int i = 0; i < MAX_PICKUPS; i++)
        if (g.pickups[i].on) DrawPickup(&g.pickups[i]);
    for (int i = 0; i < MAX_ESHOTS; i++)
        if (g.eshots[i].on) DrawShot(&g.eshots[i]);
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (g.enemies[i].on) DrawEnemy(&g.enemies[i]);
    for (int i = 0; i < MAX_PSHOTS; i++)
        if (g.pshots[i].on) DrawShot(&g.pshots[i]);
    for (int i = 0; i < 2; i++) {
        Pilot *p = &g.pilots[i];
        if (!p->inGame || !p->alive) continue;
        int blink = p->invuln > 0.f && ((int)(p->invuln * 16.f) % 2 == 0);
        if (!blink) DrawHero(p->hero, p->x, p->y);
        if (p->shield) DrawRing((Vector2){p->x, p->y}, 18, 21, 0, 360, 24, (Color){255, 214, 100, 210});
        if (p->flash > 0.f) {
            Color flash = (p->hero == HERO_ELON) ? COL_BAND : COL_LASER;
            DrawCircleV((Vector2){p->x, p->y - 34}, 5, flash);
        }
    }
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!g.parts[i].on) continue;
        Color c = g.parts[i].c;
        float a = Clampf(g.parts[i].life / g.parts[i].maxLife, 0.f, 1.f);
        c.a = (unsigned char)(255.f * a);
        DrawCircleV((Vector2){g.parts[i].x, g.parts[i].y}, g.parts[i].r, c);
    }
    for (int i = 0; i < MAX_POPUPS; i++) {
        if (!g.popups[i].on) continue;
        Color c = g.popups[i].c;
        c.a = (unsigned char)(255.f * Clampf(g.popups[i].life, 0.f, 1.f));
        int w = MeasureText(g.popups[i].text, 16);
        DrawText(g.popups[i].text, (int)(g.popups[i].x - (float)w / 2), (int)g.popups[i].y, 16, c);
    }
}

static void DrawLives(float x, float y, int lives, Color c) {
    for (int i = 0; i < lives && i < 5; i++) DrawHeart(x + (float)i * 16.f, y, 7.f, c);
}

static Enemy *LiveBoss(void) {
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (g.enemies[i].on && g.enemies[i].kind == EK_BOSS) return &g.enemies[i];
    return NULL;
}

static void DrawPowerTags(const Pilot *p, float x, float y) {
    float cursor = x;
    if (p->spread > 0.f) {
        char b[16];
        snprintf(b, sizeof b, "FAN %.0f", ceilf(p->spread));
        DrawText(b, (int)cursor, (int)y, 12, COL_GOLD);
        cursor += (float)MeasureText(b, 12) + 8;
    }
    if (p->rapid > 0.f) {
        char b[16];
        snprintf(b, sizeof b, "ZIP %.0f", ceilf(p->rapid));
        DrawText(b, (int)cursor, (int)y, 12, (Color){120, 230, 255, 255});
        cursor += (float)MeasureText(b, 12) + 8;
    }
    if (p->shield) DrawText("HULL", (int)cursor, (int)y, 12, COL_GOLD);
}

static void DrawHud(void) {
    char score[32];
    snprintf(score, sizeof score, "SCORE %06d", g.score);
    DrawTextOutlined(score, 12, 8, 18, COL_INK);

    const char *wave = "WAVE 1";
    if (g.wave == 2) wave = "WAVE 2";
    else if (g.wave == 3) wave = "WAVE 3";
    else if (g.wave >= 4) wave = g.countsBest ? "BOSS" : "PRACTICE";
    int ww = MeasureText(wave, 16);
    DrawTextOutlined(wave, SCREEN_W - ww - 54, 10, 16, COL_GOLD);

    DrawRectangleRounded(PauseRect(), 0.3f, 4, (Color){16, 20, 36, 220});
    DrawText("II", SCREEN_W - 36, 13, 16, COL_INK);

    if (g.mode == MODE_COOP) {
        DrawText("P1", 12, 30, 12, COL_BAND);
        DrawLives(32, 36, g.pilots[0].lives, COL_BAND);
        DrawPowerTags(&g.pilots[0], 12, 48);
        DrawText("P2", 250, 30, 12, COL_LASER);
        DrawLives(270, 36, g.pilots[1].lives, COL_LASER);
        DrawPowerTags(&g.pilots[1], 250, 48);
    } else {
        Color c = (g.pilots[0].hero == HERO_JENSEN) ? COL_LASER : COL_BAND;
        DrawLives(14, 40, g.pilots[0].lives, c);
        DrawPowerTags(&g.pilots[0], 78, 32);
    }

    Enemy *boss = LiveBoss();
    if (boss) {
        float f = Clampf((float)boss->hp / (float)boss->maxHp, 0.f, 1.f);
        DrawTextC("QUEEN BEETLE", 46, 12, COL_INK);
        DrawRectangle(40, 62, 400, 8, (Color){16, 20, 36, 255});
        Color fill = f > 0.5f ? COL_BUG : (f > 0.25f ? COL_GOLD : COL_BAND);
        DrawRectangle(40, 62, (int)(400.f * f), 8, fill);
    }

    if (g.bannerT > 0.f) {
        DrawRectangle(36, 250, 408, 78, (Color){6, 8, 20, 170});
        DrawTextCOutlined(g.bannerA, 260, 28, COL_GOLD);
        DrawTextCOutlined(g.bannerB, 294, 16, COL_INK);
    }

    if (g.paused) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){4, 6, 16, 150});
        DrawTextCOutlined("PAUSED", 300, 40, COL_GOLD);
        DrawTextC("P resume    Esc hangar", 348, 16, COL_INK);
    }

    if (g.touchMode && !g.paused) {
        DrawText("drag to fly", 12, SCREEN_H - 58, 12, (Color){180, 186, 210, 200});
    }

    if (g.hurtFlash > 0.f) {
        unsigned char a = (unsigned char)(Clampf(g.hurtFlash / 0.18f, 0.f, 1.f) * 70.f);
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){255, 244, 220, a});
    }
}

static void DrawPlay(void) {
    Camera2D cam = {0};
    cam.zoom = 1.f;
    if (g.shake > 0.05f) {
        cam.offset.x = (float)GetRandomValue(-2, 2);
        cam.offset.y = (float)GetRandomValue(-2, 2);
    }
    BeginMode2D(cam);
    DrawWorld();
    EndMode2D();
    DrawHud();
    DrawDisclaimer();
}

static void DrawCard(int index, int hovered) {
    Rectangle r = CardRect(index);
    int hot = (g.sel == index) || hovered;
    Color edge = hot ? COL_GOLD : (Color){70, 78, 112, 255};
    Color bg = hot ? (Color){32, 28, 58, 255} : (Color){16, 18, 36, 230};
    DrawRectangleRounded(r, 0.12f, 6, edge);
    DrawRectangleRounded((Rectangle){r.x + 3, r.y + 3, r.width - 6, r.height - 6}, 0.12f, 6, bg);

    const char *name = "ELON";
    const char *line1 = "Black tee, chunky rocket";
    const char *line2 = "Heavy rockets. Slow, punchy.";
    if (index == MODE_JENSEN) {
        name = "JENSEN";
        line1 = "Grey hair, leather jacket";
        line2 = "Rapid chip-lasers. Light, fast.";
    } else if (index == MODE_COOP) {
        name = "BOTH";
        line1 = "Local co-op, one keyboard";
        line2 = "Shared score. Separate lives.";
    }

    if (index == MODE_COOP) {
        DrawHeroScaled(HERO_ELON, r.x + 40, r.y + 58, 0.62f);
        DrawHeroScaled(HERO_JENSEN, r.x + 78, r.y + 58, 0.62f);
        DrawText(name, (int)r.x + 120, (int)r.y + 22, 26, COL_INK);
    } else {
        int hero = (index == MODE_JENSEN) ? HERO_JENSEN : HERO_ELON;
        DrawHeroScaled(hero, r.x + 52, r.y + 62 + sinf(g.t * 2.f + (float)index) * 2.f, 0.82f);
        DrawText(name, (int)r.x + 100, (int)r.y + 22, 26, COL_INK);
    }
    DrawText(line1, (int)r.x + (index == MODE_COOP ? 120 : 100), (int)r.y + 54, 14, COL_MUTED);
    DrawText(line2, (int)r.x + (index == MODE_COOP ? 120 : 100), (int)r.y + 74, 14, COL_GOLD);
}

static void DrawTitle(void) {
    DrawBackdrop();
    DrawTextCOutlined("A FAN-MADE PARODY", 16, 14, COL_GOLD);
    DrawTextCOutlined("LIFTOFF & LEATHER", 36, 32, COL_INK);
    DrawTextC("Cartoon pilots versus alien bugs", 74, 16, COL_MUTED);
    if (g.best > 0) {
        char b[32];
        snprintf(b, sizeof b, "BEST %06d", g.best);
        DrawTextC(b, 94, 14, COL_GOLD);
    }

    int hovered = -1;
    for (int i = 0; i < 3; i++) {
        if (CheckCollisionPointRec(g_lastMouse, CardRect(i))) hovered = i;
        DrawCard(i, hovered == i);
    }

    Rectangle practice = {90, 450, 300, 40};
    int ph = CheckCollisionPointRec(g_lastMouse, practice);
    DrawRectangleRounded(practice, 0.4f, 6, ph ? COL_GOLD : (Color){70, 78, 112, 255});
    DrawRectangleRounded((Rectangle){practice.x + 2, practice.y + 2, practice.width - 4, practice.height - 4},
                         0.4f, 6, (Color){18, 20, 40, 255});
    DrawTextC("Boss practice   [B]", 460, 18, COL_INK);

    DrawTextC("Arrows / WASD move     Space or Z shoot", 508, 14, COL_MUTED);
    DrawTextC("Co-op: P2 uses arrows + Right Shift", 528, 14, COL_MUTED);
    DrawTextC("1  2  3 launch     Enter launch     B boss", 548, 14, COL_INK);
    DrawTextC("P pause    M mute    Esc hangar", 568, 14, COL_MUTED);
    DrawTextC("Touch: drag to fly, auto-fire", 588, 14, COL_MUTED);
    DrawTextC("F fan shot    Z zip fire    H hull shield", 616, 14, COL_GOLD);
    DrawDisclaimer();
}

static void DrawEnd(int victory) {
    DrawBackdrop();
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){6, 8, 20, 150});
    if (victory) {
        DrawTextCOutlined("BUGS CLEARED", 150, 36, COL_GOLD);
        DrawTextC("Queen Beetle files a polite complaint", 200, 16, COL_INK);
        DrawTextC("and leaves the sector.", 222, 16, COL_INK);
    } else {
        DrawTextCOutlined("OUT OF FUEL", 150, 36, COL_BAND);
        DrawTextC("The bugs throw a very small parade.", 204, 16, COL_INK);
    }
    char buf[40];
    snprintf(buf, sizeof buf, "SCORE %06d", g.score);
    DrawTextCOutlined(buf, 270, 28, COL_INK);
    if (g.best > 0) {
        snprintf(buf, sizeof buf, "BEST %06d", g.best);
        DrawTextC(buf, 306, 16, COL_GOLD);
    }
    DrawTextC(victory && !g.countsBest ? "Practice complete" : " ", 336, 14, COL_MUTED);

    Rectangle again = {90, 380, 300, 48};
    Rectangle hangar = {140, 444, 200, 36};
    int hovA = CheckCollisionPointRec(g_lastMouse, again);
    int hovH = CheckCollisionPointRec(g_lastMouse, hangar);
    DrawRectangleRounded(again, 0.25f, 6, hovA ? COL_GOLD : (Color){90, 98, 130, 255});
    DrawRectangleRounded((Rectangle){again.x + 3, again.y + 3, again.width - 6, again.height - 6}, 0.25f, 6,
                         (Color){20, 24, 46, 255});
    DrawTextC("Fly again", 394, 20, COL_INK);
    DrawRectangleRounded(hangar, 0.3f, 5, hovH ? COL_GOLD : (Color){70, 78, 112, 255});
    DrawTextC("Hangar", 452, 16, COL_MUTED);
    DrawTextC("Enter relaunches    Esc returns", 500, 14, COL_MUTED);
    DrawDisclaimer();
}

static Input ReadInput(void) {
    Input in;
    memset(&in, 0, sizeof in);

    int coop = (g.state == ST_PLAY && g.mode == MODE_COOP);
    int left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
    int right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    int up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
    int down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);

    if (coop) {
        in.ax = (float)((IsKeyDown(KEY_D) ? 1 : 0) - (IsKeyDown(KEY_A) ? 1 : 0));
        in.ay = (float)((IsKeyDown(KEY_S) ? 1 : 0) - (IsKeyDown(KEY_W) ? 1 : 0));
        in.bx = (float)((IsKeyDown(KEY_RIGHT) ? 1 : 0) - (IsKeyDown(KEY_LEFT) ? 1 : 0));
        in.by = (float)((IsKeyDown(KEY_DOWN) ? 1 : 0) - (IsKeyDown(KEY_UP) ? 1 : 0));
        in.aShoot = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z);
        in.bShoot = IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_L);
    } else {
        in.ax = (float)((right ? 1 : 0) - (left ? 1 : 0));
        in.ay = (float)((down ? 1 : 0) - (up ? 1 : 0));
        in.aShoot = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z) || IsKeyDown(KEY_RIGHT_SHIFT) ||
                    IsKeyDown(KEY_L) || IsKeyDown(KEY_J);
    }
    in.shootEdge = Edge(IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_Z), &prevShoot);

    if (IsGamepadAvailable(0)) {
        float gx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float gy = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(gx) > 0.22f) in.ax = gx;
        if (fabsf(gy) > 0.22f) in.ay = gy;
        if (IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
            IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1))
            in.aShoot = true;
    }

    in.confirm = Edge(IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_KP_ENTER), &prevConfirm);
    in.cancel = Edge(IsKeyDown(KEY_ESCAPE), &prevCancel);
    in.mute = Edge(IsKeyDown(KEY_M), &prevMute);
    in.pause = Edge(IsKeyDown(KEY_P), &prevPause);
    in.boss = Edge(IsKeyDown(KEY_B), &prevBoss);
    in.restart = Edge(IsKeyDown(KEY_R), &prevRestart);
    in.up = Edge(IsKeyDown(KEY_UP) || IsKeyDown(KEY_W), &prevUp);
    in.down = Edge(IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S), &prevDown);
    in.left = Edge(IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A), &prevLeft);
    in.right = Edge(IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D), &prevRight);
    in.k1 = Edge(IsKeyDown(KEY_ONE) || IsKeyDown(KEY_KP_1), &prevK1);
    in.k2 = Edge(IsKeyDown(KEY_TWO) || IsKeyDown(KEY_KP_2), &prevK2);
    in.k3 = Edge(IsKeyDown(KEY_THREE) || IsKeyDown(KEY_KP_3), &prevK3);
    in.k4 = Edge(IsKeyDown(KEY_FOUR) || IsKeyDown(KEY_KP_4), &prevK4);

    in.mouse = GetMousePosition();
    g_lastMouse = in.mouse;
    in.mouseEdge = Edge(IsMouseButtonDown(MOUSE_BUTTON_LEFT), &prevMouse);

    in.touches = GetTouchPointCount();
    if (in.touches > 4) in.touches = 4;
    for (int i = 0; i < in.touches; i++) in.touch[i] = GetTouchPosition(i);
    in.touchBegan = (prevTouchCount == 0 && in.touches > 0);
    prevTouchCount = in.touches;
    if (in.touches > 0) g.touchMode = 1;
    return in;
}

static void UpdateTitle(const Input *in) {
    if (in->up || in->left) g.sel = (g.sel + 2) % 3;
    if (in->down || in->right) g.sel = (g.sel + 1) % 3;

    if (in->k1) StartRun(MODE_ELON, 0);
    else if (in->k2) StartRun(MODE_JENSEN, 0);
    else if (in->k3) StartRun(MODE_COOP, 0);
    else if (in->k4 || in->boss) StartRun(g.sel, 1);
    else if (in->confirm || in->shootEdge) StartRun(g.sel, 0);
    else if (in->mouseEdge || in->touchBegan) {
        Vector2 p = in->mouseEdge ? in->mouse : in->touch[0];
        Rectangle practice = {90, 450, 300, 40};
        int started = 0;
        for (int i = 0; i < 3; i++) {
            if (CheckCollisionPointRec(p, CardRect(i))) {
                g.sel = i;
                StartRun(i, 0);
                started = 1;
                break;
            }
        }
        if (!started && CheckCollisionPointRec(p, practice)) StartRun(g.sel, 1);
    }
}

static void UpdateEnd(const Input *in) {
    Rectangle again = {90, 380, 300, 48};
    Rectangle hangar = {140, 444, 200, 36};
    if (in->cancel) {
        g.state = ST_TITLE;
        return;
    }
    if (in->confirm || in->restart) {
        StartRun(g.mode, !g.countsBest);
        return;
    }
    if (in->mouseEdge || in->touchBegan) {
        Vector2 p = in->mouseEdge ? in->mouse : in->touch[0];
        if (CheckCollisionPointRec(p, hangar)) g.state = ST_TITLE;
        else if (CheckCollisionPointRec(p, again) || 1) StartRun(g.mode, !g.countsBest);
    }
}

static void InitBackdrop(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        int layer = i % 3;
        g.stars[i].x = (float)GetRandomValue(0, SCREEN_W);
        g.stars[i].y = (float)GetRandomValue(0, SCREEN_H);
        g.stars[i].v = (layer == 0) ? 28.f : (layer == 1) ? 70.f : 130.f;
        g.stars[i].r = (layer == 0) ? 1.f : (layer == 1) ? 1.5f : 2.2f;
        g.stars[i].a = (unsigned char)(layer == 0 ? 120 : layer == 1 ? 180 : 240);
    }
    Color neb[MAX_NEBULA] = {
        {70, 50, 140, 28},
        {30, 80, 110, 26},
        {120, 50, 90, 18},
        {40, 90, 70, 20},
    };
    for (int i = 0; i < MAX_NEBULA; i++) {
        g.nebulas[i].x = (float)GetRandomValue(20, SCREEN_W - 20);
        g.nebulas[i].y = (float)GetRandomValue(0, SCREEN_H);
        g.nebulas[i].r = (float)GetRandomValue(70, 140);
        g.nebulas[i].v = 12.f + (float)i * 4.f;
        g.nebulas[i].c = neb[i];
    }
}

static float Dt(void) {
    double now = GetTime();
    if (!g_primed) {
        g_primed = 1;
        g_prevTime = now;
        return 1.f / 60.f;
    }
    float dt = (float)(now - g_prevTime);
    g_prevTime = now;
    if (dt < 0.f || dt > 0.05f) dt = 1.f / 60.f;
    return dt;
}

static void Frame(void) {
    float dt = Dt();
    g.t += dt;
    g.sfxBudget = 0;
    Input in = ReadInput();
    if (in.mute) {
        g.muted = !g.muted;
        SfxUnlock();
        if (!g.muted) SfxTone(660.f, 0.06f, 0.04f, 3);
    } else if (in.confirm || in.aShoot || in.touches > 0 || in.mouseEdge) {
        SfxUnlock();
    }

    UpdateBackdrop(dt);

    if (g.state == ST_TITLE) UpdateTitle(&in);
    else if (g.state == ST_PLAY) UpdatePlay(dt, &in);
    else UpdateEnd(&in);

    BeginDrawing();
    ClearBackground(COL_BG);
    if (g.state == ST_TITLE) DrawTitle();
    else if (g.state == ST_PLAY) DrawPlay();
    else DrawEnd(g.state == ST_WIN);
    EndDrawing();
}

int main(void) {
    InitWindow(SCREEN_W, SCREEN_H, "Liftoff & Leather");
    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_WARNING);
    SetRandomSeed((unsigned int)time(NULL));
    memset(&g, 0, sizeof g);
    g.state = ST_TITLE;
    InitBackdrop();
#if defined(PLATFORM_WEB)
    EM_ASM({ console.log("Liftoff & Leather ready", $0, $1); }, SCREEN_W, SCREEN_H);
    emscripten_set_main_loop(Frame, 0, 1);
#else
    while (!WindowShouldClose()) Frame();
    CloseWindow();
#endif
    return 0;
}
