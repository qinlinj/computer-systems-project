#
# Student makefile for Cache Lab
#

CC = clang
CLANG_FORMAT = clang-format
LLVM_LINK = llvm-link
LLVM_OPT = opt

COPT = -O1
CFLAGS = -std=c99 $(COPT) -g -Wall -Wextra -Wpedantic -Wconversion -gdwarf-4
CFLAGS += -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter -Werror -fno-unroll-loops

HANDIN_TAR = cachelab-handin.tar
FILES = test-csim csim test-trans test-trans-simple tracegen-ct

all: $(FILES)
.PHONY: all

csim: csim.o cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-csim: test-csim.o cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-trans: test-trans.o trans.o cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test-trans-simple: test-trans-simple.o trans-san.o cachelab-san.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tracegen-ct: LDFLAGS += -pthread
tracegen-ct: trans-fin.o tracegen-ct.o cachelab.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# this is an easy mistake for students to make, and the built-in %:%.c rule
# does something extra unhelpful with it
.PHONY: trans
trans:
	@printf '%s\n' >&2 \
	  "Makefile: The program compiled from trans.c is 'test-trans'." \
	  "Makefile: Use 'make test-trans', not 'make trans'."; \
	exit 1

# Header file dependencies
cachelab.o: cachelab.c cachelab.h
cachelab-san.o: cachelab.c cachelab.h
csim.o: csim.c cachelab.h
test-csim.o: test-csim.c cachelab.h
test-trans.o: test-trans.c cachelab.h
test-trans-simple.o: test-trans-simple.c cachelab.h
tracegen-ct.o: tracegen-ct.c cachelab.h
trans.o: trans.c cachelab.h
trans-san.o: trans.c cachelab.h

# Compile certain targets with sanitizers
%-san.o: %.c
	$(COMPILE.c) -o $@ $<

SAN_FLAGS = -fsanitize=integer,alignment,bounds,address
SAN_FLAGS += -fno-sanitize-recover=bounds
cachelab-san.o trans-san.o: CFLAGS += $(SAN_FLAGS)
test-trans-simple: LDFLAGS += $(SAN_FLAGS)

# Compile tracegen-ct using custom CT instrumentation
%.o: %.bc
	$(CC) $(CFLAGS) -c -o $@ $<

trans-fin.bc: trans-ct.bc ct/ct.bc
	$(LLVM_LINK) -o $@ $^

trans-ct.bc: trans.ll ct/CLabInst.so
	$(LLVM_OPT) -enable-new-pm=0 -load=ct/CLabInst.so -CLabInst -o $@ $<

trans.ll: trans.c cachelab.h
	$(CC) $(CFLAGS) -emit-llvm -S -o $@ $<

trans-check.ll: trans.c cachelab.h
	$(CC) $(CFLAGS) -emit-llvm -S -o $@ $<

tracegen-ct.o: COPT = -O3
trans-fin.o: COPT = -O3 -fno-unroll-loops
trans-fin.o: CFLAGS += -DNDEBUG
trans.ll: COPT = -O3
trans-check.ll: COPT = -O0

# Also put trans.c through some custom checks.
trans-check.bc: trans-check.ll ct/Check.so
	$(LLVM_OPT) -enable-new-pm=0 -load=ct/Check.so -Check -o $@ $<
all: trans-check.bc

.PHONY: clean
clean:
	-rm -f *.tar *~ *.o *.bc *.ll
	-rm -f $(FILES)
	-rm -f trace.all trace.f*
	-rm -f .csim_results .marker .format-checked

# Include rules for submit, format, etc
FORMAT_FILES = csim.c trans.c
HANDIN_FILES = csim.c trans.c \
    .clang-format \
    .format-checked \
    traces/traces/tr1.trace \
    traces/traces/tr2.trace \
    traces/traces/tr3.trace
HANDOUT_SCRIPTS = \
    ct/CLabInst.so \
    ct/Check.so \
    csim-ref \
    driver.py \
    traces-driver.py
include helper.mk
