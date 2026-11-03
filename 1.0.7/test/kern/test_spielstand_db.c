/* PS5 Cleanup 1.0.7 - Test für app/spielstand_db.c am PC (da_Chris86 01.11.2026): ein Aufruf, Ergebnis als eine Zeile.
 *   test_spielstand_db <savedata.db> <titel>   (seit 02.11.2026 ohne Sicherung)
 * Ausgabe: "ERGEBNIS r=<0|-1> zeilen=<n> grund=<text>" - geprüft wird in lauf_sauber.sh (mit python3 sqlite3, nur lesen). */
#include "../../app/spielstand_db.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "Aufruf: test_spielstand_db <db> <titel>\n"); return 2; }
    int z = -7; char g[400] = "";
    int r = spielstand_db_austragen(argv[1], argv[2], &z, g, sizeof g);
    printf("ERGEBNIS r=%d zeilen=%d grund=%s\n", r, z, g);
    return 0;
}
