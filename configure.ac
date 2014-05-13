AC_INIT(mrklkit, 0.1, markiyan.kushnir@gmail.com, mrklkit, https://github.com/mkushnir/mrklkit)
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
            AC_SUBST(CLANG_DEBUG, ['-ftrapv -fcatch-undefined-behavior'])
        fi
    else
        CC="cc"
        CXX="c++"
        CPP="cpp"
    fi
fi

cxxlib='-lc++'
uname_s=`uname -s`
if test "$uname_s" = "Linux"
then
    AC_SUBST(_GNU_SOURCE_MACRO, ['-D_GNU_SOURCE'])
    cxxlib='-lstdc++'
fi

AC_CONFIG_MACRO_DIR([m4])

AC_LANG(C)

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
AC_PROG_INSTALL

LT_INIT
AM_INIT_AUTOMAKE

dnl AM_MAINTAINER_MODE

AC_CONFIG_FILES([Makefile src/Makefile src/test/Makefile man/Makefile])
AC_CONFIG_HEADERS(config.h)

AC_CHECK_HEADERS([syslog.h])

AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_FUNCS([memset])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([limits.h])
AC_FUNC_STRTOD
AC_TYPE_INT64_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT64_T
AC_TYPE_OFF_T


# LLVM_CFALGS modulo -On
if which llvm-config >/dev/null
then
    AC_SUBST(LLVM_CFLAGS, ["`llvm-config --cflags | sed -e 's/-O[[0-9]]//' -e 's/-DNDEBUG//'`"])
    AC_SUBST(LLVM_LIBS, ["`llvm-config --libs`"])
    AC_SUBST(LLVM_LDFLAGS, ["`llvm-config --ldflags` $cxxlib"])
    #AC_SUBST(LLVM_LIBS, ["`llvm-config --libs engine interpreter all-targets ipo`"])
else
    AC_SUBST(LLVM_CFLAGS, ["`llvm-config34 --cflags | sed -e 's/-O[[0-9]]//' -e 's/-DNDEBUG//'`"])
    AC_SUBST(LLVM_LIBS, ["`llvm-config34 --libs`"])
    AC_SUBST(LLVM_LDFLAGS, ["`llvm-config34 --ldflags` $cxxlib"])
    #AC_SUBST(LLVM_LIBS, ["`llvm-config34 --libs engine interpreter all-targets ipo`"])
fi

AC_PRESERVE_HELP_ORDER

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug],
                             [Enable debug build (default=no)]),
              [AM_CONDITIONAL([DEBUG], [enable_debug=yes])],
              [AM_CONDITIONAL([DEBUG], [test "$enable_debug" = "yes"])])

# pass LDFLAGS=-L${LOCALBASE}/lib in the environment
AC_CHECK_LIB(mrkcommon, _fini, [], [AC_MSG_FAILURE(libmrkcommon.so is required. Please find it at git@github.com:mkushnir/mrkcommon.git)])

AC_OUTPUT
