AC_PREREQ([2.68])
AC_INIT([tiptop], [2.1], [erven.rohou@inria.fr])

AC_CONFIG_SRCDIR([src/tiptop.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LEX
AC_PROG_YACC
AC_PROG_INSTALL


AC_ARG_ENABLE(curses, [  --disable-curses        disable usage of libcurses, even if available],
              [disable_curses=$enableval], [disable_curses=$enableval])

AC_ARG_ENABLE(libxml2, [  --disable-libxml2       disable usage of libxml2, even if available],
              [disable_libxml2=$enableval], [disable_libxml2=$enableval])

# Checks for libraries.
if test ! x$disable_curses = xno; then # check only if not disabled
AC_CHECK_LIB([curses], [initscr],
                  [have_curses=yes;
                  AC_DEFINE([HAVE_LIBCURSES], [1], [Define to 1 if you have the `curses' library (-lcurses).])
                  LIBS="-lcurses $LIBS"],
             [have_curses=no])
fi

if test ! x$disable_libxml2 = xno; then # check only if not disabled
AC_CHECK_LIB([xml2], [xmlParseFile],
                  [have_xml2=yes;
                  AC_DEFINE([HAVE_LIBXML2], [1], [Define to 1 if you have the `libxml2' library (-lxml2).])
                  LIBS="-lxml2 $LIBS"
                  CFLAGS="$CFLAGS -I/usr/include/libxml2"],
             [have_xml2=no])
fi

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdint.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])



AC_CHECK_HEADERS([linux/perf_counter.h], [have_perf_counter=yes],
                                         [have_perf_counter=no])

AC_CHECK_HEADERS([linux/perf_event.h], [have_perf_event=yes],
                                       [have_perf_event=no])

if test "x${have_perf_counter}" = xno -a "x${have_perf_event}" = xno; then
   os=`uname -s -r`
   AC_MSG_FAILURE([
  ------------------------------------------------------------
   Could not locate linux/perf_count.h or linux/perf_event.h.
   Are performance counters supported on this machine?
   Linux 2.6.31+ is required.
   uname reports: ${os}
  ------------------------------------------------------------])
fi


# Check for hardware architecture
no_target=yes
AC_MSG_CHECKING([hardware])
hw=`uname -m`
case $hw in
  x86_64 | i386 | i686 ) :
    AC_MSG_RESULT([x86])
    AC_DEFINE([TARGET_X86], [1], [Define to 1 if the target is x86.])
    no_target=no
    ;;
  unknown ) :
    AC_MSG_RESULT([unknown])
    AC_MSG_WARN([Could not detect architecture])
    ;;
  * ) :
    AC_MSG_RESULT([$hw])
    ;;
esac

if test x$no_target = xyes; then
   AC_DEFINE([NOTARGET], [1], [Define to 1 when no specific target is supported.])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset select strdup strerror strstr uname])

AC_CONFIG_FILES([Makefile
                 src/Makefile])

AC_ARG_ENABLE(debug, [  --enable-debug          enable support for debug],
              [AC_DEFINE([ENABLE_DEBUG], [1], [Define to 1 to enable support for debug.])])


if test "x${have_curses}" = xno; then
   AC_MSG_WARN([
  -----------------------------------------------------
   Library curses not found. Building without support.
  -----------------------------------------------------])
fi

if test "x${have_xml2}" = xno; then
   AC_MSG_WARN([
  -----------------------------------------------------
   Library xml2 not found. Building without support.
  -----------------------------------------------------])
fi

AC_OUTPUT
