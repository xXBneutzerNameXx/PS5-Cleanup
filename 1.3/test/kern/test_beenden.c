/* PS5 Cleanup - NEU 1.1 (da_Chris86 15.11.2026 07:11): welche sonst weiterlaufenden Programme beim Löschen mit beendet werden
 * (kern_laufende_gewaehlt). Regel: Jailbreak, nanoDNS, Payload Manager und FTP-Server laufen weiter - außer ihr Programm (am Dateikopf
 * erkannt), ihr ganzer Datenordner oder ein Ordner mit ihrem Programm ist zum Löschen gewählt. Etwas IN ihrem Datenordner (ein Spiel in
 * /data/etaHEN/games), eine Datendatei, die nur so heißt, oder eine Verknüpfung beendet sie nicht (Codex-Prüfung 315).
 * Aufruf: test_beenden <leerer Ordner> */
#include "../../kern/kern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static int fehler = 0;
static void pruefe(bool ok, const char *was) { printf("%s%s\n", ok ? "OK   " : "FAIL ", was); if (!ok) fehler++; }
static kern *K;
static const char *W;
static void baue(const char *rel, const char *inhalt)   /* Datei unter der Wurzel anlegen (Ordner davor auch) */
{
    char p[1024]; snprintf(p, sizeof p, "%s%s", W, rel);
    for (char *x = p + strlen(W) + 1; *x; x++) if (*x == '/') { *x = 0; mkdir(p, 0755); *x = '/'; }
    if (!inhalt) { mkdir(p, 0755); return; }
    FILE *f = fopen(p, "wb"); if (f) { fwrite(inhalt, 1, strlen(inhalt), f); fclose(f); }
}
static const char ELF[] = "\x7f" "ELF\x02\x01\x01 Programm";
/* Besitzer-Wechsel am PC nachgestellt wie in test_privat.c: "privat" = Rechte 000 (uns gehörend), der Wechsel öffnet den versperrenden
 * Eintrag für den einen Schritt (0700) und stellt danach die alten Rechte wieder her */
