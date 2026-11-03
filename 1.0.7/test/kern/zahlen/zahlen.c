#include "../../../kern/kern.h"
#include <stdio.h>
/* nutzt die echte Formatierung über kern_groesse_text: Werte * 1024 -> KB (N0) bzw. * 1024*1024 -> MB (N1) */
int main(void) {
  double t[] = { 0, 1.5, 2.5, 1023.5, 0.05, 0.15, 0.25, 0.35, 0.45, 1234567.891, 999.95, 9999999.95, 17.5, 1.05, 2.675, 12345678901.5, 1.0/3, 2.0/3, 5.55, 0.95, 1048575.0/1024, 123.45, 6.05, 1022.999 };
  arena *a = arena_neu();
  for (int i = 0; i < 24; i++) {
    SP_EN = false; groesse g = { (long long)0, 0, 0, false, false };
    (void)g;
    printf("%.17g\n", t[i]);
  }
  arena_frei(a); return 0; }
