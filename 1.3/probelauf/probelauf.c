/* PS5 Cleanup 1.1 – Probelauf
 * Copyright (C) 2026 da_Chris86 – GPL-3.0-or-later (siehe LICENSE im Ordner 1.1)
 *
 * Nur ein Test der Technik für 1.1. Das Programm
 *   - zeigt eine eigene Oberfläche am Fernseher (SDL2),
 *   - liest Modell und Firmware,
 *   - zeigt alle DualSense-Eingaben an,
 *   - testet auf △ die Bildschirmtastatur,
 *   - endet, wenn ○ eine Sekunde gehalten wird.
 * Es sucht nichts, löscht nichts und schreibt keine Dateien.
 */
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "fremd/stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "fremd/stb_image.h"

/* ---------- eingebettete Dateien (Schrift, Logo) ---------- */
#define EINBINDEN(name, datei)                                              \
  __asm__(".section .rodata\n.global " #name "\n.balign 16\n" #name ":\n"   \
          ".incbin \"" datei "\"\n.global " #name "_ende\n" #name "_ende:\n" \
          ".byte 0\n.previous\n");                                          \
  extern const unsigned char name[], name##_ende[];

EINBINDEN(schrift_normal, "fremd/DejaVuSans.ttf")
EINBINDEN(schrift_fett, "fremd/DejaVuSans-Bold.ttf")
EINBINDEN(logo_png, "logo.png")

/* ---------- PS5-Funktionen ---------- */
int sceKernelGetHwModelName(char *out);
typedef struct {
  uint64_t size;
  char text[0x1C];
  uint32_t version;
} SwVersion;
int sceKernelGetProsperoSystemSwVersion(SwVersion *v);
int sceSystemServiceHideSplashScreen(void);
int sceSystemServiceLoadExec(const char *pfad, const char **argv);

#define BREITE 1920
#define HOEHE 1080

/* Farben wie in der Windows-Version */
#define F_HG      0x141519
#define F_KARTE   0x1F2127
#define F_RAND    0x3A3E49
#define F_TEXT    0xE9EAEE
#define F_GRAU    0x9CA1AD
#define F_BLAU    0x2F6FDB
#define F_BLAU_HELL 0x7FB2FF
#define F_GRUEN   0x6FD39E
#define F_GRUEN_HG 0x14291F
#define F_GELB    0xF2C46B
#define F_ROT     0xFF8A8A

static SDL_Surface *ziel;               /* Fensterfläche */
static stbtt_fontinfo font[2];          /* 0 normal, 1 fett */

/* ---------- Glyphen-Zwischenspeicher ---------- */
typedef struct {
  int belegt, f, groesse, cp;
  int w, h, xoff, yoff, vorschub;
  unsigned char *bild;
} Glyph;
#define GLYPHEN 4096
static Glyph glyphen[GLYPHEN];

static Glyph *glyph(int f, int groesse, int cp) {
  unsigned h = (unsigned)(cp * 2654435761u) ^ (unsigned)(groesse * 97) ^ (unsigned)f;
  for (int i = 0; i < GLYPHEN; i++) {
    Glyph *g = &glyphen[(h + i) % GLYPHEN];
    if (g->belegt && g->f == f && g->groesse == groesse && g->cp == cp) return g;
    if (!g->belegt) {
      float s = stbtt_ScaleForPixelHeight(&font[f], (float)groesse);
      int adv, lsb;
      g->belegt = 1; g->f = f; g->groesse = groesse; g->cp = cp;
      g->bild = stbtt_GetCodepointBitmap(&font[f], s, s, cp, &g->w, &g->h, &g->xoff, &g->yoff);
      stbtt_GetCodepointHMetrics(&font[f], cp, &adv, &lsb);
      g->vorschub = (int)(adv * s + 0.5f);
      return g;
    }
  }
  return NULL;
}

static int utf8_naechstes(const char **s) {
  const unsigned char *p = (const unsigned char *)*s;
  int cp;
  if (p[0] < 0x80) { cp = p[0]; *s += 1; }
  else if ((p[0] & 0xE0) == 0xC0) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *s += 2; }
  else if ((p[0] & 0xF0) == 0xE0) { cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *s += 3; }
  else { cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); *s += 4; }
  return cp;
}

static int textbreite(int f, int groesse, const char *t) {
  int b = 0;
  while (*t) { Glyph *g = glyph(f, groesse, utf8_naechstes(&t)); if (g) b += g->vorschub; }
  return b;
}

