/* PS5 Cleanup - Test der Bedienung im Browser (app/fern.c), neu für die eigene Seite (08.11.2026): Schutz gegen fremde Seiten,
 * Zustand (wartet auf Neues), Liste, Protokoll, Befehle, Bilder, Grenzen, Ausschalten. Bindet fern.c direkt ein (auch die inneren
 * Funktionen). Läuft in WSL, nur über 127.x ("andere Geräte" sind andere Absender-Adressen aus 127.0.0.0/8).
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later */
#define ZUSTAND_WARTEN_MS 600
#define BILD_WARTEN_MS 700
#define LESEN_MS 800
#include "../../app/fern.c"

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static int PORT = 0;
static char ZEILEN[64][300]; static int ZN = 0;
static pthread_mutex_t zm = PTHREAD_MUTEX_INITIALIZER;
static void prot(const char *t) { pthread_mutex_lock(&zm); if (ZN < 64) snprintf(ZEILEN[ZN++], sizeof ZEILEN[0], "%s", t); pthread_mutex_unlock(&zm); printf("     Protokoll: %s\n", t); }
static int geweckt = 0;
static void weck(void) { __atomic_add_fetch(&geweckt, 1, __ATOMIC_RELAXED); }
static bool prot_hat(const char *t) { bool j = false; pthread_mutex_lock(&zm); for (int i = 0; i < ZN; i++) if (strstr(ZEILEN[i], t)) j = true; pthread_mutex_unlock(&zm); return j; }
static int prot_zahl(const char *t) { int j = 0; pthread_mutex_lock(&zm); for (int i = 0; i < ZN; i++) if (strstr(ZEILEN[i], t)) j++; pthread_mutex_unlock(&zm); return j; }
static void warte_ms(int ms) { struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&t, NULL); }
static long ms_seit(uint64_t t0) { return (long)(ms_jetzt() - t0); }

