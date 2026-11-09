/* PS5 Cleanup 1.0.9 - Bedienung im Browser
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * fern.c - siehe fern.h. Kleine HTTP-Bearbeitung ohne fremde Bibliothek (wie der Hintergrundteil auf 9099). Die Seite (fern_seite.html)
 * steckt im Programm; Bilder der Kacheln kodiert stb_image_write (Sean Barrett, gemeinfrei bzw. MIT) als JPEG.
 * Sicherheit:
 *  - nur Geräte aus dem Heimnetz (10.x, 172.16-31.x, 192.168.x, 169.254.x) und die PS5 selbst (127.x)
 *  - die Seite antwortet nur, wenn sie mit einer Zahlen-Adresse aufgerufen wird (Kopfzeile Host): eine fremde Webseite kann den
 *    Browser nicht über einen eigenen Namen hierher umleiten (DNS-Rebinding) und dann mitlesen
 *  - Befehle nur als POST mit application/json und ohne fremde Herkunft (Origin) - das kann eine fremde Webseite nicht ohne
 *    Vorab-Anfrage schicken, und die beantwortet PS5 Cleanup nie (keine Access-Control-Allow-*). Lesen (GET) verändert nichts.
 *  - Größen begrenzt: Kopf 8 KB, Inhalt 2 KB, höchstens 16 Verbindungen gleichzeitig, jede Anfrage mit Zeitgrenze
 * Was ein Befehl darf, entscheidet die Oberfläche (app.c fern_befehle) - genau wie bei einem Tastendruck am Controller.
 */
#include "fern.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include "cJSON.h"
#ifdef __PROSPERO__
#include <sys/sysctl.h>   /* Zufall des Kernels für den Schlüssel (kern.arandom) */
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO                  /* nur in den eigenen Speicher */
#include "../probelauf/fremd/stb_image_write.h"   /* (die Warnung bleibt für die ganze Datei aus: stb erklärt Funktionen, die hier fehlen) */

#define FERN_EINBINDEN(name, datei)                                         \
  __asm__(".section .rodata\n.global " #name "\n.balign 16\n" #name ":\n"   \
          ".incbin \"" datei "\"\n.global " #name "_ende\n" #name "_ende:\n" \
          ".byte 0\n.previous\n");                                          \
  extern const unsigned char name[], name##_ende[];
FERN_EINBINDEN(fern_seite, "fern_seite.html")

#define PORT_ERSTER 9100
#define PORT_LETZTER 9109
#define FAEDEN_MAX 16
#define KOPF_MAX 8192
#define INHALT_MAX 2048
#define BEFEHLE_MAX 64
#define PROT_MAX 3000                 /* so viele Protokollzeilen hält die Seite "Protokoll" */
#define PROT_JE_ABRUF 500
#define BILDER_MAX 1024
#define WUNSCH_MAX 64
#ifndef ZUSTAND_WARTEN_MS           /* die Tests (test/kern/test_fern.c) setzen kürzere Zeiten */
#define ZUSTAND_WARTEN_MS 8000
#endif
#ifndef BILD_WARTEN_MS
#define BILD_WARTEN_MS 4000
#endif
#ifndef LESEN_MS                   /* Codex-Prüfung 123 (P2): so lange darf die ganze Anfrage dauern (nicht nur jedes einzelne recv) */
#define LESEN_MS 5000
#endif

static pthread_mutex_t fm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t zustand_c = PTHREAD_COND_INITIALIZER, bild_c = PTHREAD_COND_INITIALIZER;
static void (*f_protokoll)(const char *t) = NULL;
static void (*f_wecken)(void) = NULL;
typedef struct { char id[24]; unsigned char *jpeg; size_t n; bool keins; } bild_t;
static struct {
    bool an; int lausch; int port; char adresse[64]; uint32_t gen; int faeden; bool en;
    char schluessel[17];                     /* Codex-Prüfung 141: steht nur in der Seite selbst, jede Daten-Adresse braucht ihn (k=...) */
    pthread_t t_lausch;
    int offen[FAEDEN_MAX]; int offen_n;       /* Codex-Prüfung 123 (P2): offene Verbindungen - beim Ausschalten beendet */
    /* Daten von der Oberfläche */
    char *zustand; size_t zustand_n; uint32_t zustand_v;
    char *liste; size_t liste_n;
    uint64_t zuletzt, zustand_zuletzt;       /* letzte Anfrage überhaupt / letzte Frage nach dem Zustand */
    /* Befehle an die Oberfläche */
    char befehl[BEFEHLE_MAX][INHALT_MAX + 1]; int bef_a, bef_n;
    /* Protokoll (Ringpuffer) */
    char *prot[PROT_MAX]; long long prot_ges;
    /* Bilder */
    bild_t bild[BILDER_MAX]; int bild_n, bild_reihum;   /* voll: reihum überschreiben (Codex-Prüfung 140, P2) */
    char wunsch[WUNSCH_MAX][24]; int wunsch_a, wunsch_n;
    /* kommt vom Browser etwas an? (da_Chris86 08.11.2026 15:48, Codex-Prüfung 139) - je Gerät das erste Mal angekommen (Bit 1) bzw.
     * abgewiesen (Bit 2), höchstens 8 Geräte; letzt_art: 0 nichts, 1 angekommen, 2 abgewiesen - nicht im Heimnetz, 3 - nicht die Zahlen-Adresse */
    struct { uint32_t ip; unsigned char gemeldet; } bekannt[8]; int bekannt_n;
    int letzt_art; char letzt_ip[20];
} F = { .lausch = -1 };
static void offen_dazu(int c) { if (F.offen_n < FAEDEN_MAX) F.offen[F.offen_n++] = c; }   /* unter fm */
static void offen_weg(int c) { for (int i = 0; i < F.offen_n; i++) if (F.offen[i] == c) { F.offen[i] = F.offen[--F.offen_n]; break; } }   /* unter fm, vor close */

