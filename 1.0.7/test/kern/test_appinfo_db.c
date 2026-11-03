/* PS5 Cleanup 1.0.7 - Test für appinfo_zusatz_reparieren (app/spielstand_db.c) am PC (01.11.2026, Befund Red Dead Redemption 2):
 *   test_appinfo_db <appinfo.db> <titel>   (seit 02.11.2026 ohne Sicherung)
 * Ausgabe: "ERGEBNIS r=<0|-1|-2> zeilen=<n> grund=<text>" - geprüft wird in lauf_sauber.sh Teil 3 (python3 sqlite3, nur lesen). */
#include "../../app/spielstand_db.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "Aufruf: test_appinfo_db <db> <titel>\n"); return 2; }
    int z = -7; char g[400] = "";
    int r = appinfo_zusatz_reparieren(argv[1], argv[2], &z, g, sizeof g);
    printf("ERGEBNIS r=%d zeilen=%d grund=%s\n", r, z, g);
    return 0;
}
