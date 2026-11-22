/* PS5 Cleanup - Test der Datenbank-Übersicht (NEU 1.0.10, kern_appdb.c und db_appdb_titel): nur lesen.
 *   test_appdb <Ordner mit app.db> <Erwartung>   Erwartung: "KENNUNG=Buchstabe,..." - Sony-Antworten und Installationen fest im Test
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later */
#include "../../kern/kern.h"
#include "../../db/db_lesen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
typedef struct { arena *a; sliste id, wert; } sammler;
static void zeile(void *c, const char *t, const char *f, const char *w) { sammler *s = c; if (!strcmp(f, "APPDB")) { sl_add(s->a, &s->id, a_kopie(s->a, t)); sl_add(s->a, &s->wert, a_kopie(s->a, w)); } }
/* Sony führt (im Test fest): PPSA20000 ja, PPSA20001 nein, alle anderen -> nicht fragbar */
static int sony(const char *id) { return !strcmp(id, "PPSA20000") ? 1 : !strcmp(id, "PPSA20001") || !strncmp(id, "TBSH", 4) || !strncmp(id, "CUSA", 4) || !strcmp(id, "PPSA30000") ? 0 : -1; }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "Aufruf: test_appdb <ordner> <erwartung> [ext_da]\n"); return 2; }
    arena *a = arena_neu();
    sammler s = { a, {0}, {0} };
    const char *g = "";
    int n = db_appdb_titel(a, argv[1], zeile, &s, &g);
    printf("gelesen: %d Titel%s%s\n", n, g[0] ? " - " : "", g);
    pruefe(n == s.id.n && n > 0, "app.db gelesen, je Titel eine Zeile");
    bool ext_da = argc > 3 && !strcmp(argv[3], "1");
    char ist[2000] = "";
    for (int i = 0; i < s.id.n; i++) {
        const char *tx;
        bool inst = !strcmp(s.id.e[i], "PPSA10000");   /* im Test: diese Installation hat die Suche gefunden */
        char e = kern_appdb_einordnen(a, s.id.e[i], s.wert.e[i], -1, inst, ext_da, &tx);
        if (e == 'u') e = kern_appdb_einordnen(a, s.id.e[i], s.wert.e[i], sony(s.id.e[i]), inst, ext_da, &tx);
        printf("     %s %c  %s   [%s]\n", s.id.e[i], e, tx, s.wert.e[i]);
        size_t l = strlen(ist); snprintf(ist + l, sizeof ist - l, "%s%s=%c", l ? "," : "", s.id.e[i], e);
    }
    /* Erwartung: jede genannte Kennung mit ihrem Buchstaben */
    char *erw = strdup(argv[2]);
    for (char *t = strtok(erw, ","); t; t = strtok(NULL, ",")) {
        char was[200]; bool da = strstr(ist, t) != NULL;
        snprintf(was, sizeof was, "eingeordnet %s", t); pruefe(da, was);
    }
    /* der Name darf ';' enthalten und steht vollständig da */
    for (int i = 0; i < s.id.n; i++) if (!strcmp(s.id.e[i], "PPSA30000")) pruefe(strstr(s.wert.e[i], ";n=Name; mit Semikolon") != NULL, "Name mit ';' vollständig");
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