static uint64_t ms_jetzt(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000u + (uint64_t)(t.tv_nsec / 1000000); }
/* Protokollzeilen: unter fm nur gesammelt (melden), geschrieben erst ohne fm (ausgeben) - die Oberfläche nimmt dafür ihre eigene Sperre,
 * und fern.c ruft sie nie, während es fm hält (sonst könnten sich beide Sperren gegenseitig blockieren) */
static char M[6][300]; static int M_n = 0;
static void melden(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void melden(const char *fmt, ...)   /* unter fm */
{
    if (M_n >= 6) return;
    va_list v; va_start(v, fmt); vsnprintf(M[M_n++], sizeof M[0], fmt, v); va_end(v);
}
static void ausgeben(void)   /* ohne fm */
{
    char z[6][300]; int n;
    pthread_mutex_lock(&fm); n = M_n; memcpy(z, M, sizeof z); M_n = 0; pthread_mutex_unlock(&fm);
    for (int i = 0; i < n; i++) if (f_protokoll) f_protokoll(z[i]);
}
static void wecken(void) { if (f_wecken) f_wecken(); }
static const char *TX(const char *de, const char *en) { return F.en ? en : de; }
void fern_einrichten(void (*p)(const char *t), void (*w)(void)) { f_protokoll = p; f_wecken = w; }

static bool heimnetz(uint32_t ip)   /* Adresse in Host-Reihenfolge */
{
    return (ip >> 24) == 10 || (ip >> 24) == 127 || (ip >> 20) == 0xAC1 || (ip >> 16) == 0xC0A8 || (ip >> 16) == 0xA9FE;
}
static void ip_text(char *z, size_t n, uint32_t ip) { struct in_addr a; a.s_addr = htonl(ip); if (!inet_ntop(AF_INET, &a, z, (socklen_t)n)) snprintf(z, n, "?"); }
/* kommt vom Browser etwas an? Alles unter fm; true = zum ersten Mal (dann eine Zeile ins Protokoll und die Anzeige am Fernseher ändert
 * sich). art: Bit 1 angekommen, Bit 2 abgewiesen. Höchstens 8 Geräte insgesamt - weitere werden nicht mehr einzeln gemeldet. */
static bool zum_ersten_mal(uint32_t ip, unsigned char art)
{
    for (int i = 0; i < F.bekannt_n; i++)
        if (F.bekannt[i].ip == ip) { if (F.bekannt[i].gemeldet & art) return false; F.bekannt[i].gemeldet |= art; return true; }
    if (F.bekannt_n >= 8) return false;
    F.bekannt[F.bekannt_n].ip = ip; F.bekannt[F.bekannt_n].gemeldet = art; F.bekannt_n++;
    return true;
}
static const char *abgew_grund(int art) { return art == 3 ? TX("nicht die Zahlen-Adresse vom Fernseher", "not the numeric address from the TV") : TX("nicht im Heimnetz", "not in the home network"); }
static bool kontakt(uint32_t ip)   /* eine gültige Anfrage (Adresse geprüft) aus dem Heimnetz oder von der PS5 selbst */
{
    if (!zum_ersten_mal(ip, 1)) return false;
    ip_text(F.letzt_ip, sizeof F.letzt_ip, ip); F.letzt_art = 1;
    melden(TX("Browser-Bedienung: erste Anfrage von %s angekommen (Port %d).", "Browser control: first request from %s arrived (port %d)."), F.letzt_ip, F.port);
    return true;
}
static bool abgewiesen(uint32_t ip, int art)   /* art 2: nicht im Heimnetz, 3: nicht die Zahlen-Adresse (Kopfzeile Host) */
{
    if (!zum_ersten_mal(ip, 2)) return false;
    ip_text(F.letzt_ip, sizeof F.letzt_ip, ip); F.letzt_art = art;
    melden(TX("Browser-Bedienung: Anfrage von %s abgewiesen (%s).", "Browser control: request from %s refused (%s)."), F.letzt_ip, abgew_grund(art));
    return true;
}
/* eigene Adresse im Heimnetz: ein UDP-Socket "verbindet" sich (es wird nichts gesendet) - das System wählt die Adresse, über die es ginge */
static bool eigene_adresse(char *z, size_t n)
{
    static const char *const ZIEL[] = { "8.8.8.8", "192.168.0.1", "10.0.0.1", "172.16.0.1" };
    for (size_t i = 0; i < sizeof ZIEL / sizeof *ZIEL; i++) {
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) return false;
        struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons(53); inet_pton(AF_INET, ZIEL[i], &a.sin_addr);
        bool ok = false;
        if (connect(s, (struct sockaddr *)&a, sizeof a) == 0) {
            struct sockaddr_in e; socklen_t el = sizeof e;
            if (getsockname(s, (struct sockaddr *)&e, &el) == 0) {
                uint32_t ip = ntohl(e.sin_addr.s_addr);
                if (ip && heimnetz(ip) && (ip >> 24) != 127) { ip_text(z, n, ip); ok = true; }
            }
        }
        close(s);
        if (ok) return true;
    }
    return false;
}

