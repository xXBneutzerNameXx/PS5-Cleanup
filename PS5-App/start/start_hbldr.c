/* PS5 Cleanup 1.0.6 - start_hbldr.c: Vollbild-Start (Weg A), abgeleitet aus websrv src/ps5/hbldr.c
 * (ps5-payload-dev/websrv, Commit 7748df9, Copyright (C) 2024 John Törnblom, GPL-3.0-or-later).
 * Änderungen (da_Chris86, 2026, GPL-3.0-or-later):
 *  - NEU 1.0.6 (Freigabe da_Chris86 29.10.2026 21:23): statt der fremden Platzhalter-App FAKE00000 der EIGENE
 *    Startordner /system_ex/app/PSCL00002 (startordner.h - wie remount_system_ex()/fakeapp_create_if_missing(),
 *    aber nur der eigene Ordner, Einhängezustand danach wiederhergestellt). FAKE00000 wird nicht mehr benutzt.
 *  - Die Oberfläche wird aus dem Speicher geladen (eingebettet), nicht aus einer Datei.
 *  - kein websrv-Kopf (sys.h, websrv.h, fs.h) mehr nötig.
 * Original-Hinweis:
 * Copyright (C) 2024 John Törnblom

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <ps5/kernel.h>
#include <ps5/klog.h>
#include <time.h>

#include "elfldr.h"
#include "start_hbldr.h"
#include "pt.h"


#include "startordner.h"

#define IOVEC_ENTRY(x) {x ? x : 0, \
			x ? strlen(x)+1 : 0}
#define IOVEC_SIZE(x) (sizeof(x) / sizeof(struct iovec))


typedef struct app_launch_ctx {
  uint32_t structsize;
  uint32_t user_id;
  uint32_t app_opt;
  uint64_t crash_report;
  uint32_t check_flag;
} app_launch_ctx_t;


int sceUserServiceInitialize(void*);
int sceUserServiceGetForegroundUser(uint32_t *user_id);
int sceUserServiceTerminate(void);

int sceSystemServiceGetAppIdOfRunningBigApp(void);
int sceSystemServiceKillApp(int app_id, int how, int reason, int core_dump);
int sceSystemServiceLaunchApp(const char* title_id, char** argv,
			      app_launch_ctx_t* ctx);

int sceKernelGetAppState(int app_id, int*, int*);


/* NEU 1.0.6: eigenen Startordner bereitstellen (startordner.h). 0 = bereit; text: Meldung (auch bei Erfolg mit Hinweis) */
int
start_startordner(char *text, size_t text_len, int *umgehaengt) {
  return so_einrichten(text, text_len, umgehaengt);
}


/**
 * Get the pid of a process with the given name.
 **/
static int find_fehler = 0;   /* NEU 1.0.6 (Codex-Prüfung 26): Prozessliste nicht lesbar */
static pid_t
find_pid(const char* name) {
  int mib[4] = {1, 14, 8, 0};
  pid_t mypid = getpid();
  pid_t pid = -1;
  size_t buf_size;
  uint8_t *buf;

  find_fehler = 0;
  if(sysctl(mib, 4, 0, &buf_size, 0, 0)) {
    perror("sysctl");
    find_fehler = 1;
    return -1;
  }

  if(!(buf=malloc(buf_size))) {
    perror("malloc");
    find_fehler = 1;
    return -1;
  }

  if(sysctl(mib, 4, buf, &buf_size, 0, 0)) {
    perror("sysctl");
    free(buf);
    find_fehler = 1;
    return -1;
  }

  for(uint8_t *ptr=buf; ptr<(buf+buf_size);) {
    int ki_structsize = *(int*)ptr;
    pid_t ki_pid = *(pid_t*)&ptr[72];
    char *ki_tdname = (char*)&ptr[447];

    ptr += ki_structsize;
    if(!strcmp(name, ki_tdname) && mypid != ki_pid) {
      pid = ki_pid;
    }
  }

  free(buf);

  return pid;
}


/**
 *
 **/
static void*
bigapp_launch_thread(void* arg) {
  char** argv = (char**)arg;
  app_launch_ctx_t ctx = {0};

  if(sceUserServiceGetForegroundUser(&ctx.user_id)) {
    return 0;
  }

  int e = sceSystemServiceLaunchApp(SO_ID, argv, &ctx);   /* NEU 1.0.6: eigener Startordner statt FAKE00000 */
  klog_printf("PS5 Cleanup: sceSystemServiceLaunchApp(%s) -> 0x%08X\n", SO_ID, (unsigned)e);

  return 0;
}


/**
 *
 **/
static pid_t
bigapp_launch(char** argv) {
  struct kevent evt = {0};
  pid_t parent = -1;
  pid_t child = -1;
  pthread_t trd;
  int kq = -1;

  if((parent=find_pid("SceSysCore.elf")) < 0) {
    perror("findpid");
    return -1;
  }

  if((kq=kqueue()) < 0) {
    perror("kqueue");
    return -1;
  }

  EV_SET(&evt, parent, EVFILT_PROC, EV_ADD | EV_ENABLE | EV_CLEAR,
	 NOTE_FORK | NOTE_EXEC | NOTE_TRACK, 0, 0);
  if(kevent(kq, &evt, 1, 0, 0, 0) < 0) {
    perror("kevent");
    close(kq);
    return -1;
  }

  pthread_create(&trd, 0, &bigapp_launch_thread, (void*)argv);
  pthread_detach(trd);

  {   /* NEU 1.0.6: höchstens 20 s auf den Start warten - startet Sony die App nicht, nicht ewig hängen */
    struct timespec bis = { 20, 0 };
    int n = kevent(kq, 0, 0, &evt, 1, &bis);
    if(n <= 0) {
      klog_printf("PS5 Cleanup: App-Start %s\n", n == 0 ? "- keine Rückmeldung nach 20 s" : "- kevent-Fehler");
      close(kq);
      return -1;
    }
  }

  close(kq);

  if(!(evt.fflags & NOTE_CHILD)) {
    return -1;
  }

  child = evt.ident;

  if(pt_attach(child)) {
    perror("pt_attach");
    return -1;
  }

  if(pt_follow_exec(child) < 0) {
    perror("pt_follow_exec");
    pt_detach(child, SIGKILL);
    return -1;
  }

  if(pt_continue(child, SIGCONT) < 0) {
    perror("pt_continue");
    pt_detach(child, SIGKILL);
    return -1;
  }

  if(pt_await_exec(child)) {
    perror("pt_await_exec");
    pt_detach(child, SIGKILL);
    return -1;
  }

  return child;
}


