AUTOMAKE_OPTIONS = foreign
CLEANFILES = *.core
#CLEANFILES += *.in

noinst_PROGRAMS=testfoo testfparser testdparser testlk

distdir = ../../$(PACKAGE)-$(VERSION)/src/test
dist_HEADERS = unittest.h

BUILT_SOURCES = ../diag.c ../diag.h

noinst_HEADERS = ../mrklkit/mrklkit.h

if DEBUG
DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

nodist_testfoo_SOURCES = ../diag.c
testfoo_SOURCES = testfoo.c
testfoo_CFLAGS = -Wall -Wextra -Werror -std=c99 -I.. -I$(includedir) @MRKCOMMON_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS)
testfoo_LDFLAGS = -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testfparser_SOURCES = ../diag.c
testfparser_SOURCES = testfparser.c
testfparser_CFLAGS = -Wall -Wextra -Werror -std=c99 -I.. -I$(includedir) @MRKCOMMON_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS)
testfparser_LDFLAGS = -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testdparser_SOURCES = ../diag.c
testdparser_SOURCES = testdparser.c
testdparser_CFLAGS = -Wall -Wextra -Werror -std=c99 -I.. -I$(includedir) @MRKCOMMON_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS)
testdparser_LDFLAGS =  -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testlk_SOURCES = ../diag.c
testlk_SOURCES = testlk.c
testlk_CFLAGS = -Wall -Wextra -Werror -std=c99 -I.. -I$(includedir) @MRKCOMMON_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS)
testlk_LDFLAGS =  -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

../diag.c ../diag.h: ../diag.txt
	sh ../gen-diag

testrun: all
	for i in $(noinst_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;
