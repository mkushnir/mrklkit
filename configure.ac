AC_INIT(mrklkit, 0.2, markiyan.kushnir@gmail.com, mrklkit, https://github.com/mkushnir/mrklkit)
if test -n "$USE_GCC"
then
    probe="gcc${USE_GCC}"
    if which $probe >/dev/null
    then
        CC="gcc${USE_GCC}"
        CXX="c++${USE_GCC}"
        CPP="cpp"
    else
        if which gcc >/dev/null
        then
            CC="gcc"
            CXX="g++"
            CPP="cpp"
        else
            echo "Cannot find gcc"
            exit 1
        fi
    fi
else
    if which clang34 >/dev/null
    then
        CC="clang -Qunused-arguments -fcolor-diagnostics"
        CXX="clang++ -Qunused-arguments -fcolor-diagnostics"
        CPP="clang-cpp"
        clang_version=`clang --version | grep version | awk '{print $4;}'`
        clang_version_mjr=`echo $clang_version | awk -F. '{print $1;}'`
        clang_version_mnr=`echo $clang_version | awk -F. '{print $2;}'`
        if test "$clang_version_mjr" = "3" -a "$clang_version_mnr" -lt 3
        then
            AC_SUBST(CLANG_DEBUG, ['-ftrapv -fcatch-undefined-behavior'])
        else
            # we assume all others are 3.3+, add here options if needed
            AC_SUBST(CLANG_DEBUG, ['-ftrapv'])
        fi
    else
        CC="cc"
        CXX="c++"
        CPP="cpp"
    fi
fi

cxxlib='-lc++'
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
case "$target_os" in
    *linux*)
        AC_SUBST(_GNU_SOURCE_MACRO, ['-D_GNU_SOURCE'])
        cxxlib='-lstdc++'
    ;;
    *)
        AC_SUBST(_GNU_SOURCE_MACRO)
    ;;
esac

AC_CONFIG_MACRO_DIR([m4])

AC_LANG(C)

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

LT_INIT
AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])

dnl AM_MAINTAINER_MODE

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile man/Makefile])
AC_CONFIG_HEADERS(config.h)

AC_CHECK_HEADERS([syslog.h])

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([gettimeofday])
AC_CHECK_FUNCS([localtime_r])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADER_STDBOOL
AC_FUNC_MKTIME
AC_FUNC_STRTOD
AC_TYPE_INT8_T
AC_TYPE_INT64_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_OFF_T

# LLVM_CFALGS modulo -On
AC_CHECK_PROG([LLVM_CONFIG], [llvm-config39], [llvm-config39])
test -z "$LLVM_CONFIG" && AC_CHECK_PROG([LLVM_CONFIG], [llvm-config-devel], [llvm-config-devel])
test -z "$LLVM_CONFIG" && AC_CHECK_PROG([LLVM_CONFIG], [llvm-config], [llvm-config])
test -z "$LLVM_CONFIG" && AC_MSG_FAILURE(llvm-config39 or higher is required. Cannot proceed ...)
llvm_version="`$LLVM_CONFIG --version`"
llvm_version_maj="`echo $llvm_version | awk -F. '{print $1;}'`"
llvm_version_min="`echo $llvm_version | awk -F. '{print $2;}'`"
llvm_version_num="`printf '%d%03d' $llvm_version_maj $llvm_version_min`"
test "$llvm_version_num" -lt 3009 && AC_MSG_FAILURE(LLVM version 3.9 or higher is required. Cannot proceed ...)
AC_SUBST(LLVM_VERSION_NUM, [$llvm_version_num])
dnl AC_SUBST(LLVM_CFLAGS, ["`$LLVM_CONFIG --cflags | sed -e 's/-O[[0-9]]//' -e 's/-DNDEBUG//' -e 's/-pedantic//'` -DLLVM_VERSION_NUM=$llvm_version_num"])
AC_SUBST(LLVM_CFLAGS, ["-I`$LLVM_CONFIG --includedir` -DLLVM_VERSION_NUM=$llvm_version_num"])
AC_SUBST(LLVM_LIBS, [" `$LLVM_CONFIG --system-libs` $cxxlib  `$LLVM_CONFIG --libs engine ipo objcarcopts instrumentation mcjit irreader linker`"])
AC_SUBST(LLVM_LDFLAGS, ["`$LLVM_CONFIG --ldflags`"])

AC_PRESERVE_HELP_ORDER

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [Enable debug build (default=no)]),
              [AM_CONDITIONAL([DEBUG], [enable_debug=yes])],
              [AM_CONDITIONAL([DEBUG], [test "$enable_debug" = "yes"])])

# since we require libmrkcommon be *installed* on the system, we are not setting local compiler/linker flags here.
AC_SUBST(MRKCOMMON_LOCAL_CFLAGS, [''])
AC_SUBST(MRKCOMMON_LOCAL_LDFLAGS, [''])

AC_SUBST(MRKLKIT_LOCAL_CFLAGS, [-I\$\(top_srcdir\)/src])
AC_SUBST(MRKLKIT_LOCAL_LDFLAGS, [-L\$\(top_srcdir\)/src/.libs])

# pass LDFLAGS=-L${LOCALBASE}/lib in the environment
AC_CHECK_LIB(mrkcommon, _fini, [], [AC_MSG_FAILURE(libmrkcommon.so is required. Please find it at git@github.com:mkushnir/mrkcommon.git)])
#export LDFLAGS="`$LLVM_CONFIG --ldflags` $LDFLAGS"
#AC_CHECK_LIB(LLVMCore, _fini, [], [AC_MSG_FAILURE(libLLVMCore.a is required. Please find it at ...)])

AC_OUTPUT
