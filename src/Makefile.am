AUTOMAKE_OPTIONS = foreign subdir-objects

distdir = ../$(PACKAGE)-$(VERSION)/src
dist_HEADERS = fparser_private.h

BUILT_SOURCES = diag.c diag.h
EXTRA_DIST = diag.txt gen-diag
CLEANFILES = $(BUILT_SOURCES) *.core
#CLEANFILES += *.in

lib_LTLIBRARIES = libmrklkit.la

nobase_include_HEADERS = \
	mrklkit/fparser.h \
	mrklkit/dparser.h \
	mrklkit/lexpr.h \
	mrklkit/lparse.h \
	mrklkit/ltype.h \
	mrklkit/ltypegen.h \
	mrklkit/lruntime.h \
	mrklkit/module.h \
	mrklkit/mrklkit.h \
	mrklkit/builtin.h \
	mrklkit/util.h \
	mrklkit/modules/testrt.h

noinst_HEADERS =

libmrklkit_la_SOURCES = \
	fparser.c \
	dparser.c \
	lexpr.c \
	lparse.c \
	ltype.c \
	ltypegen.c \
	lruntime.c \
	mrklkit.c \
	builtin.c \
	builtingen.c \
	util.c \
	modules/testrt.c

nodist_libmrklkit_la_SOURCES = diag.c

if DEBUG
DEBUG_FLAGS = -g -O0
#DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

libmrklkit_la_CFLAGS = -Wall -Wextra -Werror -std=c99 -I$(includedir) @MRKCOMMON_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS)

libmrklkit_la_LDFLAGS = -version-info 0:0:0 -L$(libdir) -lbz2 -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

SUBDIRS = .

diag.c diag.h: diag.txt
	/bin/sh ./gen-diag

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
