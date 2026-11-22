/* PS5 Cleanup 1.0.7 - Schreibsperre für alle Test-, Simulations- und Prüffassungen
 * Copyright (C) 2026 da_Chris86 - Lizenz: GPL-3.0-or-later
 *
 * da_Chris86 31.10.2026 21:50 (Punkt 5): "Stelle sicher, dass die Test-ELF wirklich ausschließlich lesend arbeitet,
 * einschließlich aller Hilfsfunktionen." Diese Datei wird beim Übersetzen jeder Datei VOR allem anderen eingebunden
 * (-include bau/nurlesen.h, nur in Bauten OHNE KERN_LOESCHEN). Danach sind alle schreibenden, löschenden, umbenennenden,
 * einhängenden und Rechte ändernden Funktionen "vergiftet": steht eine davon im übersetzten Programmtext, bricht der
 * Compiler ab. Zusätzlich prüfen die Bau-Skripte die fertigen Objektdateien (nm) und alle fopen-Aufrufe (Lesemodus).
 * ("link" ist nicht vergiftet, weil es im Kern ein Feldname ist - link() prüft das Bau-Skript über nm.)
 * open() selbst bleibt erlaubt (zum Lesen nötig) - die Schreib-Kennzeichen O_WRONLY/O_RDWR/O_CREAT/O_TRUNC/O_APPEND
 * sind ebenfalls vergiftet, damit open() nur lesend benutzt werden kann.
 */
#ifndef PS5C_NURLESEN_H
#define PS5C_NURLESEN_H
#ifdef KERN_LOESCHEN
#error "nurlesen.h gehört nur in Bauten OHNE KERN_LOESCHEN"
#endif
/* zuerst alle Systemköpfe, die die vergifteten Namen selbst erwähnen */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#ifndef NURLESEN_OHNE_UNISTD
#include <unistd.h>
#endif
#include <dirent.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#ifdef __linux__
#include <sys/mount.h>
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/uio.h>
#endif
#ifdef NURLESEN_MIT_SDL
#include <SDL2/SDL.h>
#endif
/* Makros gleichen Namens zuerst entfernen (sonst warnt der Compiler beim Vergiften) */
#undef putc
#undef putw
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_TRUNC
#undef O_APPEND
#pragma GCC poison fwrite fputs fputc putc putw fprintf vfprintf freopen tmpfile tmpnam mkstemp mkdtemp
#pragma GCC poison remove rename renameat unlink unlinkat rmdir mkdir mkdirat mkfifo mknod
#pragma GCC poison chmod fchmod fchmodat lchmod chown fchown lchown chflags fchflags lchflags
#pragma GCC poison truncate ftruncate linkat symlink symlinkat utimes futimes lutimes utimensat futimens
#pragma GCC poison write pwrite writev pwritev sendfile
#pragma GCC poison nmount mount unmount umount umount2
#pragma GCC poison O_WRONLY O_RDWR O_CREAT O_TRUNC O_APPEND
#pragma GCC poison reboot sceSystemServiceRequestReboot sceAppInstUtilAppUnInstall sceAppInstUtilAppInstallAll sceAppInstUtilAppInstallTitleDir
#endif
