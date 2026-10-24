# ps5info.mk - baut ps5info.elf mit dem ps5-payload-sdk (John Törnblom)
# Aufruf: make -f ps5info.mk PS5_PAYLOAD_SDK=/pfad/zum/ps5-payload-sdk
# Lizenz: GPL-3.0 (siehe LICENSE_GPL-3.0_ps5info.txt)
include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2
LDLIBS := -lkernel_sys
ps5info.elf: ps5info.c ps5info_antwort.h
	$(CC) $(CFLAGS) -o $@ ps5info.c $(LDLIBS)
	$(PS5_PAYLOAD_SDK)/bin/prospero-strip --strip-all $@