/* =================================================================== Antworten */
static bool alles_senden(int c, const void *d, size_t n)
{
    const char *p = d;
#ifdef MSG_NOSIGNAL
    const int fl = MSG_NOSIGNAL;
#else
    const int fl = 0;
#endif
    while (n > 0) {
        ssize_t k = send(c, p, n, fl);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return false;
        p += k; n -= (size_t)k;
    }
    return true;
}
static void antwort_x(int c, int code, const char *typ, const void *d, size_t n, const char *extra, bool merken)
{
    const char *txt = code == 200 ? "OK" : code == 204 ? "No Content" : code == 400 ? "Bad Request" : code == 403 ? "Forbidden"
                    : code == 404 ? "Not Found" : code == 405 ? "Method Not Allowed" : code == 413 ? "Payload Too Large"
                    : code == 429 ? "Too Many Requests" : "Service Unavailable";
    char k[1024];
    int l = snprintf(k, sizeof k, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: %s\r\nConnection: close\r\n"
                     "X-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\n%s\r\n", code, txt, typ, n,
                     merken ? "private, max-age=600" : "no-store", extra ? extra : "");
    if (l <= 0 || (size_t)l >= sizeof k) return;
    if (alles_senden(c, k, (size_t)l) && n) alles_senden(c, d, n);
}
static void antwort(int c, int code, const char *typ, const void *d, size_t n, const char *extra) { antwort_x(c, code, typ, d, n, extra, false); }
static void json(int c, int code, const char *j) { antwort(c, code, "application/json; charset=utf-8", j, strlen(j), NULL); }

