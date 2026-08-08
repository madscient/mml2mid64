# Makefile for POSIX systems (Linux, macOS, *BSD) and MinGW.
#
# There is also a CMake build in the parent directory, which additionally
# supports MSVC and runs the sample regression tests:
#
#     cmake -S . -B build && cmake --build build && ctest --test-dir build
#
# Platform selection is automatic now; no -DUNIX is needed.

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man
MANJP   = ja
CONFIGS =

CC      = cc
# gnu11, not c11: glibc hides the POSIX declarations this program needs under
# a strict -std=c11.  compat.h sets the feature-test macros too, so a strict
# build works as well; this just avoids surprises.
CSTD    = -std=gnu11
WARN    = -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
CFLAGS  = -O2 $(CSTD) $(WARN) -finput-charset=UTF-8 $(CONFIGS)
LDFLAGS =
LIBS    =

INSTALL = install

EXE  = mml2mid
OBJS = file.o charproc.o macroexp.o dbgmap.o note.o mmlproc.o mml2mid.o
HDRS = compat.h file.h charproc.h macroexp.h dbgmap.h mmlproc.h mml2mid.h

all : $(EXE)

$(EXE) : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# Every object depends on every header; the program is small enough that the
# extra rebuilding costs nothing and the rules stay correct.
$(OBJS) : $(HDRS)

.c.o :
	$(CC) $(CFLAGS) -c $<

install : all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(EXE) $(DESTDIR)$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/$(MANJP)/man1
	$(INSTALL) -m 644 mml2mid.1 $(DESTDIR)$(MANDIR)/$(MANJP)/man1

# Compile every sample and compare against test/baseline.sha256.
# See test/README.md for how that baseline was validated.
# run-trackname.sh and run-regress.sh need no sample/, so they always run.
check : all
	@sh ../test/run-trackname.sh ./$(EXE)
	@sh ../test/run-regress.sh ./$(EXE)
	@sh ../test/run-baseline.sh ./$(EXE)

clean :
	rm -f $(EXE) $(OBJS) core

.PHONY : all install check clean
.SUFFIXES : .c .o
