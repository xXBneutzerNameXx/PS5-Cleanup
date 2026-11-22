/*
 * netz_tls.c - NEU 1.1 (Weg D, da_Chris86 15.11.2026: "ich will, dass das mit der Update-Funktion funktioniert"):
 * eigene, GEPRÜFTE HTTPS-Verbindung mit mbedTLS 3.6 (LTS) und der Zertifikatsliste von Mozilla (bau/mbedtls-ps5/ca-bundle.crt, ins
 * Programm eingebettet). Grund: Sonys HTTP-Bibliothek kennt die Stelle nicht, die GitHubs Zertifikate ausstellt (0x8095F00C) - ein
 * Update (ausführbarer Code) wurde darum nie geladen. Hier prüft mbedTLS Aussteller, Gültigkeit und Rechnernamen immer; ohne Prüfung
 * gibt es keine Antwort. Kleines HTTP/1.1 dazu: GET/POST, Weiterleitungen (nur https), Content-Length und chunked.
 * (Erst libcurl+OpenSSL - da_Chris86 15.11.2026: "du hast gemeint das es nur 0.5 bis 1mb sind" - darum wie abgemacht mbedTLS.)
 * Nur in den Fassungen release und testlauf (MIT_NETZ_TLS, siehe bauen.sh).
 */
#ifdef __linux__
#define _GNU_SOURCE   /* strcasestr - nur für den Test auf dem PC (WSL) */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#ifdef __linux__
#include <sys/random.h>
#endif
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"
#include "psa/crypto.h"
#include "netz_tls.h"

/* Zertifikatsliste von Mozilla (PEM), wie fern_seite.html über .incbin eingebettet; mit 0 am Ende (braucht mbedTLS) */
__asm__(".section .rodata\n.global netz_ca_liste\n.balign 16\nnetz_ca_liste:\n"
        ".incbin \"../bau/mbedtls-ps5/ca-bundle.crt\"\n.global netz_ca_liste_ende\nnetz_ca_liste_ende:\n"
        ".byte 0\n.previous\n");
extern const unsigned char netz_ca_liste[], netz_ca_liste_ende[];

/* Kennung im Programm - bauen.sh prüft damit, dass nur release und testlauf die eigene Verbindung enthalten */
const char netz_tls_marke[] = "PS5CLEANUP-EIGENE-HTTPS-VERBINDUNG";

#define KOPF_MAX (64 * 1024)
#define WEITER_MAX 5
#define WARTEN_S 30   /* so lange darf beim Lesen/Schreiben nichts kommen */

/* Zufall: der des Systems (FreeBSD/PS5: arc4random_buf, aus dem Kernel gespeist) - für TLS und PSA (MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) */
static void zufall_fuellen(unsigned char *b, size_t n)
{
#ifdef __linux__
    while (n) { ssize_t r = getrandom(b, n, 0); if (r > 0) { b += r; n -= (size_t)r; } else if (errno != EINTR) abort(); }
#else
    arc4random_buf(b, n);
#endif
}
static int zufall(void *p, unsigned char *b, size_t n) { (void)p; zufall_fuellen(b, n); return 0; }
psa_status_t mbedtls_psa_external_get_random(mbedtls_psa_external_random_context_t *k, uint8_t *b, size_t n, size_t *l)
{
    (void)k; zufall_fuellen(b, n); *l = n; return PSA_SUCCESS;
}

static pthread_once_t einmal = PTHREAD_ONCE_INIT;
static mbedtls_x509_crt ca;
static int ca_anzahl = 0, init_fehler = 0;
static void netz_init(void)
{
    psa_status_t ps = psa_crypto_init();
    if (ps != PSA_SUCCESS) { init_fehler = (int)ps; return; }
    mbedtls_x509_crt_init(&ca);
    int r = mbedtls_x509_crt_parse(&ca, netz_ca_liste, (size_t)(netz_ca_liste_ende - netz_ca_liste) + 1);
    if (r < 0) { init_fehler = r; return; }   /* r > 0: so viele einzelne Zertifikate passten nicht - der Rest gilt */
    for (const mbedtls_x509_crt *c = &ca; c && c->raw.len; c = c->next) ca_anzahl++;
}

