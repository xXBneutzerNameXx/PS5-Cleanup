/* PC-Vorschau des Probelaufs: zeichnet zwei Zustände in BMP-Dateien (nur zum Prüfen des Layouts) */
#define main probelauf_main
#include "probelauf.c"
#undef main
int sceKernelGetHwModelName(char *o) { strcpy(o, "CFI-7021 B01Y"); return 0; }
int sceKernelGetProsperoSystemSwVersion(SwVersion *v) { v->version = 0x12000000; strcpy(v->text, "12.000.000"); return 0; }
int sceSystemServiceHideSplashScreen(void) { return 0; }
int sceSystemServiceLoadExec(const char *p, const char **a) { (void)p; (void)a; return 0; }
int main(void) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Window *f = SDL_CreateWindow("v", 0, 0, BREITE, HOEHE, 0);
  ziel = SDL_GetWindowSurface(f);
  stbtt_InitFont(&font[0], schrift_normal, 0);
  stbtt_InitFont(&font[1], schrift_fett, 0);
  logo_laden(); konsole_lesen();
  zeichnen(); SDL_SaveBMP(ziel, "ohne_controller.bmp");
  pad = (SDL_GameController *)1; strcpy(pad_name, "PS5 Controller");
  tasten[SDL_CONTROLLER_BUTTON_A] = 1; tasten[SDL_CONTROLLER_BUTTON_DPAD_UP] = 1; tasten[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = 1;
  achse[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 16000; achse[SDL_CONTROLLER_AXIS_LEFTX] = 20000; achse[SDL_CONTROLLER_AXIS_LEFTY] = -15000;
  strcpy(letzte_taste, "✕"); strcpy(eingabe, "LÖSCHEN"); strcpy(tastatur_ergebnis, "Tastatur funktioniert – Eingabe übernommen.");
  kreis_seit = SDL_GetTicks() - 500;
  zeichnen(); SDL_SaveBMP(ziel, "mit_controller.bmp");
  return 0;
}
