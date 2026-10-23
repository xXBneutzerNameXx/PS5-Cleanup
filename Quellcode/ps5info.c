/* ps5info - fragt die Modellnummer der PS5 und den Kopplungs-Status des
 * abnehmbaren Laufwerks ab und beendet sich. Es wird nur GELESEN.
 * Ausgabe: auf stdout (fuer den Payload-Lader an Port 9021) und zusaetzlich
 * in die Datei /data/ps5info.txt (fuer Lader ohne Rueckkanal, z.B. den
 * Payload-Manager). Das Skript liest die Datei per FTP und loescht sie wieder.
 * Modell nach PS5Upload (hw_info.c) und drakmor/ps5-hwinfo (GPL-3.0),
 * Laufwerks-Status nach earthonion/ddd_status (GPL-3.0): ein ioctl auf
 * /dev/ddd liefert eine Statuszahl (0..6), ob das Laufwerk gekoppelt ist.
 * Lizenz: GPL-3.0. */
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ps5/kernel.h>

int sceKernelGetHwModelName(char *out);

/* ioctl-Nummer und Puffer-Struktur des Laufwerks-Treibers /dev/ddd. */
#define DDD_STATUS_IOCTL 0xC0284406UL

typedef struct {
  uint8_t   vorlauf[4];
  int32_t   ein_laenge;
  uint8_t  *ein_puffer;
  int32_t   aus_laenge;
  uint8_t  *aus_puffer;
  uint32_t  status;
  uint32_t  nachlauf;
} ddd_abfrage_t;

static sigjmp_buf g_jb;

static void on_fault(int sig) {
  (void)sig;
  siglongjmp(g_jb, 1);
}

/* Hebt die eigenen Rechte so weit an, dass /dev/ddd geoeffnet werden darf. */
static void rechte_anheben(void) {
  uint8_t caps[16];
  pid_t pid = getpid();
  kernel_set_ucred_authid(pid, 0x4800000000000010ULL);
  if (kernel_get_ucred_caps(pid, caps) == 0) {
    caps[7] |= 0x40;
    kernel_set_ucred_caps(pid, caps);
  }
  setuid(0);
}

/* Liefert die Statuszahl (0..6) oder -1, wenn kein abnehmbares Laufwerk da ist
 * (erste PS5: fest verbaut) bzw. die Abfrage scheitert. Nur lesend. */
static int laufwerk_status(void) {
  int fd = open("/dev/ddd", O_RDWR);
  if (fd < 0) {
    return -1;
  }
  ddd_abfrage_t a;
  uint32_t aus = 0;
  memset(&a, 0, sizeof a);
  a.aus_laenge = (int32_t)sizeof aus;
  a.aus_puffer = (uint8_t *)&aus;
  int r = ioctl(fd, DDD_STATUS_IOCTL, &a);
  close(fd);
  if (r != 0) {
    return -1;
  }
  return (int)aus;
}

int main(void) {
  char model[256];
  struct sigaction sa;
  int status = -1;
  FILE *f;

  memset(model, 0, sizeof model);
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = on_fault;
  sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGBUS, &sa, NULL);

  if (sigsetjmp(g_jb, 1) == 0) {
    if (sceKernelGetHwModelName(model) != 0) {
      model[0] = '\0';
    }
  } else {
    model[0] = '\0';
  }
  model[sizeof model - 1] = '\0';

  /* Laufwerks-Status eigener Try-Block: scheitert er, bleibt es bei -1. */
  if (sigsetjmp(g_jb, 1) == 0) {
    rechte_anheben();
    status = laufwerk_status();
  } else {
    status = -1;
  }

  printf("PS5INFO_MODEL=%s\n", model);
  printf("PS5INFO_DDD=%d\n", status);
  fflush(stdout);

  f = fopen("/data/ps5info.txt", "w");
  if (f) {
    fprintf(f, "PS5INFO_MODEL=%s\n", model);
    fprintf(f, "PS5INFO_DDD=%d\n", status);
    fclose(f);
  }
  return 0;
}
