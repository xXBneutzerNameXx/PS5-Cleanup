/* PC-Test für start/selbstweg.c: /system_ex-Einhängezustand und nmount nachgebildet (kein echtes Einhängen) */
#pragma once
#include <sys/statfs.h>
#include <sys/uio.h>
#define MNT_RDONLY 1
#define MNT_UPDATE 0x10000
extern int TEST_RO, TEST_NMOUNT;
int nmount(struct iovec *iov, unsigned n, int flags);
int test_statfs(const char *p, struct statfs *s);
#define statfs(p, s) test_statfs(p, s)