static int wechsel_n = 0;
static bool pc_als_besitzer(int dfd, const char *name, unsigned uid, bool (*tu)(void *c), void *c)
{
    wechsel_n++;
    if (uid != (unsigned)getuid()) return false;
    struct stat st;
    if ((name ? fstatat(dfd, name, &st, AT_SYMLINK_NOFOLLOW) : fstat(dfd, &st)) != 0 || S_ISLNK(st.st_mode) || st.st_uid != uid) return false;
    mode_t alt = st.st_mode & 07777;
    if ((name ? fchmodat(dfd, name, alt | 0700, 0) : fchmod(dfd, alt | 0700)) != 0) return false;
    bool ok = tu(c);
    int e = errno;
    if (name) { if (fchmodat(dfd, name, alt, 0)) {} } else if (fchmod(dfd, alt)) {}
    errno = e;
    return ok;
}
static eintrag *neu(const char *p, bool wahl)
{
    eintrag *f = a_hole(K->a, sizeof *f); memset(f, 0, sizeof *f);
    f->Wahl = wahl; f->Text = f->TextBasis = p;
    if (p) sl_add(K->a, &f->Pfade, p);
    f->PfadeBasis = f->Pfade;
    pl_add(K->a, &K->Funde, f);
    return f;
}
static void leer(void) { K->Funde.n = 0; }
static void fall(const char *p, bool wahl, const char *soll, bool jb, const char *wozu)
{
    leer(); neu(p, wahl);
    const char *ist = kern_laufende_gewaehlt(K);
    char t[500]; snprintf(t, sizeof t, "%s: %s %s -> \"%s\" (soll \"%s\")%s", wozu, wahl ? "gewählt" : "nicht gewählt", p, ist, soll, jb ? ", Jailbreak-Hinweis" : "");
    pruefe(!strcmp(ist, soll) && kern_jailbreak_in(ist) == jb, t);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "Aufruf: test_beenden <ordner>\n"); return 2; }
    W = argv[1];
    baue("/user/data/homebrew/ftpsrv-ps5.elf", ELF);
    baue("/mnt/usb0/payloads/kstuff.elf", ELF);
    baue("/mnt/usb0/payloads/etaHEN-2.1b.bin", ELF);
    baue("/user/data/etaHEN/config.ini", "x=1");
    baue("/user/data/etaHEN/games/Spiel - PPSA01234.ffpkg", "Spieldaten");
    baue("/mnt/usb0/payloads/OnionHEN.elf", ELF);   /* NEU 1.3 (Rückmeldung #50) */
    baue("/user/data/OnionHEN/config.ini", "x=1");
    baue("/user/data/OnionHEN/payloads/meins.elf", ELF);
    baue("/user/data/kstuff_notiz.txt", "nur Text");
    baue("/user/data/kstuff-cache.bin", "Daten, kein Programm");
    baue("/user/data/nanodns/hosts", "127.0.0.1 sony");
    baue("/user/data/zftpd/zftpd.conf", "port=21");
    baue("/user/data/pldmgr/payloads/local/x.elf", ELF);
    baue("/user/data/pldmgr/payloads/repo/ftpsrv@abc/ftpsrv.elf", ELF);
    baue("/user/data/homebrew/MeinFTP/bin/ftpsrv.elf", ELF);
    baue("/user/data/homebrew/shadowmount.elf", ELF);
    baue("/user/data/homebrew/ps5cleanup.elf", ELF);
    baue("/user/data/fremd/irgendwas.elf", ELF);
    { char z[1024], q[1024]; snprintf(q, sizeof q, "%s/user/data/fremd/irgendwas.elf", W); snprintf(z, sizeof z, "%s/user/data/homebrew/kstuff.elf", W); unlink(z); if (symlink(q, z)) perror("symlink"); }
    /* Codex-Prüfung 317: Verknüpfung weiter OBEN im Pfad (Elternordner zeigt auf einen fremden Ordner mit kstuff.elf) */
    baue("/user/data/fremdordner/kstuff.elf", ELF);
    { char z[1024], q[1024]; snprintf(q, sizeof q, "%s/user/data/fremdordner", W); snprintf(z, sizeof z, "%s/user/data/linkeltern", W); unlink(z); if (symlink(q, z)) perror("symlink"); }
    /* Codex-Prüfung 318: ein fremder Ordner mit kstuff.elf, nach der Suche auf den gewählten Ordner /user/data/archiv eingehängt (am PC über
     * EINGEHAENGT nachgestellt - echte Einbindungen braucht root) */
    baue("/user/data/archiv/kstuff.elf", ELF);
    baue("/user/data/kstuff-sammlung/unten/kstuff.elf", ELF);
    baue("/user/data/sammlung/kstuff.elf", ELF);   /* NEU 1.3 */
    baue("/user/data/titan-core/payloads/kstuff-lite-v1.11.elf", ELF);   /* NEU 1.3 (PS5-Test 22.11.2026) */
    baue("/user/data/titan-core/config.json", "{}");
    baue("/user/data/pldmgr/payloads/local/archiv/x.txt", "x");
    baue("/user/data/zftpd/bin/zftpd.elf", ELF);
    { char z[1024]; snprintf(z, sizeof z, "%s/user/data/zftpd/bin", W); chmod(z, 0); }   /* privat wie auf der PS5 (am PC: Rechte 000) */
    setenv("EINGEHAENGT", "/user/data/archiv", 1);
    datei_optionen o = { .wurzel = W, .sortiert = true, .ordnergroesse_null = true, .test_einspielungen = true, .test_hier = "." };
    K = kern_neu(zugriff_datei(&o));
    leer();
    pruefe(!strcmp(kern_laufende_gewaehlt(K), ""), "nichts gewählt -> keines");
    fall("/user/data/homebrew/ftpsrv-ps5.elf", true, "ftpsrv,", false, "Programmdatei (bis 1.0.10 blieb ftpsrv laufen)");
    fall("/mnt/usb0/payloads/kstuff.elf", true, "kstuff,", true, "Programmdatei");
    fall("/mnt/usb0/payloads/etaHEN-2.1b.bin", true, "etahen,", true, "Programmdatei .bin");
    fall("/user/data/etaHEN", true, "etahen,", true, "ganzer Datenordner");
    fall("/user/data/etaHEN/games/Spiel - PPSA01234.ffpkg", true, "", false, "etwas IM Datenordner");
    fall("/mnt/usb0/payloads/OnionHEN.elf", true, "onionhen,", true, "onionHEN: Programmdatei (NEU 1.3)");
    fall("/user/data/OnionHEN", true, "onionhen,", true, "onionHEN: ganzer Datenordner (NEU 1.3)");
    fall("/user/data/OnionHEN/payloads/meins.elf", true, "", false, "onionHEN: eigenes Programm IN seinem Datenordner (NEU 1.3)");
    fall("/user/data/OnionHEN", false, "", false, "onionHEN: Datenordner nicht gewählt (NEU 1.3)");
    fall("/user/data/kstuff_notiz.txt", true, "", false, "Textdatei, die nur so heißt");
    fall("/user/data/kstuff-cache.bin", true, "", false, "Datendatei .bin ohne Programmkopf (Codex-Prüfung 315)");
    fall("/user/data/homebrew/kstuff.elf", true, "", false, "Verknüpfung auf eine fremde Datei - gelöscht wird nur sie (Codex-Prüfung 315)");
    fall("/user/data/linkeltern/kstuff.elf", true, "", false, "Verknüpfung im Elternpfad (Codex-Prüfung 317)");
    fall("/user/data/linkeltern", true, "", false, "der verknüpfte Ordner selbst");
    fall("/user/data/archiv", true, "", false, "eingehängter fremder Ordner mit kstuff.elf (Codex-Prüfung 318)");
    fall("/user/data/archiv/kstuff.elf", true, "", false, "Programm IN einem eingehängten Ordner");
    setenv("EINGEHAENGT", "/user/data/pldmgr/payloads/local/archiv", 1);
    fall("/user/data/pldmgr/payloads/local/archiv", true, "", false, "eingehängter Ordner im Payload Manager (Codex-Prüfung 319)");
    zugriff_als_besitzer = pc_als_besitzer;
    { int w0 = wechsel_n;
      fall("/user/data/zftpd/bin/zftpd.elf", true, "zftpd,", false, "gewähltes Programm in einem privaten Ordner (als Besitzer, Codex-Prüfung 319)");
      pruefe(wechsel_n > w0, a_fmt(K->a, "dabei als Besitzer gelesen (%d Wechsel)", wechsel_n - w0)); }
    zugriff_als_besitzer = NULL;
    /* Codex-Prüfung 320: der private Datenordner lässt sich öffnen, ist aber unbrauchbar (PS5, EBADF) - nicht prüfbar: nur die Namensregel */
    setenv("OFFEN_UNBRAUCHBAR", "zftpd", 1);
    fall("/user/data/zftpd", true, "zftpd,", false, "privater Datenordner, geöffnet aber unbrauchbar (Codex-Prüfung 320)");
    /* Codex-Prüfung 321: fremder Ordner GENAU an /user/data/zftpd eingehängt UND geöffnet unbrauchbar - die Einhängeliste sagt "fremd" */
    setenv("EINGEHAENGT", "/user/data/zftpd", 1);
    fall("/user/data/zftpd", true, "", false, "an /data/zftpd eingehängter fremder Ordner, dazu unbrauchbar (Codex-Prüfung 321)");
    { char z[1024]; snprintf(z, sizeof z, "%s/user/data/zftpd", W); chmod(z, 0);
      fall("/user/data/zftpd", true, "", false, "an /data/zftpd eingehängt und gar nicht zu öffnen (Codex-Prüfung 322)");
      chmod(z, 0755); }
    unsetenv("EINGEHAENGT");
    setenv("OFFEN_UNBRAUCHBAR", "archiv2", 1);
    baue("/user/data/homebrew/archiv2/kstuff.elf", ELF);
    fall("/user/data/homebrew/archiv2", true, "", false, "nicht prüfbarer fremder Ordner: hineingeschaut wird nicht, beendet wird nichts");
    unsetenv("OFFEN_UNBRAUCHBAR");
    setenv("EINGEHAENGT", "/user/data/kstuff-sammlung/unten", 1);
    fall("/user/data/kstuff-sammlung", true, "", false, "eingehängter Unterordner wird nicht durchsucht");
    unsetenv("EINGEHAENGT");
    fall("/user/data/kstuff-sammlung", true, "kstuff,", true, "Gegenprobe: ohne Einbindung wird das Programm darin erkannt");
    /* NEU 1.3 (PS5-Test 22.11.2026): Jailbreak-Kopie im Ordner eines anderen Programms beendet den laufenden Jailbreak nicht */
    fall("/user/data/titan-core", true, "", false, "TITAN CORE mit eigener kstuff-Kopie: Kopie geht mit, kstuff läuft weiter (NEU 1.3)");
    fall("/user/data/sammlung", true, "", false, "fremder Ordner mit kstuff.elf: Kopie dieses Ordners (NEU 1.3)");
    fall("/user/data/titan-core/payloads/kstuff-lite-v1.11.elf", true, "kstuff,", true, "dieselbe Kopie selbst angehakt: wie bisher (NEU 1.3)");
    fall("/user/data/nanodns", true, "nanodns,", false, "ganzer Datenordner");
    fall("/user/data/zftpd", true, "zftpd,", false, "ganzer Datenordner");
    fall("/user/data/pldmgr/payloads/local/x.elf", true, "pldmgr,", false, "wie 1.0.10: alles im Payload Manager");
    fall("/user/data/pldmgr/payloads/repo/ftpsrv@abc", true, "pldmgr,ftpsrv,", false, "Programmordner mit ftpsrv.elf (Codex-Prüfung 315)");
    fall("/user/data/homebrew/MeinFTP", true, "ftpsrv,", false, "Ordner, zwei Ebenen darunter ftpsrv.elf");
    fall("/user/data/homebrew/ftpsrv-ps5.elf", false, "", false, "nicht gewählt");
    fall("/user/data/homebrew/shadowmount.elf", true, "", false, "ShadowMount: eigener Ablauf, steht nie in der Liste");
    fall("/user/data/homebrew/ps5cleanup.elf", true, "", false, "PS5 Cleanup selbst nie");
    leer(); { eintrag *f = neu("/user/app/PLDM00001", true); f->KachelId = "PLDM00001"; }
    pruefe(!strcmp(kern_laufende_gewaehlt(K), "pldmgr,"), "Kachel des Payload Managers gewählt -> pldmgr");
    leer(); neu("/mnt/usb0/payloads/kstuff.elf", true); neu("/user/data/homebrew/ftpsrv-ps5.elf", true); neu("/user/data/nanodns", true);
    pruefe(!strcmp(kern_laufende_gewaehlt(K), "kstuff,nanodns,ftpsrv,"), a_fmt(K->a, "mehrere gewählt -> \"%s\"", kern_laufende_gewaehlt(K)));
    /* Codex-Prüfung 316: 3000 gewählte Dateien in EINEM Ordner - der Ordner wird nur einmal gelesen (Speicher bleibt klein) */
    for (int i = 0; i < 3000; i++) { char r[80]; snprintf(r, sizeof r, "/user/data/viele/notiz_%04d.txt", i); baue(r, "x"); }
    leer();
    for (int i = 0; i < 3000; i++) neu(a_fmt(K->a, "/user/data/viele/notiz_%04d.txt", i), true);
    { long vor = (long)basis_speicher_belegt(); const char *l3 = kern_laufende_gewaehlt(K); long nach = (long)basis_speicher_belegt();
      pruefe(!strcmp(l3, "") && (nach - vor) < 64L * 1024 * 1024, a_fmt(K->a, "3000 Dateien in einem Ordner: keines, Speicher +%ld KiB", (nach - vor) / 1024)); }
    { char z[1024]; snprintf(z, sizeof z, "%s/user/data/zftpd/bin", W); chmod(z, 0755); }   /* aufräumbar lassen */
    printf("FEHLERZAHL %d\n", fehler);
    return fehler ? 1 : 0;
}
