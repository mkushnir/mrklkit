AUTOMAKE_OPTIONS = foreign subdir-objects
CLEANFILES = *.core
#CLEANFILES += *.in

noinst_PROGRAMS=testfoo testfparser testdparser testlk testminmodule



BUILT_SOURCES = ../src/diag.c ../src/diag.h

noinst_HEADERS = unittest.h ../src/mrklkit/mrklkit.h ../src/mrklkit/util.h

if DEBUG
DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

nodist_testfoo_SOURCES = ../src/diag.c
testfoo_SOURCES = testfoo.c
testfoo_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testfoo_LDFLAGS = -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testfparser_SOURCES = ../src/diag.c
testfparser_SOURCES = testfparser.c
testfparser_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testfparser_LDFLAGS = -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testdparser_SOURCES = ../src/diag.c
testdparser_SOURCES = testdparser.c
testdparser_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testdparser_LDFLAGS =  -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testlk_SOURCES = ../src/diag.c
testlk_SOURCES = testlk.c
testlk_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testlk_LDFLAGS =  -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testminmodule_SOURCES = ../src/diag.c
testminmodule_SOURCES = testminmodule.c
testminmodule_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testminmodule_LDFLAGS =  -L$(libdir) -lmrkcommon @MRKCOMMON_LOCAL_LDFLAGS@ -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

../src/diag.c ../src/diag.h: ../src/diag.txt
	$(AM_V_GEN) /bin/sh ../src/gen-diag mrklkit

testrun: all
	for i in $(noinst_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;