/* Text mit Grundlinie bei y zeichnen (Alpha-Mischung direkt in die Fläche) */
static void text(int x, int y, int f, int groesse, uint32_t farbe, const char *t) {
  Uint8 fr = (farbe >> 16) & 255, fg = (farbe >> 8) & 255, fb = farbe & 255;
  SDL_PixelFormat *pf = ziel->format;
  int ascent, descent, gap;
  float s = stbtt_ScaleForPixelHeight(&font[f], (float)groesse);
  stbtt_GetFontVMetrics(&font[f], &ascent, &descent, &gap);
  (void)descent; (void)gap; (void)s;
  while (*t) {
    Glyph *g = glyph(f, groesse, utf8_naechstes(&t));
    if (!g) continue;
    for (int j = 0; j < g->h; j++) {
      int py = y + g->yoff + j;
      if (py < 0 || py >= ziel->h) continue;
      Uint32 *zeile = (Uint32 *)((Uint8 *)ziel->pixels + py * ziel->pitch);
      for (int i = 0; i < g->w; i++) {
        int px = x + g->xoff + i;
        unsigned a = g->bild[j * g->w + i];
        if (!a || px < 0 || px >= ziel->w) continue;
        Uint8 r, gg, b;
        SDL_GetRGB(zeile[px], pf, &r, &gg, &b);
        r = (Uint8)((fr * a + r * (255 - a)) / 255);
        gg = (Uint8)((fg * a + gg * (255 - a)) / 255);
        b = (Uint8)((fb * a + b * (255 - a)) / 255);
        zeile[px] = SDL_MapRGB(pf, r, gg, b);
      }
    }
    x += g->vorschub;
  }
}

static void text_mitte(int cx, int y, int f, int groesse, uint32_t farbe, const char *t) {
  text(cx - textbreite(f, groesse, t) / 2, y, f, groesse, farbe, t);
}

static void rechteck(int x, int y, int w, int h, uint32_t farbe) {
  SDL_Rect r = {x, y, w, h};
  SDL_FillRect(ziel, &r, SDL_MapRGB(ziel->format, (farbe >> 16) & 255, (farbe >> 8) & 255, farbe & 255));
}

static void karte(int x, int y, int w, int h, uint32_t rand, uint32_t innen) {
  rechteck(x, y, w, h, rand);
  rechteck(x + 2, y + 2, w - 4, h - 4, innen);
}