/* --- Verbindung (eigene Sende-/Lese-Funktionen statt MBEDTLS_NET_C: nur Sockets, mit Zeitlimit) */
static int bio_senden(void *x, const unsigned char *b, size_t n)
{
    int fd = *(int *)x;
    for (;;) {
#ifdef MSG_NOSIGNAL
        ssize_t r = send(fd, b, n, MSG_NOSIGNAL);
#else
        ssize_t r = send(fd, b, n, 0);
#endif
        if (r >= 0) return (int)r;
        if (errno == EINTR) continue;
        return errno == EAGAIN || errno == EWOULDBLOCK ? MBEDTLS_ERR_SSL_TIMEOUT : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
}
static int bio_lesen(void *x, unsigned char *b, size_t n)
{
    int fd = *(int *)x;
    for (;;) {
        ssize_t r = recv(fd, b, n, 0);
        if (r >= 0) return (int)r;   /* 0 = Gegenseite hat geschlossen */
        if (errno == EINTR) continue;
        return errno == EAGAIN || errno == EWOULDBLOCK ? MBEDTLS_ERR_SSL_TIMEOUT : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
    }
}
/* NEU 1.1 (PS5-Test 17.11.2026 17:31: nach "Alles bereinigen" stand "Kein Internet" - nanoDNS, über das die PS5 Namen auflöst, war mit
 * beendet und gelöscht): jede gefundene Adresse wird gemerkt (je Rechner und Anschluss). Lässt sich ein Name später nicht mehr auflösen,
 * nimmt die Verbindung die gemerkte Adresse; Zertifikat und Rechnername prüft mbedTLS wie immer. Nichts davon wird gespeichert. */
#define NM_MAX 6
#define NM_ADR 4
static struct { char schl[160]; int n; struct sockaddr_storage a[NM_ADR]; socklen_t al[NM_ADR]; int fam[NM_ADR], typ[NM_ADR], prot[NM_ADR]; } nm[NM_MAX];
static int nm_n;
static pthread_mutex_t nm_m = PTHREAD_MUTEX_INITIALIZER;
static void nm_speichern(const char *host, const char *port, const struct addrinfo *l)
{
    char s[160]; if (snprintf(s, sizeof s, "%s:%s", host, port) >= (int)sizeof s) return;
    pthread_mutex_lock(&nm_m);
    int i = 0; while (i < nm_n && strcmp(nm[i].schl, s)) i++;
    if (i == nm_n) { if (nm_n < NM_MAX) nm_n++; else i = NM_MAX - 1; }
    snprintf(nm[i].schl, sizeof nm[i].schl, "%s", s); nm[i].n = 0;
    for (const struct addrinfo *a = l; a && nm[i].n < NM_ADR; a = a->ai_next) {
        if (!a->ai_addr || a->ai_addrlen > sizeof nm[i].a[0]) continue;
        int k = nm[i].n++;
        memcpy(&nm[i].a[k], a->ai_addr, a->ai_addrlen); nm[i].al[k] = a->ai_addrlen;
        nm[i].fam[k] = a->ai_family; nm[i].typ[k] = a->ai_socktype; nm[i].prot[k] = a->ai_protocol;
    }
    pthread_mutex_unlock(&nm_m);
}
/* die gemerkten Adressen als kleine Liste (in den Speicher des Aufrufers); 0 = keine */
static int nm_holen(const char *host, const char *port, struct addrinfo *e, struct sockaddr_storage *k)
{
    char s[160]; if (snprintf(s, sizeof s, "%s:%s", host, port) >= (int)sizeof s) return 0;
    int n = 0;
    pthread_mutex_lock(&nm_m);
    for (int i = 0; i < nm_n; i++) {
        if (strcmp(nm[i].schl, s)) continue;
        for (; n < nm[i].n; n++) {
            memcpy(&k[n], &nm[i].a[n], sizeof k[n]);
            memset(&e[n], 0, sizeof e[n]);
            e[n].ai_family = nm[i].fam[n]; e[n].ai_socktype = nm[i].typ[n]; e[n].ai_protocol = nm[i].prot[n];
            e[n].ai_addr = (struct sockaddr *)&k[n]; e[n].ai_addrlen = nm[i].al[n];
        }
        break;
    }
    pthread_mutex_unlock(&nm_m);
    for (int j = 0; j < n; j++) e[j].ai_next = j + 1 < n ? &e[j + 1] : NULL;
    return n;
}
static int verbinden(const char *host, const char *port, char *grund, size_t gn)
{
    struct addrinfo h, *l = NULL; memset(&h, 0, sizeof h); h.ai_family = AF_UNSPEC; h.ai_socktype = SOCK_STREAM;
    int r = getaddrinfo(host, port, &h, &l);
    struct addrinfo ersatz[NM_ADR]; struct sockaddr_storage ersatz_a[NM_ADR]; bool gemerkt = false;
    if (r || !l) {
        if (l) { freeaddrinfo(l); l = NULL; }
        if (nm_holen(host, port, ersatz, ersatz_a) > 0) { l = ersatz; gemerkt = true; }
        else { snprintf(grund, gn, "Name %s nicht gefunden (%d)", host, r); return -1; }
    } else nm_speichern(host, port, l);
    int fd = -1, e = 0;
    for (struct addrinfo *a = l; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) { e = errno; continue; }
        struct timeval tv = { WARTEN_S, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#ifdef SO_NOSIGPIPE
        { int eins = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &eins, sizeof eins); }
#endif
        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;
        e = errno; close(fd); fd = -1;
    }
    if (!gemerkt) freeaddrinfo(l);
    if (fd < 0) snprintf(grund, gn, "keine Verbindung zu %s (Fehler %d%s)", host, e, gemerkt ? ", gemerkte Adresse" : "");
    return fd;
}

