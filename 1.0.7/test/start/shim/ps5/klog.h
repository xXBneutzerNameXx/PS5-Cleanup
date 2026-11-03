/* PC-Test: Kernel-Protokoll -> Standardausgabe */
#pragma once
#include <stdio.h>
#define klog_printf(...) printf("KLOG " __VA_ARGS__)