/* =================================================================== Anfrage lesen */
typedef struct {
    char methode[8], pfad[256], host[80], origin[128], typ[80], site[24];
    int hosts; long laenge; bool chunked;
    char inhalt[INHALT_MAX + 1]; size_t in_n;
    char b[KOPF_MAX + 1];
} anfrage_t;
static void kopie(char *z, size_t n, const char *q) { size_t l = strlen(q); while (l && (q[l - 1] == ' ' || q[l - 1] == '\t')) l--; if (l >= n) l = n - 1; memcpy(z, q, l); z[l] = 0; }
static bool bereit(int c, uint64_t bis)   /* Daten da, bevor "bis" erreicht ist? (Codex-Prüfung 123, P2) */
{
    for (;;) {
        uint64_t j = ms_jetzt();
        if (j >= bis) return false;
        struct pollfd p = { c, POLLIN, 0 };
        int r = poll(&p, 1, (int)(bis - j));
        if (r > 0) return true;
        if (r == 0 || errno != EINTR) return false;
    }
}
static int lies(int c, anfrage_t *q)   /* 0 gut, sonst HTTP-Fehlercode; -1 = Verbindung weg oder zu langsam (dann ohne Antwort) */
{
    char *b = q->b;
    size_t n = 0; char *ende = NULL;
    uint64_t bis = ms_jetzt() + LESEN_MS;   /* die ganze Anfrage, nicht nur jedes einzelne recv */
    while (!ende) {
        if (n >= KOPF_MAX) return 413;
        if (!bereit(c, bis)) return -1;
        ssize_t r = recv(c, b + n, KOPF_MAX - n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        n += (size_t)r; b[n] = 0;
        if (memchr(b, 0, n)) return 400;   /* keine Nullbytes im Kopf */
        ende = strstr(b, "\r\n\r\n");
    }
    size_t kopf = (size_t)(ende - b) + 4;
    *ende = 0;
    char *nl = strstr(b, "\r\n"); if (nl) *nl = 0;
    char ver[16] = "";
    if (sscanf(b, "%7s %255s %15s", q->methode, q->pfad, ver) != 3 || strncmp(ver, "HTTP/1.", 7)) return 400;
    for (char *p = nl ? nl + 2 : NULL; p && *p; ) {
        char *e = strstr(p, "\r\n"); if (e) *e = 0;
        char *dp = strchr(p, ':');
        if (dp) {
            *dp = 0; const char *w = dp + 1; while (*w == ' ' || *w == '\t') w++;
            if (!strcasecmp(p, "Host")) { kopie(q->host, sizeof q->host, w); q->hosts++; }
            else if (!strcasecmp(p, "Origin")) kopie(q->origin, sizeof q->origin, w);
            else if (!strcasecmp(p, "Content-Type")) kopie(q->typ, sizeof q->typ, w);
            else if (!strcasecmp(p, "Sec-Fetch-Site")) kopie(q->site, sizeof q->site, w);
            else if (!strcasecmp(p, "Content-Length")) { char *x = NULL; errno = 0; q->laenge = strtol(w, &x, 10); if (errno || !x || x == w || *x || q->laenge < 0) return 400; }
            else if (!strcasecmp(p, "Transfer-Encoding")) q->chunked = true;
        }
        p = e ? e + 2 : NULL;
    }
    if (q->chunked) return 400;
    if (q->laenge > INHALT_MAX) return 413;
    size_t schon = n - kopf;
    if (schon > (size_t)q->laenge) schon = (size_t)q->laenge;
    memcpy(q->inhalt, b + kopf, schon); q->in_n = schon;
    while (q->in_n < (size_t)q->laenge) {
        if (!bereit(c, bis)) return -1;
        ssize_t r = recv(c, q->inhalt + q->in_n, (size_t)q->laenge - q->in_n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        q->in_n += (size_t)r;
    }
    q->inhalt[q->in_n] = 0;
    return 0;
}
static bool host_ok(const char *h)   /* nur Zahlen-Adresse (oder localhost), Port nur aus Ziffern */
{
    char a[80]; snprintf(a, sizeof a, "%s", h);
    char *dp = strrchr(a, ':');
    if (dp) { if (!dp[1]) return false; for (char *p = dp + 1; *p; p++) if (*p < '0' || *p > '9') return false; *dp = 0; }
    if (!strcasecmp(a, "localhost")) return true;
    struct in_addr x;
    return inet_pton(AF_INET, a, &x) == 1;
}
/* Codex-Prüfung 140 (P2): eine fremde Webseite kann Daten-Adressen als Bild oder Skript einbinden (GET ohne CORS) - z. B. massenhaft
 * /bild?id=... (füllt den Speicher, verrät, welche Spiele es gibt). Neuere Browser sagen in Sec-Fetch-Site, woher eine Anfrage kommt:
 * außer der Seite selbst ("/", auch über einen Link oder den QR-Code) nur Anfragen der eigenen Seite. Ohne die Kopfzeile (ältere
 * Browser, andere Programme) wie bisher. */
static bool site_ok(const anfrage_t *q) { return !q->site[0] || !strcasecmp(q->site, "same-origin") || !strcasecmp(q->site, "none"); }
static bool origin_ok(const anfrage_t *q) { if (!q->origin[0]) return true; char s[128]; snprintf(s, sizeof s, "http://%s", q->host); return !strcmp(q->origin, s); }
static bool kennung_ok(const char *id)   /* Kennung eines Bildes: 4 Großbuchstaben, 5 Ziffern, dahinter höchstens "_" und 2 Ziffern */
{
    size_t l = strlen(id);
    if (l != 9 && l != 12) return false;
    for (int i = 0; i < 4; i++) if (id[i] < 'A' || id[i] > 'Z') return false;
    for (int i = 4; i < 9; i++) if (id[i] < '0' || id[i] > '9') return false;
    return l == 9 || (id[9] == '_' && id[10] >= '0' && id[10] <= '9' && id[11] >= '0' && id[11] <= '9');
}
typedef struct { char t[64]; } wert_t;
static const char *frage_wert(const char *frage, const char *name, wert_t *w)   /* "v=12&x=3", "v" -> "12" (in w); NULL = fehlt */
{
    size_t l = strlen(name);
    for (const char *p = frage; p && *p; ) {
        if (!strncmp(p, name, l) && p[l] == '=') { const char *e = strchr(p + l + 1, '&'); size_t n = e ? (size_t)(e - p - l - 1) : strlen(p + l + 1); if (n >= sizeof w->t) n = sizeof w->t - 1; memcpy(w->t, p + l + 1, n); w->t[n] = 0; return w->t; }
        p = strchr(p, '&'); if (p) p++;
    }
    return NULL;
}

/* Codex-Prüfung 141 (P2): Sec-Fetch-Site schicken Browser an http://192.168.x.x nicht mit. Darum steht in der Seite selbst ein
 * zufälliger Schlüssel (neu bei jedem Einschalten), den jede Daten-Adresse mitbringen muss (?k=...). Eine fremde Webseite kann die
 * Seite nicht lesen (keine CORS-Freigabe, Host-Prüfung gegen DNS-Rebinding) und kennt ihn deshalb nicht - auch nicht für Bild-Adressen.
 * Der Nutzer merkt davon nichts. */
#define SCHLUESSEL_PLATZ "@@@@SCHLUESSEL@@"   /* 16 Zeichen in fern_seite.html, wird beim Ausliefern ersetzt */
static bool zufall_datei(const char *n, unsigned char *z, size_t m)   /* m Bytes ganz gelesen? */
{
    int f = open(n, O_RDONLY);
    if (f < 0) return false;
    size_t g = 0;
    while (g < m) { ssize_t k = read(f, z + g, m - g); if (k < 0 && errno == EINTR) continue; if (k <= 0) break; g += (size_t)k; }
    close(f);
    return g == m;
}
/* Codex-Prüfungen 142/143 (P2): nur aus einer Zufallsquelle des Systems mit prüfbarem Erfolg - ohne sie keine Browser-Bedienung. Nicht
 * arc4random_buf: das des PS5-SDK nimmt still Uhrzeit und PID, wenn der Kernel nichts liefert. Auf der PS5 zuerst der Kernel
 * (sysctl kern.arandom, wie das SDK), dann /dev/urandom bzw. /dev/random. */
static bool schluessel_neu(char s[17])
{
    unsigned char z[8] = { 0 }; bool ok = false;
#ifdef __PROSPERO__
    {   int mib[2] = { CTL_KERN, KERN_ARND }; size_t g = 0;
        while (g < sizeof z) { size_t l = sizeof z - g; if (sysctl(mib, 2, z + g, &l, NULL, 0) == -1 || l == 0) break; g += l; }
        ok = g == sizeof z; }
#endif
    if (!ok) ok = zufall_datei("/dev/urandom", z, sizeof z) || zufall_datei("/dev/random", z, sizeof z);
    for (int i = 0; i < 8; i++) snprintf(s + 2 * i, 3, "%02x", z[i]);
    return ok;
}
static bool schluessel_ok(const char *frage)
{
    wert_t w; const char *k = frage_wert(frage, "k", &w);
    if (!k || strlen(k) != 16) return false;
    pthread_mutex_lock(&fm); unsigned d = 0; for (int i = 0; i < 16; i++) d |= (unsigned)(k[i] ^ F.schluessel[i]); bool ok = F.schluessel[0] && !d; pthread_mutex_unlock(&fm);
    return ok;
}
/* =================================================================== die einzelnen Aufrufe */
static void z_zustand(int c, const char *frage, uint32_t gen)   /* wartet, bis es einen neueren Zustand als "v" gibt (höchstens 8 s) */
{
    wert_t wv; const char *vs = frage_wert(frage, "v", &wv);
    unsigned long v = vs ? strtoul(vs, NULL, 10) : 0;
    struct timespec bis; clock_gettime(CLOCK_REALTIME, &bis);
    bis.tv_sec += ZUSTAND_WARTEN_MS / 1000; bis.tv_nsec += (long)(ZUSTAND_WARTEN_MS % 1000) * 1000000L;
    if (bis.tv_nsec >= 1000000000L) { bis.tv_sec++; bis.tv_nsec -= 1000000000L; }
    char *d = NULL; size_t n = 0;
    pthread_mutex_lock(&fm);
    F.zustand_zuletzt = ms_jetzt();
    while (F.an && F.gen == gen && (!F.zustand || F.zustand_v <= v)) {
        if (pthread_cond_timedwait(&zustand_c, &fm, &bis) == ETIMEDOUT) break;
    }
    F.zustand_zuletzt = ms_jetzt();
    if (F.zustand && F.zustand_n >= 2 && F.zustand[0] == '{' && (d = malloc(F.zustand_n + 64))) {   /* vorn die Nummer des Zustands und der Protokollzeilen */
        int k = snprintf(d, 64, "{\"v\":%lu,\"pn\":%lld%s", (unsigned long)F.zustand_v, F.prot_ges, F.zustand[1] == '}' ? "" : ",");
        memcpy(d + k, F.zustand + 1, F.zustand_n - 1); n = (size_t)k + F.zustand_n - 1;
    }
    pthread_mutex_unlock(&fm);
    if (!d) { antwort(c, 204, "text/plain", NULL, 0, NULL); return; }
    antwort(c, 200, "application/json; charset=utf-8", d, n, NULL);
    free(d);
}
static void z_liste(int c)
{
    char *d = NULL; size_t n = 0;
    pthread_mutex_lock(&fm);
    if (F.liste && (d = malloc(F.liste_n))) { memcpy(d, F.liste, F.liste_n); n = F.liste_n; }
    pthread_mutex_unlock(&fm);
    if (!d) { antwort(c, 204, "text/plain", NULL, 0, NULL); return; }
    antwort(c, 200, "application/json; charset=utf-8", d, n, NULL);
    free(d);
}
static void z_protokoll(int c, const char *frage)
{
    wert_t wa; const char *as = frage_wert(frage, "ab", &wa);
    long long ab = as ? strtoll(as, NULL, 10) : 0;
    cJSON *o = cJSON_CreateObject(), *z = cJSON_AddArrayToObject(o, "z");
    pthread_mutex_lock(&fm);
    long long ges = F.prot_ges, erst = ges > PROT_MAX ? ges - PROT_MAX : 0;
    if (ab < erst) ab = erst;
    if (ab > ges) ab = ges;
    long long bis = ab + PROT_JE_ABRUF < ges ? ab + PROT_JE_ABRUF : ges;
    for (long long i = ab; i < bis; i++) cJSON_AddItemToArray(z, cJSON_CreateString(F.prot[i % PROT_MAX] ? F.prot[i % PROT_MAX] : ""));
    pthread_mutex_unlock(&fm);
    cJSON_AddNumberToObject(o, "ab", (double)ab); cJSON_AddNumberToObject(o, "bis", (double)bis); cJSON_AddNumberToObject(o, "n", (double)ges);
    char *t = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    if (!t) { json(c, 503, "{\"fehler\":\"speicher\"}"); return; }
    json(c, 200, t); free(t);
}
static int bild_suchen(const char *id) { for (int i = 0; i < F.bild_n; i++) if (!strcmp(F.bild[i].id, id)) return i; return -1; }   /* unter fm */
static void z_bild(int c, const char *frage, uint32_t gen)
{
    wert_t wi; const char *id = frage_wert(frage, "id", &wi);
    if (!id || !kennung_ok(id)) { json(c, 400, "{\"fehler\":\"kennung\"}"); return; }
    char kid[24]; snprintf(kid, sizeof kid, "%s", id);
    struct timespec bis; clock_gettime(CLOCK_REALTIME, &bis);
    bis.tv_sec += BILD_WARTEN_MS / 1000; bis.tv_nsec += (long)(BILD_WARTEN_MS % 1000) * 1000000L;
    if (bis.tv_nsec >= 1000000000L) { bis.tv_sec++; bis.tv_nsec -= 1000000000L; }
    unsigned char *d = NULL; size_t n = 0; bool keins = false, geweckt = false;
    pthread_mutex_lock(&fm);
    int i = bild_suchen(kid);
    if (i < 0) {
        bool schon = false;
        for (int j = 0; j < F.wunsch_n; j++) if (!strcmp(F.wunsch[(F.wunsch_a + j) % WUNSCH_MAX], kid)) schon = true;
        if (!schon && F.wunsch_n < WUNSCH_MAX) { snprintf(F.wunsch[(F.wunsch_a + F.wunsch_n) % WUNSCH_MAX], 24, "%s", kid); F.wunsch_n++; geweckt = true; }
    }
    pthread_mutex_unlock(&fm);
    if (geweckt) wecken();
    pthread_mutex_lock(&fm);
    while (F.an && F.gen == gen && (i = bild_suchen(kid)) < 0) if (pthread_cond_timedwait(&bild_c, &fm, &bis) == ETIMEDOUT) break;
    if (i >= 0) { keins = F.bild[i].keins; if (!keins && (d = malloc(F.bild[i].n))) { memcpy(d, F.bild[i].jpeg, F.bild[i].n); n = F.bild[i].n; } }
    pthread_mutex_unlock(&fm);
    if (i < 0) { json(c, 503, "{\"fehler\":\"bild\"}"); return; }
    if (keins || !d) { free(d); antwort_x(c, 404, "text/plain", NULL, 0, NULL, true); return; }
    antwort_x(c, 200, "image/jpeg", d, n, NULL, true);
    free(d);
}
static void z_befehl(int c, const anfrage_t *q)
{
    cJSON *j = cJSON_ParseWithLength(q->inhalt, q->in_n);
    const cJSON *b = cJSON_IsObject(j) ? cJSON_GetObjectItemCaseSensitive(j, "b") : NULL;
    bool gut = cJSON_IsString(b) && b->valuestring && b->valuestring[0] && strlen(b->valuestring) <= 32;
    cJSON_Delete(j);
    if (!gut) { json(c, 400, "{\"fehler\":\"befehl\"}"); return; }
    pthread_mutex_lock(&fm);
    bool platz = F.bef_n < BEFEHLE_MAX;
    if (platz) { memcpy(F.befehl[(F.bef_a + F.bef_n) % BEFEHLE_MAX], q->inhalt, q->in_n + 1); F.bef_n++; }
    pthread_mutex_unlock(&fm);
    if (platz) wecken();
    json(c, platz ? 200 : 429, platz ? "{\"ok\":true}" : "{\"fehler\":\"voll\"}");
}

static void bearbeiten(int c, uint32_t ip, uint32_t gen)
{
    anfrage_t *q = calloc(1, sizeof *q);
    if (!q) return;
    int f = lies(c, q);
    if (f < 0) goto ende;
    if (f) { json(c, f, "{\"fehler\":\"anfrage\"}"); goto ende; }
    if (q->hosts != 1 || !host_ok(q->host)) {
        pthread_mutex_lock(&fm); bool neu = F.an && F.gen == gen && abgewiesen(ip, 3); pthread_mutex_unlock(&fm);
        if (neu) { ausgeben(); wecken(); }
        const char *t = "Bitte die Zahlen-Adresse benutzen, die der Fernseher zeigt. / Please use the numeric address shown on the TV.";
        antwort(c, 403, "text/plain; charset=utf-8", t, strlen(t), NULL); goto ende;
    }
    {   /* eine gültige Anfrage ist angekommen (für die Anzeige am Fernseher erst nach der Prüfung der Adresse) */
        pthread_mutex_lock(&fm); F.zuletzt = ms_jetzt(); bool neu = F.an && F.gen == gen && kontakt(ip); pthread_mutex_unlock(&fm);
        if (neu) { ausgeben(); wecken(); }
    }
    bool get = !strcmp(q->methode, "GET"), post = !strcmp(q->methode, "POST");
    if (!get && !post) { json(c, 405, "{\"fehler\":\"methode\"}"); goto ende; }   /* auch OPTIONS (Vorab-Anfrage fremder Seiten): nie erlaubt */
    char *frage = strchr(q->pfad, '?'); if (frage) *frage++ = 0;
    if (post && (strncasecmp(q->typ, "application/json", 16) || !origin_ok(q))) { json(c, 403, "{\"fehler\":\"herkunft\"}"); goto ende; }
    if (get && !strcmp(q->pfad, "/")) {
        const char *csp = "Content-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n";
        size_t n = (size_t)(fern_seite_ende - fern_seite);
        char *d = malloc(n + 1);
        if (!d) { json(c, 503, "{\"fehler\":\"speicher\"}"); goto ende; }
        memcpy(d, fern_seite, n); d[n] = 0;
        char *pl = strstr(d, SCHLUESSEL_PLATZ);
        if (pl) { pthread_mutex_lock(&fm); memcpy(pl, F.schluessel, 16); pthread_mutex_unlock(&fm); }
        antwort(c, 200, "text/html; charset=utf-8", d, n, csp); free(d); goto ende;
    }
    if (!site_ok(q) || !schluessel_ok(frage)) { json(c, 403, "{\"fehler\":\"herkunft\"}"); goto ende; }
    if (get && !strcmp(q->pfad, "/zustand")) z_zustand(c, frage, gen);
    else if (get && !strcmp(q->pfad, "/liste")) z_liste(c);
    else if (get && !strcmp(q->pfad, "/protokoll")) z_protokoll(c, frage);
    else if (get && !strcmp(q->pfad, "/bild")) z_bild(c, frage, gen);
    else if (post && !strcmp(q->pfad, "/befehl")) z_befehl(c, q);
    else json(c, 404, "{\"fehler\":\"unbekannt\"}");
ende:
    free(q);
}
typedef struct { int c; uint32_t ip, gen; } verbindung_t;
static void schliessen(int c)   /* erst die Antwort sicher hinausgeben, Ungelesenes kurz abholen - sonst kann ein RST die Antwort verschlucken */
{
    shutdown(c, SHUT_WR);
    struct timeval tv = { 0, 200000 }; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    char m[512];
    for (int i = 0; i < 64; i++) { ssize_t r = recv(c, m, sizeof m, 0); if (r <= 0) break; }
    close(c);
}
static void *verbindung(void *x)
{
    verbindung_t v = *(verbindung_t *)x; free(x);
    bearbeiten(v.c, v.ip, v.gen);
    pthread_mutex_lock(&fm); offen_weg(v.c); pthread_mutex_unlock(&fm);   /* ab hier beendet fern_stoppen sie nicht mehr */
    schliessen(v.c);
    pthread_mutex_lock(&fm); F.faeden--; pthread_mutex_unlock(&fm);
    return NULL;
}
static int faden(pthread_t *t, void *(*f)(void *), void *x)
{
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 256 * 1024);
    int r = pthread_create(t, &a, f, x);
    pthread_attr_destroy(&a);
    return r;
}
static void *lauscher(void *x)
{
    uint32_t gen = (uint32_t)(uintptr_t)x;
    for (;;) {
        pthread_mutex_lock(&fm);
        bool weiter = F.an && F.gen == gen; int s = F.lausch;
        pthread_mutex_unlock(&fm);
        if (!weiter) break;
        struct pollfd p = { s, POLLIN, 0 };
        if (poll(&p, 1, 500) <= 0) continue;
        struct sockaddr_in a; socklen_t al = sizeof a;
        int c = accept(s, (struct sockaddr *)&a, &al);
        if (c < 0) continue;
        uint32_t ip = ntohl(a.sin_addr.s_addr);
        if (a.sin_family != AF_INET || !heimnetz(ip)) {   /* nur Heimnetz und die PS5 selbst */
            pthread_mutex_lock(&fm); bool neu = F.an && F.gen == gen && abgewiesen(ip, 2); pthread_mutex_unlock(&fm);
            close(c);
            if (neu) { ausgeben(); wecken(); }
            continue;
        }
        struct timeval tv = { 5, 0 }; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        struct timeval ts = { 10, 0 }; setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &ts, sizeof ts);
#ifdef SO_NOSIGPIPE
        int eins = 1; setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &eins, sizeof eins);
#endif
        pthread_mutex_lock(&fm);
        bool ok = F.an && F.gen == gen && F.faeden < FAEDEN_MAX;
        if (ok) { F.faeden++; offen_dazu(c); }
        pthread_mutex_unlock(&fm);
        verbindung_t *v = ok ? malloc(sizeof *v) : NULL;
        pthread_t t;
        if (v) { v->c = c; v->ip = ip; v->gen = gen; }
        if (!v || faden(&t, verbindung, v) != 0) {
            if (ok) { pthread_mutex_lock(&fm); F.faeden--; offen_weg(c); pthread_mutex_unlock(&fm); }
            free(v); json(c, 503, "{\"fehler\":\"voll\"}"); schliessen(c); continue;
        }
        pthread_detach(t);
    }
    return NULL;
}