/* --- kleine Helfer fürs HTTP */
static bool url_teilen(const char *url, char *host, size_t hn, char *port, size_t pn, const char **pfad)
{
    if (strncasecmp(url, "https://", 8)) return false;   /* nur HTTPS */
    const char *h = url + 8; size_t l = strcspn(h, ":/?#");
    if (!l || l >= hn) return false;
    memcpy(host, h, l); host[l] = 0;
    const char *r = h + l;
    snprintf(port, pn, "443");
    if (*r == ':') { size_t pl = strspn(r + 1, "0123456789"); if (!pl || pl >= pn) return false; memcpy(port, r + 1, pl); port[pl] = 0; r += 1 + pl; }
    *pfad = *r == '/' ? r : "/";
    return *r == 0 || *r == '/';
}
/* Kopfzeile "Name: Wert" suchen (Groß/klein egal); Wert in w (gekürzt), true wenn gefunden */
static bool kopf_wert(const char *k, size_t kl, const char *name, char *w, size_t wn)
{
    size_t nl = strlen(name);
    for (const char *z = k, *e = k + kl; z < e;) {
        const char *ende = z; while (ende < e && *ende != '\n') ende++;
        if ((size_t)(ende - z) > nl && !strncasecmp(z, name, nl) && z[nl] == ':') {
            const char *v = z + nl + 1; while (v < ende && (*v == ' ' || *v == '\t')) v++;
            const char *ve = ende; while (ve > v && (ve[-1] == '\r' || ve[-1] == ' ')) ve--;
            size_t l = (size_t)(ve - v); if (l >= wn) l = wn - 1;
            memcpy(w, v, l); w[l] = 0; return true;
        }
        z = ende + 1;
    }
    return false;
}
/* chunked entschlüsseln: 1 fertig (Ergebnis in *aus, Länge *al), 0 noch nicht vollständig, -1 kaputt */
static int chunked(const char *b, size_t n, char **aus, size_t *al, size_t max)
{
    size_t i = 0, o = 0; char *d = NULL;
    for (;;) {
        size_t z = i; while (z + 1 < n && !(b[z] == '\r' && b[z + 1] == '\n')) z++;
        if (z + 1 >= n) { free(d); return 0; }
        char *e; unsigned long long g = strtoull(b + i, &e, 16);
        if (e == b + i) { free(d); return -1; }
        i = z + 2;
        if (g == 0) { if (!d) { d = malloc(1); if (!d) return -1; } d[o] = 0; *aus = d; *al = o; return 1; }   /* Anhänge danach egal */
        if (g > max || o + g > max) { free(d); return -1; }
        if (i + g + 2 > n) { free(d); return 0; }
        char *nd = realloc(d, o + g + 1); if (!nd) { free(d); return -1; }
        d = nd; memcpy(d + o, b + i, g); o += g; i += g + 2;
    }
}

