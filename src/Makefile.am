diags = diag.txt
BUILT_SOURCES = diag.c diag.h
EXTRA_DIST = $(diags)
CLEANFILES = $(BUILT_SOURCES) *.core
#CLEANFILES += *.in

lib_LTLIBRARIES = libmrklkit.la

bin_PROGRAMS = mrklkit-config

nobase_include_HEADERS = \
	mrklkit/builtin.h \
	mrklkit/dparser.h \
	mrklkit/dpexpr.h \
	mrklkit/fparser.h \
	mrklkit/lexpr.h \
	mrklkit/lparse.h \
	mrklkit/lruntime.h \
	mrklkit/ltype.h \
	mrklkit/ltypegen.h \
	mrklkit/module.h \
	mrklkit/mrklkit.h \
	mrklkit/util.h \
	mrklkit/modules/testrt.h

noinst_HEADERS = fparser_private.h

libmrklkit_la_SOURCES = \
	builtin.c \
	builtingen.c \
	dparser.c \
	dpexpr.c \
	fparser.c \
	lexpr.c \
	lparse.c \
	lruntime.c \
	ltype.c \
	ltypegen.c \
	mrklkit.c \
	util.c \
	modules/testrt.c

nodist_libmrklkit_la_SOURCES = diag.c

mrklkit_config_SOURCES = mrklkit-config.c

if DEBUG
DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

libmrklkit_la_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I$(includedir)
libmrklkit_la_LDFLAGS = -version-info 0:0:0 -L$(libdir) -lmrkcommon -lmndiag -lbz2 @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

mrklkit_config_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I$(includedir) -DMRKLKIT_CONFIG_INCLUDES="\"$(CFLAGS) $(LLVM_CFLAGS)\"" -DMRKLKIT_CONFIG_LIBS="\"$(LIBS)\""
mrklkit_config_LDFLAGS = -version-info 0:0:0 -L$(libdir) -lmrkcommon -lmndiag -lbz2 @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

SUBDIRS = .

diag.c diag.h: $(diags)
	$(AM_V_GEN) cat $(diags) | sort -u >diag.txt.tmp && mndiagen -v -S diag.txt.tmp -L mrklkit -H diag.h -C diag.c *.[ch] modules/*.[ch]

run: all
	for i in $(bin_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;

testrun:
	for i in $(SUBDIRS); do if test "$$i" != "."; then cd $$i && $(MAKE) testrun && cd ..; fi; done;