/* eine rohe HTTP-Anfrage von quelle (z. B. "127.0.0.2"), Antwort komplett in a; Rückgabe: Statuscode, -1 = keine Verbindung */
static int roh(const char *quelle, const char *anfrage, size_t an, char *a, size_t n, size_t *alen)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in q; memset(&q, 0, sizeof q); q.sin_family = AF_INET; inet_pton(AF_INET, quelle, &q.sin_addr);
    if (bind(s, (struct sockaddr *)&q, sizeof q) != 0) { close(s); return -1; }
    struct sockaddr_in z; memset(&z, 0, sizeof z); z.sin_family = AF_INET; z.sin_port = htons((uint16_t)PORT); inet_pton(AF_INET, "127.0.0.1", &z.sin_addr);
    if (connect(s, (struct sockaddr *)&z, sizeof z) != 0) { close(s); return -1; }
    struct timeval tv = { 15, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    size_t g = 0; while (g < an) { ssize_t k = send(s, anfrage + g, an - g, MSG_NOSIGNAL); if (k <= 0) break; g += (size_t)k; }
    size_t m = 0; for (;;) { ssize_t k = recv(s, a + m, n - 1 - m, 0); if (k <= 0) break; m += (size_t)k; if (m >= n - 1) break; }
    a[m] = 0; if (alen) *alen = m;
    close(s);
    int code = -1; sscanf(a, "HTTP/1.1 %d", &code);
    return code;
}
static char A[4 * 1024 * 1024];
static size_t A_N = 0;
static char SCHL[17] = "";   /* Schlüssel aus der Seite (fern.c setzt ihn beim Einschalten) - anfrage() hängt ihn an, außer bei "/" und "!pfad" */
static void schluessel_holen(void) { pthread_mutex_lock(&fm); snprintf(SCHL, sizeof SCHL, "%s", F.schluessel); pthread_mutex_unlock(&fm); }
static int anfrage(const char *quelle, const char *meth, const char *pfad0, const char *kopf, const char *inhalt)
{
    char r[12000], pfad[600];
    if (pfad0[0] == '!') snprintf(pfad, sizeof pfad, "%s", pfad0 + 1);
    else if (!strcmp(pfad0, "/")) snprintf(pfad, sizeof pfad, "/");
    else snprintf(pfad, sizeof pfad, "%s%sk=%s", pfad0, strchr(pfad0, '?') ? "&" : "?", SCHL);
    int l = inhalt ? snprintf(r, sizeof r, "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n%sContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n%s",
                              meth, pfad, PORT, kopf ? kopf : "", strlen(inhalt), inhalt)
                   : snprintf(r, sizeof r, "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n%s\r\n", meth, pfad, PORT, kopf ? kopf : "");
    return roh(quelle, r, (size_t)l, A, sizeof A, &A_N);
}
static const char *koerper(void) { const char *k = strstr(A, "\r\n\r\n"); return k ? k + 4 : ""; }
static double json_zahl(const char *schl) { cJSON *j = cJSON_Parse(koerper()); const cJSON *w = cJSON_GetObjectItemCaseSensitive(j, schl); double d = cJSON_IsNumber(w) ? w->valuedouble : -1; cJSON_Delete(j); return d; }
static void *zustand_spaeter(void *x) { warte_ms(250); fern_zustand_geben(strdup((const char *)x)); return NULL; }
static void *bild_liefern(void *x)   /* spielt die Oberfläche: holt Wünsche ab und liefert Bilder (BILD00001 gibt es nicht) */
{
    int *weiter = x;
    while (__atomic_load_n(weiter, __ATOMIC_ACQUIRE)) {
        char id[24];
        while (fern_bild_wunsch(id, sizeof id)) {
            if (!strcmp(id, "BILD00001")) { fern_bild_geben(id, NULL, 0, 0); continue; }
            if (!strcmp(id, "LANG00001")) continue;   /* kommt nie */
            unsigned char px[96 * 96 * 3]; memset(px, 0x60, sizeof px); fern_bild_geben(id, px, 96, 96);
        }
        warte_ms(20);
    }
    return NULL;
}
static int verbinden(void)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in z; memset(&z, 0, sizeof z); z.sin_family = AF_INET; z.sin_port = htons((uint16_t)PORT); inet_pton(AF_INET, "127.0.0.1", &z.sin_addr);
    connect(s, (struct sockaddr *)&z, sizeof z);
    return s;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    char t[600];
    printf("== Innere Regeln\n");
    pruefe(heimnetz(0x0A000001) && heimnetz(0xC0A83201) && heimnetz(0xAC100001) && heimnetz(0xAC1FFFFF) && heimnetz(0x7F000001) && heimnetz(0xA9FE0101),
           "Heimnetz: 10.x, 192.168.x, 172.16.x bis 172.31.x, 127.x, 169.254.x");
    pruefe(!heimnetz(0x08080808) && !heimnetz(0xAC200001) && !heimnetz(0xAC0F0001) && !heimnetz(0xC0A90001) && !heimnetz(0x64400001),
           "nicht Heimnetz: 8.8.8.8, 172.32.x, 172.15.x, 192.169.x, 100.64.x");
    pruefe(host_ok("192.168.178.78:9100") && host_ok("127.0.0.1") && host_ok("localhost:9100") && host_ok("LOCALHOST"), "Host: Zahlen-Adresse mit/ohne Port und localhost");
    pruefe(!host_ok("ps5.fritz.box:9100") && !host_ok("evil.example") && !host_ok("192.168.178.78:91x0") && !host_ok("192.168.178.78:") && !host_ok("1.2.3") && !host_ok("") && !host_ok("[::1]:9100"),
           "Host: Namen, kaputter Port, unvollständige Adresse, leer, IPv6 - abgelehnt");
    pruefe(kennung_ok("PPSA01650") && kennung_ok("CUSA08519_00") && !kennung_ok("ppsa01650") && !kennung_ok("PPSA0165") && !kennung_ok("PPSA01650_0")
           && !kennung_ok("PPSA01650_AB") && !kennung_ok("../../etc") && !kennung_ok("PPSA01650/x") && !kennung_ok(""), "Bild-Kennung: nur 4 Großbuchstaben + 5 Ziffern (+ _NN)");
    {   wert_t w; pruefe(!strcmp(frage_wert("v=12&ab=3", "v", &w), "12") && !strcmp(frage_wert("v=12&ab=3", "ab", &w), "3") && !frage_wert("vv=1", "v", &w) && !frage_wert(NULL, "v", &w),
                         "Werte aus der Adresse (v=12&ab=3)"); }
    {   anfrage_t q; memset(&q, 0, sizeof q); snprintf(q.host, sizeof q.host, "192.168.178.78:9100");
        bool ohne = origin_ok(&q); snprintf(q.origin, sizeof q.origin, "http://192.168.178.78:9100"); bool gleich = origin_ok(&q);
        snprintf(q.origin, sizeof q.origin, "http://evil.example"); bool fremd = origin_ok(&q); snprintf(q.origin, sizeof q.origin, "null"); bool nul = origin_ok(&q);
        pruefe(ohne && gleich && !fremd && !nul, "Herkunft: ohne oder die eigene Adresse ja, fremd oder \"null\" nein"); }

    printf("== Einschalten\n");
    fern_einrichten(prot, weck);
    char f[200] = "";
    pruefe(!fern_an() && !fern_will_daten() && !fern_aktiv(100000), "vor dem Einschalten: aus, niemand fragt");
    pruefe(fern_starten(false, f, sizeof f), "einschalten");
    fern_adresse(t, sizeof t);
    pthread_mutex_lock(&fm); PORT = F.port; pthread_mutex_unlock(&fm); schluessel_holen();
    pruefe(PORT >= 9100 && PORT <= 9109 && strstr(t, "http://") == t && strstr(t, ":91"), t);
    pruefe(prot_hat("Browser-Bedienung eingeschaltet: http://"), "Protokoll: eingeschaltet mit Adresse");
    {   char st[300], soll[80]; snprintf(soll, sizeof soll, "Port %d offen · noch keine Anfrage vom Browser", PORT);
        pruefe(fern_kontakt(st, sizeof st) == 0 && !strcmp(st, soll), st); }
    pruefe(fern_starten(false, f, sizeof f), "zweites Einschalten: nichts passiert");

    printf("== Seite und Schutz\n");
    int c = anfrage("127.0.0.1", "GET", "/", NULL, NULL);
    pruefe(c == 200 && strstr(A, "Content-Type: text/html") && strstr(A, "Content-Security-Policy: default-src 'none'") && strstr(A, "X-Frame-Options: DENY")
           && strstr(A, "Cache-Control: no-store") && strstr(koerper(), "<title>PS5 Cleanup</title>") && strstr(koerper(), "</html>"), "GET / liefert die Seite mit Schutz-Kopfzeilen");
    pruefe(!strstr(A, "Access-Control-Allow"), "keine Freigabe für fremde Seiten (Access-Control-Allow-*)");
    {   char m[80]; snprintf(m, sizeof m, "<meta name=\"k\" content=\"%s\">", SCHL);
        bool hex = strlen(SCHL) == 16; for (int i = 0; i < 16 && hex; i++) hex = (SCHL[i] >= '0' && SCHL[i] <= '9') || (SCHL[i] >= 'a' && SCHL[i] <= 'f');
        pruefe(hex && strstr(koerper(), m) && !strstr(koerper(), "@@@@SCHLUESSEL@@"), "die Seite enthält den Schlüssel (16 Hex-Zeichen) statt des Platzhalters"); }
    pruefe(anfrage("127.0.0.1", "GET", "!/zustand?v=0", NULL, NULL) == 403 && anfrage("127.0.0.1", "GET", "!/bild?id=PPSA01650", NULL, NULL) == 403
           && anfrage("127.0.0.1", "GET", "!/liste?k=0000000000000000", NULL, NULL) == 403 && anfrage("127.0.0.1", "GET", "!/protokoll?ab=0&k=", NULL, NULL) == 403
           && anfrage("127.0.0.1", "POST", "!/befehl", NULL, "{\"b\":\"nachschauen\"}") == 403, "ohne oder mit falschem Schlüssel: 403 (Zustand, Bild, Liste, Protokoll, Befehl)");
    {   char x[64]; pruefe(!fern_befehl_holen(x, sizeof x), "der Befehl ohne Schlüssel kommt nicht an"); }
    anfrage("127.0.0.1", "GET", "/", NULL, NULL);   /* Codex-Prüfung 142 (P3): die Seite noch einmal holen - A enthält sonst die letzte 403-Antwort */
    pruefe(strstr(koerper(), "</html>") && !strstr(koerper(), "<script src") && !strstr(koerper(), "https://") && !strstr(koerper(), "localStorage"), "Seite ohne fremde Skripte und ohne Speichern im Browser");
    pruefe(fern_aktiv(2000) && !fern_will_daten(), "nach der Seite: jemand ist da, aber fragt noch nicht nach dem Zustand");
    {   char st[300]; int kz = fern_kontakt(st, sizeof st);
        pruefe(kz == 1 && strstr(st, " · Anfrage von 127.0.0.1 angekommen") && prot_zahl("erste Anfrage von 127.0.0.1 angekommen (Port") == 1, st);
        int g0 = __atomic_load_n(&geweckt, __ATOMIC_RELAXED);
        anfrage("127.0.0.1", "GET", "/", NULL, NULL); warte_ms(100);
        pruefe(__atomic_load_n(&geweckt, __ATOMIC_RELAXED) == g0 && prot_zahl("erste Anfrage von 127.0.0.1") == 1, "dasselbe Gerät noch einmal: keine neue Zeile, kein Wecken"); }
    char r[9000];
    int l = snprintf(r, sizeof r, "GET / HTTP/1.1\r\nHost: evil.example:%d\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 403, "Host mit Namen (DNS-Rebinding): 403");
    {   char st[300]; int kz = fern_kontakt(st, sizeof st);
        pruefe(kz == 2 && strstr(st, "Anfrage von 127.0.0.1 abgewiesen (nicht die Zahlen-Adresse vom Fernseher)")
               && prot_zahl("Anfrage von 127.0.0.1 abgewiesen (nicht die Zahlen-Adresse vom Fernseher)") == 1, st); }
    l = snprintf(r, sizeof r, "GET /zustand HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nHost: evil.example\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 403, "zwei Host-Zeilen: 403");
    l = snprintf(r, sizeof r, "GET /zustand HTTP/1.1\r\n\r\n");
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 403, "ohne Host: 403");
    pruefe(prot_zahl("abgewiesen") == 1, "dasselbe Gerät steht nur einmal als abgewiesen im Protokoll");
    {   pthread_mutex_lock(&fm); bool n1 = abgewiesen(0x08080808, 2), n2 = abgewiesen(0x08080808, 2); pthread_mutex_unlock(&fm); ausgeben();
        char st[300]; int kz = fern_kontakt(st, sizeof st);
        pruefe(n1 && !n2 && prot_zahl("Anfrage von 8.8.8.8 abgewiesen (nicht im Heimnetz)") == 1 && kz == 2 && strstr(st, "Anfrage von 8.8.8.8 abgewiesen (nicht im Heimnetz)"), st); }
    l = snprintf(r, sizeof r, "GET / HTTP/1.1\r\nHost: localhost:%d\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 200, "Host localhost: 200");
    pruefe(anfrage("127.0.0.1", "OPTIONS", "/befehl", "Origin: http://evil.example\r\nAccess-Control-Request-Method: POST\r\n", NULL) == 405, "Vorab-Anfrage (OPTIONS) einer fremden Seite: 405");
    pruefe(anfrage("127.0.0.1", "PUT", "/", NULL, NULL) == 405, "PUT: 405");
    pruefe(anfrage("127.0.0.1", "GET", "/gibtsnicht", NULL, NULL) == 404, "unbekannte Seite: 404");
    pruefe(anfrage("127.0.0.1", "GET", "/befehl?b=nachschauen", NULL, NULL) == 404, "Befehl als GET (z. B. Bild auf fremder Seite): 404, nichts passiert");
    l = snprintf(r, sizeof r, "POST /befehl HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: text/plain\r\nContent-Length: 18\r\n\r\n{\"b\":\"nachschauen\"}", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 403, "POST ohne application/json (Formular einer fremden Seite): 403");
    l = snprintf(r, sizeof r, "POST /befehl HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nOrigin: http://evil.example\r\nContent-Type: application/json\r\nContent-Length: 18\r\n\r\n{\"b\":\"nachschauen\"}", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 403, "POST mit fremder Herkunft (Origin): 403");
    {   char x[64]; pruefe(!fern_befehl_holen(x, sizeof x), "abgewiesene Befehle kommen nie bei der Oberfläche an"); }
    { char gross[9100]; int n = snprintf(gross, sizeof gross, "GET / HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nX-Viel: ", PORT); memset(gross + n, 'a', sizeof gross - (size_t)n - 5); memcpy(gross + sizeof gross - 5, "\r\n\r\n", 4);
      pruefe(roh("127.0.0.1", gross, sizeof gross - 1, A, sizeof A, NULL) == 413, "Kopf über 8 KB: 413"); }
    l = snprintf(r, sizeof r, "POST /befehl HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: application/json\r\nContent-Length: 5000\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 413, "Inhalt über 2 KB: 413");
    l = snprintf(r, sizeof r, "POST /befehl HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 400, "Transfer-Encoding chunked: 400");
    l = snprintf(r, sizeof r, "POST /befehl HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: application/json\r\nContent-Length: -1\r\n\r\n", PORT);
    pruefe(roh("127.0.0.1", r, (size_t)l, A, sizeof A, NULL) == 400, "negative Länge: 400");

    printf("== Zustand\n");
    uint64_t t0 = ms_jetzt();
    c = anfrage("127.0.0.2", "GET", "/zustand?v=0", NULL, NULL);
    pruefe(c == 204 && ms_seit(t0) >= 500, "noch kein Zustand: wartet (Test: 0,6 s), dann 204");
    pruefe(fern_will_daten(), "danach weiß die Oberfläche: der Browser will Daten");
    fern_zustand_geben(strdup("{\"seite\":\"start\"}"));
    c = anfrage("127.0.0.2", "GET", "/zustand?v=0", NULL, NULL);
    pruefe(c == 200 && strstr(A, "application/json") && strstr(A, "Cache-Control: no-store") && json_zahl("v") == 1 && strstr(koerper(), "\"seite\":\"start\"") && json_zahl("pn") >= 0, koerper());
    fern_zustand_geben(strdup("{\"seite\":\"start\"}"));
    t0 = ms_jetzt(); c = anfrage("127.0.0.2", "GET", "/zustand?v=1", NULL, NULL);
    pruefe(c == 200 && json_zahl("v") == 1 && ms_seit(t0) >= 500, "derselbe Zustand noch einmal zählt nicht neu: v bleibt 1, die Anfrage wartet bis zur Zeitgrenze");
    {   pthread_t th; pthread_create(&th, NULL, zustand_spaeter, "{\"seite\":\"liste\"}");
        t0 = ms_jetzt(); c = anfrage("127.0.0.2", "GET", "/zustand?v=1", NULL, NULL); long d = ms_seit(t0); pthread_join(th, NULL);
        snprintf(t, sizeof t, "neuer Zustand nach 250 ms: die wartende Anfrage kommt sofort zurück (%ld ms, v=%.0f)", d, json_zahl("v"));
        pruefe(c == 200 && json_zahl("v") == 2 && strstr(koerper(), "\"liste\"") && d >= 200 && d < 550, t); }
    fern_zustand_geben(strdup("{}"));
    c = anfrage("127.0.0.2", "GET", "/zustand?v=2", NULL, NULL);
    {   cJSON *j = cJSON_Parse(koerper()); pruefe(c == 200 && json_zahl("v") == 3 && j != NULL, "leerer Zustand {} bleibt gültiges JSON"); cJSON_Delete(j); }
    fern_zustand_geben(NULL); fern_liste_geben(NULL);
    pruefe(true, "NULL als Zustand oder Liste: ohne Absturz");
    printf("== Liste\n");
    pruefe(anfrage("127.0.0.2", "GET", "/liste", NULL, NULL) == 204, "noch keine Liste: 204");
    fern_liste_geben(strdup("{\"fg\":1,\"lv\":1,\"e\":[]}"));
    pruefe(anfrage("127.0.0.2", "GET", "/liste", NULL, NULL) == 200 && !strcmp(koerper(), "{\"fg\":1,\"lv\":1,\"e\":[]}"), "Liste wie gegeben");

    printf("== Protokoll\n");
    for (int i = 0; i < 3010; i++) { char z[40]; snprintf(z, sizeof z, "Zeile %d", i); fern_zeile(z); }
    c = anfrage("127.0.0.2", "GET", "/protokoll?ab=0", NULL, NULL);
    {   cJSON *j = cJSON_Parse(koerper()); const cJSON *z = cJSON_GetObjectItemCaseSensitive(j, "z");
        double ab = json_zahl("ab"), bis = json_zahl("bis"), n = json_zahl("n");
        const cJSON *e0 = cJSON_GetArrayItem(z, 0);
        snprintf(t, sizeof t, "3010 Zeilen: ab 10 (die ältesten sind weg), 500 je Abruf (ab=%.0f bis=%.0f n=%.0f)", ab, bis, n);
        pruefe(c == 200 && ab == 10 && bis == 510 && n == 3010 && cJSON_GetArraySize(z) == 500 && cJSON_IsString(e0) && !strcmp(e0->valuestring, "Zeile 10"), t);
        cJSON_Delete(j); }
    c = anfrage("127.0.0.2", "GET", "/protokoll?ab=3005", NULL, NULL);
    pruefe(c == 200 && json_zahl("ab") == 3005 && json_zahl("bis") == 3010 && strstr(koerper(), "\"Zeile 3009\""), "die letzten 5 Zeilen");
    c = anfrage("127.0.0.2", "GET", "/protokoll?ab=99999", NULL, NULL);
    pruefe(c == 200 && json_zahl("bis") == 3010 && strstr(koerper(), "\"z\":[]"), "ab hinter dem Ende: leer");
    fern_zustand_geben(strdup("{\"x\":1}"));
    c = anfrage("127.0.0.2", "GET", "/zustand?v=3", NULL, NULL);
    pruefe(json_zahl("pn") == 3010, "der Zustand nennt die Zahl der Protokollzeilen (pn)");
    fern_zeile(NULL);
    pruefe(true, "fern_zeile(NULL) ohne Absturz");

    printf("== Befehle\n");
    int g0 = __atomic_load_n(&geweckt, __ATOMIC_RELAXED);
    c = anfrage("127.0.0.2", "POST", "/befehl", "Origin: http://127.0.0.1:0\r\n", "{\"b\":\"wahl\"}");
    pruefe(c == 403, "Herkunft mit anderem Port: 403");
    snprintf(t, sizeof t, "Origin: http://127.0.0.1:%d\r\n", PORT);
    c = anfrage("127.0.0.2", "POST", "/befehl", t, "{\"b\":\"wahl\",\"fg\":3,\"i\":17}");
    {   char x[300] = ""; bool da = fern_befehl_holen(x, sizeof x);
        pruefe(c == 200 && da && !strcmp(x, "{\"b\":\"wahl\",\"fg\":3,\"i\":17}") && __atomic_load_n(&geweckt, __ATOMIC_RELAXED) > g0, "Befehl mit der eigenen Herkunft kommt unverändert an, die Oberfläche wird geweckt"); }
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":\"nachschauen\"}") == 200, "Befehl ohne Origin (ältere Browser): 200");
    {   char x[300]; fern_befehl_holen(x, sizeof x); pruefe(!fern_befehl_holen(x, sizeof x), "danach ist die Schlange leer"); }
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"i\":1}") == 400, "ohne b: 400");
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":7}") == 400, "b keine Zeichenkette: 400");
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":\"\"}") == 400, "b leer: 400");
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":\"123456789012345678901234567890123\"}") == 400, "b länger als 32 Zeichen: 400");
    pruefe(anfrage("127.0.0.2", "POST", "/befehl", NULL, "kein json") == 400 && anfrage("127.0.0.2", "POST", "/befehl", NULL, "[\"b\"]") == 400, "kein JSON-Objekt: 400");
    {   char x[300]; pruefe(!fern_befehl_holen(x, sizeof x), "abgelehnte Befehle kommen nicht an"); }
    {   int ok = 0, voll = 0;
        for (int i = 0; i < 70; i++) { int k = anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":\"info\",\"i\":1}"); if (k == 200) ok++; else if (k == 429) voll++; }
        char x[300]; int n = 0; while (fern_befehl_holen(x, sizeof x)) n++;
        snprintf(t, sizeof t, "70 Befehle ohne Abholen: 64 angenommen, 6 x 429 (%d/%d/%d)", ok, voll, n);
        pruefe(ok == 64 && voll == 6 && n == 64, t); }
    {   char x[8]; anfrage("127.0.0.2", "POST", "/befehl", NULL, "{\"b\":\"nachschauen\"}");
        pruefe(fern_befehl_holen(x, sizeof x) && strlen(x) == 7 && !fern_befehl_holen(x, sizeof x), "zu kleiner Puffer: gekürzt, ohne Überlauf"); }

    printf("== Bilder\n");
    int weiter = 1; pthread_t tb; pthread_create(&tb, NULL, bild_liefern, &weiter);
    pruefe(anfrage("127.0.0.2", "GET", "/bild?id=../etc", NULL, NULL) == 400 && anfrage("127.0.0.2", "GET", "/bild", NULL, NULL) == 400, "falsche oder fehlende Kennung: 400");
    t0 = ms_jetzt(); c = anfrage("127.0.0.2", "GET", "/bild?id=PPSA01650", NULL, NULL);
    {   const unsigned char *k = (const unsigned char *)koerper();
        pruefe(c == 200 && strstr(A, "Content-Type: image/jpeg") && strstr(A, "Cache-Control: private, max-age=600") && k[0] == 0xFF && k[1] == 0xD8, "Bild: im Hauptfaden geladen, als JPEG, darf 10 Minuten im Browser bleiben"); }
    pthread_mutex_lock(&fm); int bn = F.bild_n; pthread_mutex_unlock(&fm);
    c = anfrage("127.0.0.2", "GET", "/bild?id=PPSA01650", NULL, NULL);
    pthread_mutex_lock(&fm); int bn2 = F.bild_n; pthread_mutex_unlock(&fm);
    pruefe(c == 200 && bn == bn2, "dasselbe Bild noch einmal: aus dem Speicher");
    c = anfrage("127.0.0.2", "GET", "/bild?id=BILD00001", NULL, NULL);
    pruefe(c == 404 && strstr(A, "max-age=600"), "kein Bild: 404 (wird auch gemerkt)");
    t0 = ms_jetzt(); c = anfrage("127.0.0.2", "GET", "/bild?id=LANG00001", NULL, NULL);
    pruefe(c == 503 && ms_seit(t0) >= 600, "die Oberfläche liefert nicht (beschäftigt): nach der Zeitgrenze 503, der Browser versucht es später");
    __atomic_store_n(&weiter, 0, __ATOMIC_RELEASE); pthread_join(tb, NULL);
    {   /* viele gleichzeitige Wünsche nach derselben Kennung: nur einmal in der Schlange */
        int s[3]; char q[200]; int ql = snprintf(q, sizeof q, "GET /bild?id=ABCD12345&k=%s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", SCHL, PORT);
        for (int i = 0; i < 3; i++) { s[i] = verbinden(); send(s[i], q, (size_t)ql, MSG_NOSIGNAL); }
        warte_ms(200);
        char id[24]; int n = 0; while (fern_bild_wunsch(id, sizeof id)) n++;
        pruefe(n == 1, "dreimal dieselbe Kennung gleichzeitig: ein Wunsch");
        unsigned char px[16 * 16 * 3]; memset(px, 0x30, sizeof px); fern_bild_geben("ABCD12345", px, 16, 16);
        int gut = 0; for (int i = 0; i < 3; i++) { char a[400] = ""; recv(s[i], a, sizeof a - 1, 0); if (strstr(a, "200 OK")) gut++; close(s[i]); }
        pruefe(gut == 3, "alle drei bekommen das Bild");
    }

    printf("== Fremde Seiten (Sec-Fetch-Site, Codex-Prüfung 140)\n");
    pruefe(anfrage("127.0.0.2", "GET", "/bild?id=PPSA01650", "Sec-Fetch-Site: cross-site\r\n", NULL) == 403
           && anfrage("127.0.0.2", "GET", "/zustand?v=0", "Sec-Fetch-Site: same-site\r\n", NULL) == 403
           && anfrage("127.0.0.2", "GET", "/protokoll?ab=0", "Sec-Fetch-Site: cross-site\r\n", NULL) == 403
           && anfrage("127.0.0.2", "POST", "/befehl", "Sec-Fetch-Site: cross-site\r\n", "{\"b\":\"nachschauen\"}") == 403,
           "Bild, Zustand, Protokoll, Befehl von einer fremden Seite (cross-site/same-site): 403");
    {   char x[64]; pruefe(!fern_befehl_holen(x, sizeof x), "der Befehl der fremden Seite kommt nicht an"); }
    pruefe(anfrage("127.0.0.2", "GET", "/", "Sec-Fetch-Site: cross-site\r\n", NULL) == 200, "die Seite selbst darf man über einen Link öffnen (cross-site): 200");
    pruefe(anfrage("127.0.0.2", "GET", "/liste", "Sec-Fetch-Site: same-origin\r\n", NULL) == 200 && anfrage("127.0.0.2", "GET", "/liste", "Sec-Fetch-Site: none\r\n", NULL) == 200,
           "eigene Seite (same-origin) und direkt eingetippt (none): 200");
    {   /* voller Bild-Speicher: reihum überschreiben statt neue Bilder abzuweisen */
        unsigned char px[8 * 8 * 3]; memset(px, 0x40, sizeof px);
        for (int i = 0; i < BILDER_MAX + 5; i++) { char id[24]; snprintf(id, sizeof id, "VOLL%05d", i); fern_bild_geben(id, i % 2 ? NULL : px, 8, 8); }
        pthread_mutex_lock(&fm); int n = F.bild_n, a0 = bild_suchen("VOLL00000"), aN = bild_suchen("VOLL01028"); pthread_mutex_unlock(&fm);
        snprintf(t, sizeof t, "Bild-Speicher voll (%d Plätze): die ältesten werden überschrieben, das neueste ist da", n);
        pruefe(n == BILDER_MAX && a0 < 0 && aN >= 0, t);
        c = anfrage("127.0.0.2", "GET", "/bild?id=VOLL01028", NULL, NULL);
        pruefe(c == 200, "das neueste Bild wird ausgeliefert"); }

    printf("== Grenzen\n");
    {
        /* 1.0.10 (12.11.2026): zeitabhängig - die 16 Plätze sind nur LESEN_MS (Test: 0,8 s) lang belegt. Unter Volllast (Gesamttest auf
         * allen Kernen) kam die 17. Anfrage manchmal erst nach dieser Zeit an, dann waren die Plätze schon wieder frei und es gab 200 statt 503.
         * Gewertet wird darum nur ein Versuch, dessen 17. Anfrage sicher innerhalb der Zeit losging (bis zu 5 Versuche); gelingt das nie,
         * ist das ein FAIL ("nicht prüfbar") - nie ein stilles Bestehen. Die Prüfung selbst (503) ist unverändert. */
        int s16[16], code = -1, versuch = 0; bool gewertet = false;
        for (; versuch < 5 && !gewertet; versuch++) {
            uint64_t t_erste = 0;
            for (int i = 0; i < 16; i++) { s16[i] = verbinden(); if (i == 0) t_erste = ms_jetzt(); }
            /* warten, bis der Server alle 16 wirklich angenommen hat (unter Last kann das dauern) - statt fest 0,3 s */
            int belegt = 0;
            for (int w = 0; w < 200; w++) { pthread_mutex_lock(&fm); belegt = F.faeden; pthread_mutex_unlock(&fm); if (belegt >= 16) break; warte_ms(5); }
            uint64_t t_los = ms_jetzt();
            code = anfrage("127.0.0.1", "GET", "/", NULL, NULL);
            uint64_t t_ende = ms_jetzt();
            /* 22:18 (Gesamttest): auch eine rechtzeitig losgeschickte Anfrage kann unter Last erst angenommen werden, wenn die 16 Plätze
             * schon wieder frei sind - ein 200, das erst nach Ablauf von LESEN_MS ankam, beweist darum nichts und wird nicht gewertet
             * (ein 503 zählt immer, ein früh angekommenes 200 bleibt ein FAIL) */
            if (belegt >= 16 && t_los - t_erste < LESEN_MS - 150 && (code == 503 || t_ende - t_erste < LESEN_MS - 50)) gewertet = true;
            for (int i = 0; i < 16; i++) close(s16[i]);
            warte_ms(1200);
        }
        if (gewertet && code != 503) printf("     (Antwort %d, Versuch %d)\n", code, versuch);
        if (gewertet) pruefe(code == 503, "16 offene Verbindungen ohne Anfrage: die nächste bekommt 503");
        else pruefe(false, "16 offene Verbindungen ohne Anfrage: nicht prüfbar - die 17. Anfrage kam in 5 Versuchen nie rechtzeitig (Rechner zu stark belastet)");
        pruefe(anfrage("127.0.0.1", "GET", "/", NULL, NULL) == 200, "danach wieder 200");
    }
    {   /* eine Anfrage, die Byte für Byte tröpfelt, wird nach LESEN_MS (Test: 0,8 s) beendet */
        int s = verbinden();
        const char *teil = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n"; bool zu = false;
        for (int i = 0; teil[i] && !zu; i++) { if (send(s, teil + i, 1, MSG_NOSIGNAL) <= 0) zu = true; warte_ms(120); }
        char x[64]; struct timeval tv = { 2, 0 }; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        errno = 0;
        ssize_t rr = zu ? 0 : recv(s, x, sizeof x, 0);
        int fe = errno;
        pruefe(zu || rr == 0 || (rr < 0 && (fe == ECONNRESET || fe == EPIPE)), "Anfrage tröpfelt Byte für Byte: nach der Zeitgrenze ohne Antwort beendet");
        close(s);
    }

    printf("== Ausschalten\n");
    {   /* eine wartende Zustands-Anfrage und offene Verbindungen enden mit dem Ausschalten */
        int s = verbinden(); char q[200]; int ql = snprintf(q, sizeof q, "GET /zustand?v=999&k=%s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n\r\n", SCHL, PORT); send(s, q, (size_t)ql, MSG_NOSIGNAL);
        int s3[3]; for (int i = 0; i < 3; i++) s3[i] = verbinden();
        warte_ms(150);
        t0 = ms_jetzt(); fern_stoppen(); long d = ms_seit(t0);
        warte_ms(400);
        pthread_mutex_lock(&fm); int fa = F.faeden; pthread_mutex_unlock(&fm);
        snprintf(t, sizeof t, "ausgeschaltet nach %ld ms: wartende Anfrage und offene Verbindungen beendet, kein Faden mehr belegt", d);
        pruefe(fa == 0 && d < 1500, t);
        close(s); for (int i = 0; i < 3; i++) close(s3[i]);
    }
    fern_adresse(t, sizeof t);
    pruefe(!fern_an() && !t[0] && prot_hat("Browser-Bedienung ausgeschaltet") && !fern_aktiv(100000) && !fern_will_daten(), "ausgeschaltet: keine Adresse, im Protokoll");
    pruefe(anfrage("127.0.0.1", "GET", "/", NULL, NULL) == -1, "ausgeschaltet: keine Verbindung mehr");
    {   char st[300] = "x"; pruefe(fern_kontakt(st, sizeof st) == 0 && !st[0], "ausgeschaltet: keine Zeile \"Port offen\""); }
    {   char x[64]; pruefe(!fern_befehl_holen(x, sizeof x), "ausgeschaltet: keine Befehle"); }
    fern_stoppen();
    pruefe(true, "zweites Ausschalten ohne Absturz");
    char alt_k[17]; snprintf(alt_k, sizeof alt_k, "%s", SCHL);
    pruefe(fern_starten(true, f, sizeof f) && fern_an(), "wieder einschalten (Englisch)");
    pthread_mutex_lock(&fm); PORT = F.port; pthread_mutex_unlock(&fm); schluessel_holen();
    {   char st[300], soll[80]; snprintf(soll, sizeof soll, "Port %d open · no request from a browser yet", PORT);
        pruefe(fern_kontakt(st, sizeof st) == 0 && !strcmp(st, soll), st); }
    {   char p[80]; snprintf(p, sizeof p, "!/liste?k=%s", alt_k);
        pruefe(strcmp(alt_k, SCHL) && anfrage("127.0.0.6", "GET", p, NULL, NULL) == 403, "nach dem Wiedereinschalten ein neuer Schlüssel, der alte gilt nicht mehr"); }
    anfrage("127.0.0.6", "GET", "/", NULL, NULL);
    {   char st[300]; pruefe(fern_kontakt(st, sizeof st) == 1 && strstr(st, " · request from 127.0.0.6 arrived") && prot_hat("first request from 127.0.0.6 arrived (port"), st); }
    fern_sprache(false);
    {   char st[300]; fern_kontakt(st, sizeof st); pruefe(strstr(st, "Anfrage von 127.0.0.6 angekommen") != NULL, "Sprache umgeschaltet: die Zeile am Fernseher auf Deutsch"); }
    fern_stoppen();
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