/* eine Anfrage ohne Weiterleitung; *ort = neue Adresse bei 3xx (malloc) */
static bool eine_anfrage(const char *methode, const char *url, const char *const *kopf, const void *koerper, size_t klen, const char *ua,
                         char **antwort, size_t *alen, size_t amax, int *status, char **ort, char *grund, size_t gn)
{
    char host[256], port[8]; const char *pfad;
    if (!url_teilen(url, host, sizeof host, port, sizeof port, &pfad)) { snprintf(grund, gn, "Adresse nicht unterstützt"); return false; }
    /* Anfrage zusammenbauen */
    size_t ql = 1024 + strlen(pfad) + strlen(host) + strlen(ua) + klen;
    for (int i = 0; kopf && kopf[i] && kopf[i + 1]; i += 2) ql += strlen(kopf[i]) + strlen(kopf[i + 1]) + 4;
    char *q = malloc(ql); if (!q) { snprintf(grund, gn, "kein Speicher"); return false; }
    int p = snprintf(q, ql, "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n", methode, pfad, host, ua);
    for (int i = 0; kopf && kopf[i] && kopf[i + 1]; i += 2) p += snprintf(q + p, ql - (size_t)p, "%s: %s\r\n", kopf[i], kopf[i + 1]);
    if (!strcmp(methode, "POST")) p += snprintf(q + p, ql - (size_t)p, "Content-Length: %zu\r\n", klen);
    p += snprintf(q + p, ql - (size_t)p, "\r\n");
    if (klen) { memcpy(q + p, koerper, klen); p += (int)klen; }

    bool ok = false; int fd = -1, r; char *b = NULL; size_t bl = 0, bc = 0, kende = 0; char fe[160];
    mbedtls_ssl_context s; mbedtls_ssl_config c;
    mbedtls_ssl_init(&s); mbedtls_ssl_config_init(&c);
    if ((r = mbedtls_ssl_config_defaults(&c, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))) goto tls_fehler;
    mbedtls_ssl_conf_authmode(&c, MBEDTLS_SSL_VERIFY_REQUIRED);   /* immer prüfen */
    mbedtls_ssl_conf_ca_chain(&c, &ca, NULL);
    mbedtls_ssl_conf_rng(&c, zufall, NULL);
    if ((r = mbedtls_ssl_setup(&s, &c))) goto tls_fehler;
    if ((r = mbedtls_ssl_set_hostname(&s, host))) goto tls_fehler;   /* Rechnername prüfen + SNI */
    if ((fd = verbinden(host, port, grund, gn)) < 0) goto ende;
    mbedtls_ssl_set_bio(&s, &fd, bio_senden, bio_lesen, NULL);
    while ((r = mbedtls_ssl_handshake(&s))) {
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        uint32_t v = mbedtls_ssl_get_verify_result(&s);
        if (v && v != (uint32_t)-1) {
            char vi[200]; mbedtls_x509_crt_verify_info(vi, sizeof vi, "", v);
            for (char *t = vi; *t; t++) if (*t == '\n') *t = ' ';
            snprintf(grund, gn, "Zertifikat von %s nicht gültig: %s", host, vi); goto ende;
        }
        goto tls_fehler;
    }
    for (int w = 0; w < p;) {
        r = mbedtls_ssl_write(&s, (const unsigned char *)q + w, (size_t)(p - w));
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (r < 0) goto tls_fehler;
        w += r;
    }
    /* Antwort lesen, bis sie vollständig ist oder die Gegenseite schließt */
    long long cl = -1; bool chk = false, fertig = false;
    for (;;) {
        if (bc - bl < 16 * 1024 + 1) {
            size_t nc = bc ? bc * 2 : 64 * 1024; if (nc > amax + KOPF_MAX + 16 * 1024 + 1) nc = amax + KOPF_MAX + 16 * 1024 + 1;
            if (nc <= bc) { snprintf(grund, gn, "Antwort zu groß"); goto ende; }
            char *nb = realloc(b, nc); if (!nb) { snprintf(grund, gn, "kein Speicher"); goto ende; }
            b = nb; bc = nc; b[bl] = 0;   /* Nachtprüfung 16.11.2026 (Codex-Prüfung 340): immer abgeschlossen, auch wenn noch nichts kam */
        }
        r = mbedtls_ssl_read(&s, (unsigned char *)b + bl, bc - bl - 1);
        if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
#ifdef MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET
        if (r == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) continue;   /* TLS 1.3 */
#endif
        bool eof = r == 0 || r == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || r == MBEDTLS_ERR_SSL_CONN_EOF;
        if (r < 0 && !eof) goto tls_fehler;
        if (r > 0) { bl += (size_t)r; b[bl] = 0; }
        if (!kende) {
            char *e = strstr(b, "\r\n\r\n");
            if (e) {
                kende = (size_t)(e - b) + 4;
                if (sscanf(b, "HTTP/%*d.%*d %d", status) != 1) { snprintf(grund, gn, "keine HTTP-Antwort"); goto ende; }
                char w[2048];
                if (kopf_wert(b, kende, "Transfer-Encoding", w, sizeof w) && strcasestr(w, "chunked")) chk = true;
                else if (kopf_wert(b, kende, "Content-Length", w, sizeof w)) cl = strtoll(w, NULL, 10);
                if (*status >= 300 && *status < 400 && kopf_wert(b, kende, "Location", w, sizeof w)) {
                    if (!strncasecmp(w, "https://", 8)) *ort = strdup(w);
                    else if (w[0] == '/' && w[1] != '/') { size_t l = strlen(host) + strlen(w) + 16; *ort = malloc(l); if (*ort) snprintf(*ort, l, "https://%s%s", host, w); }
                    else { snprintf(grund, gn, "Weiterleitung nicht unterstützt"); goto ende; }
                    if (!*ort) { snprintf(grund, gn, "kein Speicher"); goto ende; }
                    ok = true; goto ende;   /* Inhalt der Weiterleitung braucht es nicht */
                }
                if (cl > (long long)amax) { snprintf(grund, gn, "Antwort zu groß"); goto ende; }
            } else if (bl > KOPF_MAX) { snprintf(grund, gn, "Antwortkopf zu groß"); goto ende; }
        }
        if (kende) {
            size_t kl = bl - kende;
            if (!chk && kl > amax) { snprintf(grund, gn, "Antwort zu groß"); goto ende; }
            if (cl >= 0 && (long long)kl >= cl) { fertig = true; bl = kende + (size_t)cl; }
            else if (chk) {
                char *d; size_t dl; int c2 = chunked(b + kende, kl, &d, &dl, amax);
                if (c2 < 0) { snprintf(grund, gn, "Antwort kaputt oder zu groß"); goto ende; }
                if (c2 > 0) { free(b); b = NULL; *antwort = d; *alen = dl; ok = true; goto ende; }
            }
        }
        if (fertig) break;
        if (eof) {
            if (!kende) { snprintf(grund, gn, "Verbindung ohne Antwort beendet"); goto ende; }
            if (cl >= 0 || chk) { snprintf(grund, gn, "Antwort unvollständig"); goto ende; }
            break;   /* ohne Längenangabe: bis zum Ende */
        }
    }
    {   /* Inhalt nach vorne */
        size_t kl = bl - kende; memmove(b, b + kende, kl); b[kl] = 0;
        *antwort = b; *alen = kl; b = NULL; ok = true;
    }
    goto ende;
tls_fehler:
    mbedtls_strerror(r, fe, sizeof fe);
    snprintf(grund, gn, "%s (-0x%04X)", fe, (unsigned)-r);
ende:
    if (fd >= 0) { mbedtls_ssl_close_notify(&s); close(fd); }
    mbedtls_ssl_free(&s); mbedtls_ssl_config_free(&c);
    free(b); free(q);
    return ok;
}