static int wurzel(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
static void kreis(int cx, int cy, int r, uint32_t farbe, int gefuellt) {
  int ri = r - 3;
  for (int dy = -r; dy <= r; dy++) {
    int aussen = wurzel(r * r - dy * dy);
    if (gefuellt || dy * dy >= ri * ri) {
      rechteck(cx - aussen, cy + dy, 2 * aussen + 1, 1, farbe);
    } else {
      int innen = wurzel(ri * ri - dy * dy);
      rechteck(cx - aussen, cy + dy, aussen - innen, 1, farbe);
      rechteck(cx + innen + 1, cy + dy, aussen - innen, 1, farbe);
    }
  }
}

/* ---------- Logo ---------- */
static SDL_Surface *logo;
static void logo_laden(void) {
  int w, h, n;
  unsigned char *px = stbi_load_from_memory(logo_png, (int)(logo_png_ende - logo_png), &w, &h, &n, 4);
  if (!px) return;
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(px, w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
  if (s) { logo = SDL_ConvertSurface(s, ziel->format, 0); SDL_FreeSurface(s); }
  stbi_image_free(px);
}

/* ---------- Modell und Firmware ---------- */
static char modell[64] = "unbekannt", firmware[32] = "unbekannt", fw_quelle[64] = "";

static void konsole_lesen(void) {
  char m[256] = {0};
  if (sceKernelGetHwModelName(m) == 0 && m[0]) {
    snprintf(modell, sizeof modell, "%s", m);
  }
  SwVersion v;
  memset(&v, 0, sizeof v);
  v.size = sizeof v;
  if (sceKernelGetProsperoSystemSwVersion(&v) == 0 && v.version) {
    unsigned hi = (v.version >> 24) & 0xFF, lo = (v.version >> 16) & 0xFF;
    snprintf(firmware, sizeof firmware, "%x.%02x", hi, lo);
    snprintf(fw_quelle, sizeof fw_quelle, "Systemabfrage (%s)", v.text);
    return;
  }
  /* wie in der Windows-Version: /system_ex/etc/index.dat – nur lesen */
  int fd = open("/system_ex/etc/index.dat", O_RDONLY);
  if (fd >= 0) {
    char buf[512] = {0};
    int n = (int)read(fd, buf, sizeof buf - 1);
    close(fd);
    char *p = n > 0 ? strstr(buf, "release:") : NULL;
    if (p) {
      p += 8;
      while (*p == ' ' || *p == '\t') p++;
      if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        unsigned long r = strtoul(p + 2, NULL, 16);
        snprintf(firmware, sizeof firmware, "%lx.%02lx", (r >> 24) & 0xFF, (r >> 16) & 0xFF);
        snprintf(fw_quelle, sizeof fw_quelle, "index.dat");
      }
    }
  }
}

/* ---------- Controller ---------- */
static SDL_GameController *pad;
static char pad_name[128] = "";
static int tasten[SDL_CONTROLLER_BUTTON_MAX];
static int achse[SDL_CONTROLLER_AXIS_MAX];
static char letzte_taste[64] = "–";
static Uint32 kreis_seit = 0;           /* seit wann ○ gehalten wird (0 = nicht) */
static char eingabe[256] = "";
static int tastatur_offen = 0;          /* △ gedrückt, Tastatur angefordert */
static int tastatur_gesehen = 0;        /* Tastatur war wirklich sichtbar */
static Uint32 tastatur_seit = 0;
static char tastatur_ergebnis[96] = "";

static const char *tasten_name(int b) {
  switch (b) {
    case SDL_CONTROLLER_BUTTON_A: return "✕";
    case SDL_CONTROLLER_BUTTON_B: return "○";
    case SDL_CONTROLLER_BUTTON_X: return "□";
    case SDL_CONTROLLER_BUTTON_Y: return "△";
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return "L1";
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return "R1";
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return "L3";
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return "R3";
    case SDL_CONTROLLER_BUTTON_START: return "Options";
    case SDL_CONTROLLER_BUTTON_TOUCHPAD: return "Touchpad";
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return "↑";
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return "↓";
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return "←";
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return "→";
    default: return "?";
  }
}

static void pad_oeffnen(void) {
  if (pad) return;
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    if (SDL_IsGameController(i) && (pad = SDL_GameControllerOpen(i))) {
      const char *n = SDL_GameControllerName(pad);
      snprintf(pad_name, sizeof pad_name, "%s", n ? n : "DualSense");
      SDL_GameControllerSetLED(pad, 0x2F, 0x6F, 0xDB);   /* Lightbar im PS5-Cleanup-Blau */
      return;
    }
  }
}

/* ---------- Zeichnen ---------- */
static void taste_kasten(int x, int y, int w, const char *name, int gedrueckt) {
  karte(x, y, w, 64, gedrueckt ? F_BLAU_HELL : F_RAND, gedrueckt ? F_BLAU : F_KARTE);
  text_mitte(x + w / 2, y + 43, 1, 28, gedrueckt ? 0xFFFFFF : F_GRAU, name);
}

static void stick(int cx, int cy, const char *name, int ax, int ay, int gedrueckt) {
  kreis(cx, cy, 80, gedrueckt ? F_BLAU_HELL : F_RAND, 0);
  int px = cx + ax * 60 / 32767, py = cy + ay * 60 / 32767;
  kreis(px, py, 18, gedrueckt ? F_BLAU_HELL : F_BLAU, 1);
  text_mitte(cx, cy + 120, 0, 24, F_GRAU, name);
}

static void abzug(int x, int y, const char *name, int wert) {
  int w = 260;
  karte(x, y, w, 40, F_RAND, F_KARTE);
  rechteck(x + 2, y + 2, (w - 4) * wert / 32767, 36, F_BLAU);
  char t[48];
  snprintf(t, sizeof t, "%s  %d %%", name, wert * 100 / 32767);
  text(x + 12, y + 29, 0, 22, F_TEXT, t);
}

