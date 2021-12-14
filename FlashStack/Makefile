PROGNAME    = afl
VERSION     = $(shell grep '^\#define VERSION ' config.h | cut -d '"' -f2)
PROGS       = afl-gcc afl-as spa-rustc
#SPA_LIBS    = fork.so rt_lib.so libfsgs.so
SPA_LIBS    = fork.so rt_lib.so libfsgs.so libgsrsp.so

CFLAGS     ?= -O3 -funroll-loops
CFLAGS     += -Wall -DSPA_CUR_WORK_DIR=\"$(shell pwd)\" -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign \
	      -DAFL_PATH=\"$(HELPER_PATH)\" -DDOC_PATH=\"$(DOC_PATH)\" \
	      -DBIN_PATH=\"$(BIN_PATH)\" -Wno-unused-function

ifneq "$(filter Linux GNU%,$(shell uname))" ""
  LDFLAGS  += -ldl
endif

ifeq "$(findstring clang, $(shell $(CC) --version 2>/dev/null))" ""
  TEST_CC   = afl-gcc
else
  TEST_CC   = afl-clang
endif

COMM_HDR    = alloc-inl.h config.h debug.h types.h spa.h
SPA_COMM	= init.c buddy_tls.c weak_stack_size.s util.c


all: $(PROGS) $(SPA_LIBS)


afl-gcc: afl-gcc.c $(COMM_HDR) 
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)
	#set -e; for i in clang clang++ cc c++ gcc g++ afl-g++ afl-clang afl-clang++; do ln -sf afl-gcc $$i; done
	set -e; for i in afl-g++ afl-clang afl-clang++ spa-clang spa-clang++; do ln -sf afl-gcc $$i; done


afl-as: afl-as.c afl-as.h $(COMM_HDR)
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)
	ln -sf afl-as as

spa-rustc: afl-rustc.c $(COMM_HDR)
	gcc afl-rustc.c -o spa-rustc
	ln -sf spa-rustc rustc 

fork.so: fork.c $(COMM_HDR) weak_stack_size.s 
	gcc -D_GNU_SOURCE -fPIC -shared -Wl,--dynamic-list="$(shell pwd)/dynamic_symbol_table.txt" fork.c weak_stack_size.s -o fork.so -ldl -lpthread

libfsgs.so: fsgs.c $(COMM_HDR) rt_lib.c util.c	
	gcc -O3 -D_GNU_SOURCE -fPIC -shared -mavx2  fsgs.c rt_lib.c util.c -o libfsgs.so -lpthread -ldl

# we want to add the .BUDDY.CALL_STACK_SIZE_MASK into the rt_lib.so
# such that "LD_PRELOAD=/home/iron/src/SPA/rt_lib.so python3" will succeed.
# -fno-omit-frame-pointer
rt_lib.so: $(SPA_COMM) simd_rand.c $(COMM_HDR) afl-gcc	rt_lib.c
	gcc -fPIC -mavx2 -O3 -c simd_rand.c -o simd_rand.o 
	gcc -D_GNU_SOURCE -O3 -c rt_lib.c -o rt_lib.o 
	gcc -D_GNU_SOURCE -fPIC -shared $(SPA_COMM) simd_rand.o rt_lib.o -o rt_lib.so -ldl -lpthread -lrt	
	ln -sf rt_lib.so libbustk.so


libgsrsp.so: gs.rsp.c $(COMM_HDR) rt_lib.c util.c	
	gcc -O3 -D_GNU_SOURCE -fPIC -shared -mavx2  gs.rsp.c rt_lib.c util.c -o libgsrsp.so -lpthread -ldl



.NOTPARALLEL: clean

clean:
	rm -f $(PROGS) test_tls rustc spa-rustc afl-as as clang clang++ cc c++ gcc g++ afl-g++ afl-clang afl-clang++ spa-clang spa-clang++ *.so *.spa.o *.o *~ a.out 














