/**
 *
 **/
static int
bigapp_set_argv0(pid_t pid, const char* argv0) {
  intptr_t pos = pt_getargv(pid);
  intptr_t buf = 0;

  // allocate memory
  if((buf=pt_mmap(pid, 0, PAGE_SIZE, PROT_WRITE | PROT_READ,
		  MAP_ANONYMOUS | MAP_PRIVATE,
		  -1, 0)) == -1) {
    pt_perror(pid, "pt_mmap");
    return -1;
  }

  // copy string
  if(pt_copyin(pid, argv0, buf, strlen(argv0)+1)) {
    perror("pt_copyin");
    pt_munmap(pid, buf, PAGE_SIZE);
    return -1;
  }

  // copy pointer to string
  if(pt_setlong(pid, pos, buf)) {
    perror("pt_setlong");
    pt_munmap(pid, buf, PAGE_SIZE);
    return -1;
  }

  return 0;
}


/**
 *
 **/
static pid_t
bigapp_replace(pid_t pid, uint8_t* elf, const char* progname, int stdio,
	       const char* cwd, char** envp) {
  uint8_t int3instr = 0xcc;
  intptr_t brkpoint;
  uint8_t orginstr;

  // Let the kernel assign process parameters accessed via sceKernelGetProcParam()
  if(pt_syscall(pid, 599)) {
    puts("sys_dynlib_process_needed_and_relocate failed");
    //return -1;
  }

  // Allow libc to allocate arbitrary amount of memory.
  elfldr_set_heap_size(pid, -1);

  if(!(brkpoint=kernel_dynlib_entry_addr(pid, 0))) {
    puts("kernel_dynlib_entry_addr failed");
    return -1;
  }
  brkpoint += 58;// offset to invocation of main()

  if(kernel_mprotect(pid, brkpoint, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC)) {
    puts("kernel_mprotect failed");
    pt_detach(pid, SIGKILL);
    return -1;
  }

  if(pt_copyout(pid, brkpoint, &orginstr, sizeof(orginstr))) {
    perror("pt_copyout");
    return -1;
  }
  if(pt_copyin(pid, &int3instr, brkpoint, sizeof(int3instr))) {
    perror("pt_copyin");
    return -1;
  }

  // Continue execution until we hit the breakpoint, then remove it.
  if(pt_continue(pid, SIGCONT)) {
    perror("pt_continue");
    return -1;
  }
  if(waitpid(pid, 0, 0) == -1) {
    perror("waitpid");
    return -1;
  }
  if(pt_copyin(pid, &orginstr, brkpoint, sizeof(orginstr))) {
    perror("pt_copyin");
    return -1;
  }

  bigapp_set_argv0(pid, progname);
  elfldr_raise_privileges(pid);
  elfldr_set_procname(pid, basename(progname));
  elfldr_set_environ(pid, envp);
  elfldr_set_cwd(pid, cwd);
  elfldr_set_stdio(pid, stdio);

  // invoke sys_set_butget(0). This allows bigapp homebrew to operate on
  // relative paths using open, mkdir, etc. TODO: On older firmwares, this
  // causes a kernel panic when the bigapp is restarted. Figure out what
  // firmwares this issue affect, and how to address it.
  if(kernel_get_fw_version() > 0x10000000) {
    pt_syscall(pid, 0x23b, 0);
  }

  // Execute the ELF
  if(elfldr_exec(pid, elf)) {
    return -1;
  }

  return pid;
}


pid_t
start_hbldr_launch(const char* cwd, const char* progname, uint8_t* elf, int stdio,
		   char** argv, char** envp) {
  int app_id;
  pid_t pid;

  if(so_zustand() != 1) {   /* NEU 1.0.6: nur aus dem eigenen, vollständigen Startordner starten */
    puts("Startordner " SO_DIR " fehlt oder ist nicht eindeutig der eigene");
    return -1;
  }

  if((app_id=sceSystemServiceGetAppIdOfRunningBigApp()) > 0) {
    if(sceSystemServiceKillApp(app_id, -1, 0, 0)) {
      perror("sceSystemServiceKillApp");
      return -1;
    }

    while(!sceKernelGetAppState(app_id, 0, 0)) {
      printf("Waiting for App with id 0x%x to terminate\n", app_id);
      sleep(1);
    }
  }

  if((pid=bigapp_launch(argv)) < 0) {
    return -1;
  }

  if(bigapp_replace(pid, elf, progname, stdio, cwd, envp) < 0) {
    pt_detach(pid, SIGKILL);
    pid = -1;
  }

  return pid;
}


/* NEU 1.0.6 (PS5 Cleanup): nur nachsehen - läuft ein Prozess mit diesem Namen? -1 = nein */
pid_t
start_hbldr_find_pid(const char* name) {
  pid_t p = find_pid(name);
  return (p < 0 && find_fehler) ? -2 : p;   /* -2 = unbekannt (Liste nicht lesbar) */
}