static void zeichnen(void) {
  char t[256];
  rechteck(0, 0, BREITE, HOEHE, F_HG);

  /* Kopf */
  karte(60, 40, BREITE - 120, 150, F_BLAU, F_KARTE);
  if (logo) {
    SDL_Rect z = {86, 55, 120, 120};
    SDL_BlitScaled(logo, NULL, ziel, &z);
  }
  text(230, 115, 1, 56, F_TEXT, "PS5 Cleanup");
  text(230 + textbreite(1, 56, "PS5 Cleanup") + 24, 115, 1, 30, F_BLAU_HELL, "1.1 Probelauf");
  text(232, 160, 0, 26, F_GRAU, "Nur ein Technik-Test: Es wird nichts gesucht, nichts gelöscht und nichts verändert.");

  /* Konsole */
  karte(60, 220, 560, 300, F_RAND, F_KARTE);
  text(90, 268, 1, 24, F_GRAU, "KONSOLE");
  text(90, 330, 0, 28, F_GRAU, "Modell");
  text(300, 330, 1, 32, F_TEXT, modell);
  text(90, 390, 0, 28, F_GRAU, "Firmware");
  text(300, 390, 1, 32, F_TEXT, firmware);
  if (fw_quelle[0]) text(300, 425, 0, 20, F_GRAU, fw_quelle);
  text(90, 490, 0, 22, F_GRAU, "Nur gelesen.");

  /* Bildschirmtastatur */
  karte(60, 550, 560, 300, F_RAND, F_KARTE);
  text(90, 598, 1, 24, F_GRAU, "BILDSCHIRMTASTATUR");
  text(90, 650, 0, 26, F_TEXT, "△ drücken und z. B. LÖSCHEN eintippen.");
  text(90, 720, 0, 24, F_GRAU, "Eingegeben:");
  text(90, 770, 1, 34, eingabe[0] ? F_GELB : F_GRAU, eingabe[0] ? eingabe : "(noch nichts)");
  if (tastatur_offen) text(90, 820, 0, 22, F_BLAU_HELL, "Tastatur ist offen …");
  else if (tastatur_ergebnis[0]) text(90, 820, 0, 22, strstr(tastatur_ergebnis, "funktioniert") ? F_GRUEN : F_GELB, tastatur_ergebnis);

  /* Controller */
  karte(650, 220, BREITE - 710, 630, F_RAND, F_KARTE);
  text(680, 268, 1, 24, F_GRAU, "DUALSENSE");
  if (!pad) {
    text(680, 340, 1, 32, F_GELB, "Kein Controller erkannt.");
    text(680, 385, 0, 26, F_GRAU, "Bitte eine Taste am DualSense drücken.");
  } else {
    snprintf(t, sizeof t, "%s   ·   zuletzt gedrückt: %s", pad_name, letzte_taste);
    text(680, 310, 0, 26, F_TEXT, t);
    static const int reihe1[] = {SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X,
                                 SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                                 SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT};
    static const int reihe2[] = {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                                 SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
                                 SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_TOUCHPAD};
    for (int i = 0; i < 8; i++) taste_kasten(680 + i * 140, 340, 124, tasten_name(reihe1[i]), tasten[reihe1[i]]);
    for (int i = 0; i < 6; i++) taste_kasten(680 + i * 186, 420, 170, tasten_name(reihe2[i]), tasten[reihe2[i]]);
    abzug(680, 510, "L2", achse[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
    abzug(960, 510, "R2", achse[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]);
    stick(900, 680, "linker Stick", achse[SDL_CONTROLLER_AXIS_LEFTX], achse[SDL_CONTROLLER_AXIS_LEFTY], tasten[SDL_CONTROLLER_BUTTON_LEFTSTICK]);
    stick(1300, 680, "rechter Stick", achse[SDL_CONTROLLER_AXIS_RIGHTX], achse[SDL_CONTROLLER_AXIS_RIGHTY], tasten[SDL_CONTROLLER_BUTTON_RIGHTSTICK]);
  }

  /* Fuß: Beenden */
  karte(60, 880, BREITE - 120, 140, F_RAND, F_KARTE);
  text(90, 940, 1, 30, F_TEXT, "○ eine Sekunde gedrückt halten = Probelauf beenden");
  int anteil = 0;
  if (kreis_seit) { anteil = (int)(SDL_GetTicks() - kreis_seit); if (anteil > 1000) anteil = 1000; }
  karte(90, 965, BREITE - 180, 30, F_RAND, F_HG);
  if (anteil) rechteck(92, 967, (BREITE - 184) * anteil / 1000, 26, F_GRUEN);

}

static int programm(void) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
    printf("PS5 Cleanup Probelauf: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *fenster = SDL_CreateWindow("PS5 Cleanup 1.1 Probelauf", 0, 0, BREITE, HOEHE, 0);
  if (!fenster) {
    printf("PS5 Cleanup Probelauf: SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  ziel = SDL_GetWindowSurface(fenster);
  if (!ziel) {
    printf("PS5 Cleanup Probelauf: SDL_GetWindowSurface: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  stbtt_InitFont(&font[0], schrift_normal, stbtt_GetFontOffsetForIndex(schrift_normal, 0));
  stbtt_InitFont(&font[1], schrift_fett, stbtt_GetFontOffsetForIndex(schrift_fett, 0));
  logo_laden();
  konsole_lesen();
  printf("PS5 Cleanup Probelauf: Modell=%s Firmware=%s (%s)\n", modell, firmware, fw_quelle);
  pad_oeffnen();

  int laeuft = 1, neu = 1;
  while (laeuft) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: laeuft = 0; break;
        case SDL_CONTROLLERDEVICEADDED: pad_oeffnen(); neu = 1; break;
        case SDL_CONTROLLERDEVICEREMOVED:
          if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
            SDL_GameControllerClose(pad); pad = NULL; memset(tasten, 0, sizeof tasten); kreis_seit = 0;
          }
          neu = 1; break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
          int b = e.cbutton.button, unten = e.type == SDL_CONTROLLERBUTTONDOWN;
          if (b >= 0 && b < SDL_CONTROLLER_BUTTON_MAX) tasten[b] = unten;
          if (unten) snprintf(letzte_taste, sizeof letzte_taste, "%s", tasten_name(b));
          if (b == SDL_CONTROLLER_BUTTON_B) kreis_seit = unten ? SDL_GetTicks() : 0;
          if (b == SDL_CONTROLLER_BUTTON_Y && unten && !tastatur_offen) {
            eingabe[0] = 0; tastatur_ergebnis[0] = 0;
            tastatur_offen = 1; tastatur_gesehen = 0; tastatur_seit = SDL_GetTicks();
            SDL_StartTextInput();
          }
          neu = 1; break;
        }
        case SDL_CONTROLLERAXISMOTION:
          if (e.caxis.axis < SDL_CONTROLLER_AXIS_MAX) achse[e.caxis.axis] = e.caxis.value;
          neu = 1; break;
        case SDL_TEXTINPUT:   /* kann in mehreren Stücken kommen */
          strncat(eingabe, e.text.text, sizeof eingabe - strlen(eingabe) - 1);
          neu = 1; break;
        default: break;
      }
    }
    if (tastatur_offen) {
      int sichtbar = SDL_IsScreenKeyboardShown(fenster);
      if (sichtbar) tastatur_gesehen = 1;
      if (tastatur_gesehen && !sichtbar) {
        tastatur_offen = 0;
        SDL_StopTextInput();
        snprintf(tastatur_ergebnis, sizeof tastatur_ergebnis, "%s",
                 eingabe[0] ? "Tastatur funktioniert – Eingabe übernommen." : "Tastatur geschlossen, nichts eingegeben.");
        neu = 1;
      } else if (!tastatur_gesehen && SDL_GetTicks() - tastatur_seit > 3000) {
        tastatur_offen = 0;
        SDL_StopTextInput();
        snprintf(tastatur_ergebnis, sizeof tastatur_ergebnis, "%s", "Tastatur ließ sich nicht öffnen.");
        neu = 1;
      }
    }
    if (kreis_seit) {
      neu = 1;
      if (SDL_GetTicks() - kreis_seit >= 1000) laeuft = 0;
    }
    if (neu) {
      zeichnen();
      SDL_UpdateWindowSurface(fenster);
      neu = 0;
    }
    SDL_Delay(16);
  }

  if (pad) SDL_GameControllerClose(pad);
  if (logo) SDL_FreeSurface(logo);
  SDL_DestroyWindow(fenster);
  SDL_Quit();
  printf("PS5 Cleanup Probelauf: sauber beendet.\n");
  return 0;
}

/* Wie SDL_ps5_main.c: Startbild ausblenden, am Ende sauber zum Home-Menü zurück */
int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  sceSystemServiceHideSplashScreen();
  int r = programm();
  sceSystemServiceLoadExec("exit", NULL);
  return r;
}