/* NEU 1.1: nur die Adresse eines Rechners nachschlagen und merken (keine Verbindung, nichts gesendet außer der Namensanfrage) */
void netz_tls_name_merken(const char *url)
{
    char host[256], port[8]; const char *pfad;
    if (!url || !url_teilen(url, host, sizeof host, port, sizeof port, &pfad)) return;
    struct addrinfo h, *l = NULL; memset(&h, 0, sizeof h); h.ai_family = AF_UNSPEC; h.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &h, &l) == 0 && l) nm_speichern(host, port, l);
    if (l) freeaddrinfo(l);
}
bool netz_tls_anfrage(const char *methode, const char *url, const char *const *kopf, const void *koerper, size_t klen, const char *ua,
                      char **antwort, size_t *alen, size_t amax, int *status, char *grund, size_t gn)
{
    *antwort = NULL; *alen = 0; *status = 0; if (gn) grund[0] = 0;
    pthread_once(&einmal, netz_init);
    if (init_fehler || !ca_anzahl) { snprintf(grund, gn, "nicht bereit (%d, %d Zertifikate)", init_fehler, ca_anzahl); return false; }
    char *jetzt = strdup(url);
    if (!jetzt) { snprintf(grund, gn, "kein Speicher"); return false; }
    for (int i = 0; i <= WEITER_MAX; i++) {
        char *ort = NULL;
        bool ok = eine_anfrage(i ? "GET" : methode, jetzt, kopf, i ? NULL : koerper, i ? 0 : klen, ua, antwort, alen, amax, status, &ort, grund, gn);
        if (!ok) { free(jetzt); return false; }
        if (!ort) { free(jetzt); return true; }
        if (!strcmp(methode, "POST")) { free(ort); free(jetzt); snprintf(grund, gn, "Weiterleitung beim Senden nicht unterstützt"); return false; }
        free(jetzt); jetzt = ort; *status = 0;
    }
    free(jetzt);
    snprintf(grund, gn, "zu viele Weiterleitungen");
    return false;
}
