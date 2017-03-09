dnl
dnl Due to the stupid way it's implemented, AC_CHECK_TYPE is nearly useless.
dnl
dnl usage:
dnl
dnl     AC_SCOTTY_CHECK_TYPE
dnl
dnl results:
dnl
dnl     int32_t (defined)
dnl     u_int32_t (defined)
dnl
AC_DEFUN(AC_SCOTTY_CHECK_TYPE,
    [AC_MSG_CHECKING(for $1 using $CC)
    AC_CACHE_VAL(ac_cv_scotty_have_$1,
        AC_TRY_COMPILE([
#       include "confdefs.h"
#       include <sys/types.h>
#	include <netinet/in.h>
#       if STDC_HEADERS
#       include <stdlib.h>
#       include <stddef.h>
#       endif],
        [$1 i],
        ac_cv_scotty_have_$1=yes,
        ac_cv_scotty_have_$1=no))
    AC_MSG_RESULT($ac_cv_scotty_have_$1)
    if test $ac_cv_scotty_have_$1 = no ; then
            AC_DEFINE($1, $2)
    fi])
