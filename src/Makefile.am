AUTOMAKE_OPTIONS = foreign

distdir = ../$(PACKAGE)-$(VERSION)/src
dist_HEADERS = fparser_private.h

BUILT_SOURCES = diag.c diag.h
EXTRA_DIST = diag.txt gen-diag
CLEANFILES = $(BUILT_SOURCES) *.core
#CLEANFILES += *.in

lib_LTLIBRARIES = libmrklkit.la

nobase_include_HEADERS = \
	mrklkit/fparser.h \
	mrklkit/lexpr.h \
	mrklkit/lparse.h \
	mrklkit/ltype.h \
	mrklkit/lruntime.h \
	mrklkit/module.h \
	mrklkit/mrklkit.h \
	mrklkit/builtin.h \
	mrklkit/util.h \
	mrklkit/modules/testrt.h \
	mrklkit/modules/dparser.h \
	mrklkit/dsource.h

noinst_HEADERS =

libmrklkit_la_SOURCES = \
	fparser.c \
	lexpr.c \
	lparse.c \
	ltype.c \
	ltypegen.c \
	lruntime.c \
	mrklkit.c \
	builtin.c \
	util.c \
	modules/dparser.c \
	modules/testrt.c \
	dsource.c

nodist_libmrklkit_la_SOURCES = diag.c

if DEBUG
DEBUG_FLAGS = -g -O0
#DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

libmrklkit_la_CFLAGS = -Wall -Wextra -Werror -std=c99 -I$(includedir) @LLVM_CFLAGS@ $(DEBUG_FLAGS)

libmrklkit_la_LDFLAGS = -version-info 0:0:0 -L$(libdir) @LLVM_LDFLAGS@ -lc++ @LLVM_LIBS@ -lmrkcommon

SUBDIRS = . test

diag.c diag.h: diag.txt
	sh ./gen-diag

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
