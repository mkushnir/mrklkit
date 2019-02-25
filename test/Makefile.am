CLEANFILES = *.core
#CLEANFILES += *.in

noinst_PROGRAMS=testfoo testfparser testdparser testlk testminmodule



diags = ../src/diag.txt diag.txt
BUILT_SOURCES = diag.c diag.h

noinst_HEADERS = unittest.h ../src/mrklkit/mrklkit.h ../src/mrklkit/util.h

if DEBUG
DEBUG_FLAGS = -g -O0 @CLANG_DEBUG@
else
DEBUG_FLAGS = -DNDEBUG -O3
endif

nodist_testfoo_SOURCES = diag.c
testfoo_SOURCES = testfoo.c
testfoo_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testfoo_LDFLAGS = -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testfparser_SOURCES = diag.c
testfparser_SOURCES = testfparser.c
testfparser_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testfparser_LDFLAGS = -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testdparser_SOURCES = diag.c
testdparser_SOURCES = testdparser.c
testdparser_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testdparser_LDFLAGS = -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testlk_SOURCES = diag.c
testlk_SOURCES = testlk.c
testlk_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testlk_LDFLAGS = -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

nodist_testminmodule_SOURCES = diag.c
testminmodule_SOURCES = testminmodule.c
testminmodule_CFLAGS = -Wall -Wextra -Werror -std=c99 @MRKCOMMON_LOCAL_CFLAGS@ @MRKLKIT_LOCAL_CFLAGS@ @LLVM_CFLAGS@ @_GNU_SOURCE_MACRO@ $(DEBUG_FLAGS) -I.. -I../src -I$(includedir)
testminmodule_LDFLAGS = -lmrklkit @MRKLKIT_LOCAL_LDFLAGS@ -L$(libdir) -lmrkcommon -lmndiag @MRKCOMMON_LOCAL_LDFLAGS@ @LLVM_LIBS@ @LLVM_LDFLAGS@

diag.c diag.h: $(diags)
	$(AM_V_GEN) cat $(diags) | sort -u >diag.txt.tmp && mndiagen -v -S diag.txt.tmp -L mrklkit -H diag.h -C diag.c ../*.[ch] ./*.[ch]

testrun: all
	for i in $(noinst_PROGRAMS); do if test -x ./$$i; then LD_LIBRARY_PATH=$(libdir) ./$$i; fi; done;
