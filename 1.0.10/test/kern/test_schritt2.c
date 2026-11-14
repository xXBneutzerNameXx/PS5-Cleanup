/* PS5 Cleanup - Tests zu Schritt 2 (Schutzliste)
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 *   test_schritt2 erlaubt <pfade.jsonl> <aus.txt>
 *       Erlaubt() für jeden Pfad, "1"/"0" je Zeile (wie TestKern "erlaubt" in 1.0.5:
 *       zusätzlich Astro-Kennung PPSA01326 und Trophäen NPWR99999_00 bekannt)
 *   test_schritt2 verknuepfung <wurzel> <pfade.jsonl>
 *       OhneVerknuepfung() für jeden Pfad: "V 1|0 pfad" (FAKELINKS wie miniftp4.py)
 */
#include "../../kern/kern.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *lies_datei(const char *p)
{
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)l + 1); if (fread(b, 1, (size_t)l, f) != (size_t)l) exit(2);
    b[l] = 0; fclose(f); return b;
}
static const char *json_text(arena *a, const char *z)
{
    cJSON *j = cJSON_Parse(z);
    if (!j || !cJSON_IsString(j)) { cJSON_Delete(j); return NULL; }
    const char *s = a_kopie(a, j->valuestring); cJSON_Delete(j); return s;
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "erlaubt")) {
        datei_optionen o = { .wurzel = "/gibtsnicht-kein-zugriff-noetig" };
        kern *k = kern_neu(zugriff_datei(&o));
        sl_add(k->a, &k->ASTRO_IDS, "PPSA01326"); sl_add(k->a, &k->ASTRO_NPWRS, "NPWR99999_00");
        char *alles = lies_datei(argv[2]);
        FILE *aus = fopen(argv[3], "w");
        long n = 0, ja = 0;
        char *sp = NULL;
        for (char *z = strtok_r(alles, "\n", &sp); z; z = strtok_r(NULL, "\n", &sp)) {
            size_t l = strlen(z); if (l && z[l - 1] == '\r') z[--l] = 0;
            if (!l) continue;
            arena *a = arena_neu();
            const char *p = json_text(a, z);
            bool r = kern_erlaubt(k, p);
            fprintf(aus, "%s\n", r ? "1" : "0");
            n++; if (r) ja++;
            arena_frei(a);
        }
        fclose(aus);
        printf("erlaubt: %ld von %ld\n", ja, n);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "verknuepfung")) {
        datei_optionen o = { .wurzel = argv[2], .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
        kern *k = kern_neu(zugriff_datei(&o));
        char *alles = lies_datei(argv[3]);
        char *sp = NULL;
        for (char *z = strtok_r(alles, "\n", &sp); z; z = strtok_r(NULL, "\n", &sp)) {
            size_t l = strlen(z); if (l && z[l - 1] == '\r') z[--l] = 0;
            if (!l) continue;
            const char *p = json_text(k->a, z);
            const char *volatile r = "?";
            VERSUCHE { r = kern_ohne_verknuepfung(k, p) ? "1" : "0"; }
            FANGE { r = a_fmt(k->a, "AUSNAHME %s", ausn_text); } ENDE_VERSUCHE;
            printf("V %s %s\n", r, p);
        }
        return 0;
    }
    fprintf(stderr, "Aufruf: erlaubt <pfade.jsonl> <aus.txt> | verknuepfung <wurzel> <pfade.jsonl>\n");
    return 2;
}
