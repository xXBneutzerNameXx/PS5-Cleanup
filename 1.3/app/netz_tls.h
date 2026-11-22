#ifndef NETZ_TLS_H
#define NETZ_TLS_H
/* NEU 1.1 (Weg D): eigene geprüfte HTTPS-Verbindung (mbedTLS 3.6 + Mozilla-Zertifikatsliste) - nur in release und testlauf (MIT_NETZ_TLS).
 * Wie https_anfrage in plattform_ps5.c: true, sobald eine HTTP-Antwort über eine GEPRÜFTE Verbindung kam (status = HTTP-Code, Antwort in
 * *antwort, malloc, mit 0 am Ende, höchstens amax Bytes); false = keine Antwort (grund sagt warum). kopf: "Name", "Wert", ..., NULL.
 * Wartet auf die Antwort - nur aus einem eigenen Faden. */
#include <stdbool.h>
#include <stddef.h>
bool netz_tls_anfrage(const char *methode, const char *url, const char *const *kopf, const void *koerper, size_t klen, const char *ua,
                       char **antwort, size_t *alen, size_t amax, int *status, char *grund, size_t gn);
/* NEU 1.1: Adresse eines Rechners nachschlagen und für spätere Verbindungen merken (falls die Namensauflösung danach ausfällt) */
void netz_tls_name_merken(const char *url);
#endif
