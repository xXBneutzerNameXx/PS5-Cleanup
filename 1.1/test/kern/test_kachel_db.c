/* PS5 Cleanup - Test für kachel_db_austragen (app/spielstand_db.c) am PC (NEU 1.0.10, 12.11.2026, TabShifter TBSH00001):
 *   test_kachel_db <ordner mit app.db und appinfo.db> <titel> <0 = austragen | 1 = nur prüfen>
 * Ausgabe: "ERGEBNIS r=<0|-1|-2> zeilen=<n> grund=<text>" - geprüft wird in lauf_sauber.sh (python3 sqlite3, nur lesen). */
#include "../../app/spielstand_db.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "Aufruf: test_kachel_db <ordner> <titel> <0|1>\n"); return 2; }
    int z = -7; char g[1200] = "", v[300] = "";
    int r = kachel_db_austragen(argv[1], argv[2], atoi(argv[3]), &z, g, sizeof g, v, sizeof v);
    printf("ERGEBNIS r=%d zeilen=%d grund=%s verweis=%s\n", r, z, g, v);
    return 0;
}