/* =================================================================== für die Oberfläche */
bool fern_starten(bool englisch, char *fehler, size_t fn)
{
    pthread_mutex_lock(&fm); bool schon = F.an; F.en = englisch; pthread_mutex_unlock(&fm);
    if (schon) return true;
    char ip[32] = "";
    bool ip_da = eigene_adresse(ip, sizeof ip);
    char schl[17] = "";
    if (!schluessel_neu(schl)) { snprintf(fehler, fn, "%s", englisch ? "no random source for the key" : "keine Zufallsquelle für den Schlüssel"); return false; }
    int s = socket(AF_INET, SOCK_STREAM, 0), port = 0;
    if (s < 0) { snprintf(fehler, fn, "%s", englisch ? "no network socket" : "kein Netzwerk-Socket"); return false; }
    int eins = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &eins, sizeof eins);
    for (int p = PORT_ERSTER; p <= PORT_LETZTER && !port; p++) {
        struct sockaddr_in a; memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = htons((uint16_t)p); a.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(s, (struct sockaddr *)&a, sizeof a) == 0) port = p;
    }
    if (!port || listen(s, 16) != 0) { close(s); snprintf(fehler, fn, englisch ? "ports %d to %d are all in use" : "die Ports %d bis %d sind alle belegt", PORT_ERSTER, PORT_LETZTER); return false; }
    pthread_mutex_lock(&fm);
    F.an = true; F.gen++; F.lausch = s; F.port = port; memcpy(F.schluessel, schl, sizeof F.schluessel);
    snprintf(F.adresse, sizeof F.adresse, "http://%s:%d", ip_da ? ip : (englisch ? "<PS5 address>" : "<Adresse der PS5>"), port);
    F.bekannt_n = 0; F.letzt_art = 0; F.letzt_ip[0] = 0; F.bef_n = 0; F.wunsch_n = 0;
    uint32_t gen = F.gen;
    char adr[64]; snprintf(adr, sizeof adr, "%s", F.adresse);
    bool l_ok = faden(&F.t_lausch, lauscher, (void *)(uintptr_t)gen) == 0;
    if (!l_ok) { F.an = false; F.lausch = -1; F.adresse[0] = 0; }
    pthread_mutex_unlock(&fm);
    if (!l_ok) { close(s); snprintf(fehler, fn, "%s", englisch ? "no thread" : "kein Faden"); return false; }
    if (f_protokoll) { char z[300]; snprintf(z, sizeof z, englisch ? "Browser control switched on: %s%s" : "Browser-Bedienung eingeschaltet: %s%s", adr,
              ip_da ? "" : (englisch ? " (own address in the home network not found)" : " (eigene Adresse im Heimnetz nicht gefunden)")); f_protokoll(z); }
    return true;
}
void fern_stoppen(void)
{
    pthread_mutex_lock(&fm);
    if (!F.an) { pthread_mutex_unlock(&fm); return; }
    F.an = false; F.gen++;
    for (int i = 0; i < F.offen_n; i++) shutdown(F.offen[i], SHUT_RDWR);   /* Codex-Prüfung 123 (P2): offene Verbindungen beenden */
    pthread_cond_broadcast(&zustand_c); pthread_cond_broadcast(&bild_c);
    pthread_t tl = F.t_lausch; bool en = F.en;
    pthread_mutex_unlock(&fm);
    pthread_join(tl, NULL);
    pthread_mutex_lock(&fm);
    if (F.lausch >= 0) close(F.lausch);
    F.lausch = -1; F.adresse[0] = 0;
    melden("%s", en ? "Browser control switched off." : "Browser-Bedienung ausgeschaltet.");
    pthread_mutex_unlock(&fm);
    ausgeben();
}
bool fern_an(void) { pthread_mutex_lock(&fm); bool a = F.an; pthread_mutex_unlock(&fm); return a; }
void fern_adresse(char *a, size_t n) { pthread_mutex_lock(&fm); snprintf(a, n, "%s", F.an ? F.adresse : ""); pthread_mutex_unlock(&fm); }
void fern_sprache(bool englisch) { pthread_mutex_lock(&fm); F.en = englisch; pthread_mutex_unlock(&fm); }
int fern_kontakt(char *z, size_t n)
{
    if (!n) return 0;
    pthread_mutex_lock(&fm);
    if (!F.an) { z[0] = 0; pthread_mutex_unlock(&fm); return 0; }
    int r = F.letzt_art == 1 ? 1 : F.letzt_art >= 2 ? 2 : 0;
    char a[200];
    if (r == 1) snprintf(a, sizeof a, TX("Anfrage von %s angekommen", "request from %s arrived"), F.letzt_ip);
    else if (r == 2) snprintf(a, sizeof a, TX("Anfrage von %s abgewiesen (%s)", "request from %s refused (%s)"), F.letzt_ip, abgew_grund(F.letzt_art));
    else snprintf(a, sizeof a, "%s", TX("noch keine Anfrage vom Browser", "no request from a browser yet"));
    snprintf(z, n, TX("Port %d offen · %s", "Port %d open · %s"), F.port, a);
    pthread_mutex_unlock(&fm);
    return r;
}
bool fern_aktiv(uint32_t ms) { pthread_mutex_lock(&fm); bool a = F.an && F.zuletzt && ms_jetzt() - F.zuletzt <= ms; pthread_mutex_unlock(&fm); return a; }
bool fern_will_daten(void) { pthread_mutex_lock(&fm); bool a = F.an && F.zustand_zuletzt && ms_jetzt() - F.zustand_zuletzt <= 15000; pthread_mutex_unlock(&fm); return a; }
void fern_zustand_geben(char *j)
{
    if (!j) return;
    size_t n = strlen(j);
    pthread_mutex_lock(&fm);
    if (F.zustand && F.zustand_n == n && !memcmp(F.zustand, j, n)) { pthread_mutex_unlock(&fm); free(j); return; }   /* nichts Neues */
    free(F.zustand); F.zustand = j; F.zustand_n = n; F.zustand_v++;
    pthread_cond_broadcast(&zustand_c);
    pthread_mutex_unlock(&fm);
}
void fern_liste_geben(char *j)
{
    if (!j) return;
    pthread_mutex_lock(&fm);
    free(F.liste); F.liste = j; F.liste_n = strlen(j);
    pthread_mutex_unlock(&fm);
}
void fern_zeile(const char *t)
{
    if (!t) return;
    char *k = strdup(t);
    pthread_mutex_lock(&fm);
    long long i = F.prot_ges % PROT_MAX;
    free(F.prot[i]); F.prot[i] = k; F.prot_ges++;
    pthread_mutex_unlock(&fm);
}
bool fern_befehl_holen(char *j, size_t n)
{
    pthread_mutex_lock(&fm);
    bool da = F.bef_n > 0;
    if (da) { snprintf(j, n, "%s", F.befehl[F.bef_a]); F.bef_a = (F.bef_a + 1) % BEFEHLE_MAX; F.bef_n--; }
    pthread_mutex_unlock(&fm);
    return da;
}
bool fern_bild_wunsch(char *id, size_t n)
{
    pthread_mutex_lock(&fm);
    bool da = F.wunsch_n > 0;
    if (da) { snprintf(id, n, "%s", F.wunsch[F.wunsch_a]); F.wunsch_a = (F.wunsch_a + 1) % WUNSCH_MAX; F.wunsch_n--; }
    pthread_mutex_unlock(&fm);
    return da;
}
typedef struct { unsigned char *d; size_t n, cap; bool fehler; } puffer_t;
static void jpeg_raus(void *ctx, void *d, int n)
{
    puffer_t *p = ctx;
    if (p->fehler || n <= 0) return;
    if (p->n + (size_t)n > p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 16 * 1024; while (cap < p->n + (size_t)n) cap *= 2;
        unsigned char *x = realloc(p->d, cap); if (!x) { p->fehler = true; return; }
        p->d = x; p->cap = cap;
    }
    memcpy(p->d + p->n, d, (size_t)n); p->n += (size_t)n;
}
void fern_bild_geben(const char *id, const unsigned char *rgb, int w, int h)
{
    puffer_t p = { 0 };
    bool keins = !rgb || w <= 0 || h <= 0 || !stbi_write_jpg_to_func(jpeg_raus, &p, w, h, 3, rgb, 82) || p.fehler;
    if (keins) { free(p.d); p.d = NULL; p.n = 0; }
    pthread_mutex_lock(&fm);
    int i = bild_suchen(id);
    if (i < 0 && F.bild_n < BILDER_MAX) i = F.bild_n++;
    else if (i < 0) { i = F.bild_reihum; F.bild_reihum = (F.bild_reihum + 1) % BILDER_MAX; }
    { free(F.bild[i].jpeg); snprintf(F.bild[i].id, sizeof F.bild[i].id, "%s", id); F.bild[i].jpeg = p.d; F.bild[i].n = p.n; F.bild[i].keins = keins; p.d = NULL; }
    pthread_cond_broadcast(&bild_c);
    pthread_mutex_unlock(&fm);
    free(p.d);
}
