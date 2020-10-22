#
# Copyright (C) Huawei Technologies Co., Ltd. 2019.  ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

#
# Enable fault-tolerance
#
AC_ARG_ENABLE([fault-tolerance],
              [AS_HELP_STRING([--enable-fault-tolerance],
                              [Enable fault-tolerance, default: NO])],
              [],
              [enable_fault_tolerance=no])

AS_IF([test "x$enable_fault_tolerance" = xyes],
      [AS_MESSAGE([enabling with fault-tolerance])
       AC_DEFINE([ENABLE_FAULT_TOLERANCE], [1], [Enable fault-tolerance])],
      [:])

#
# Detect some UCT API incompatiblity (see UCX Change-ID 8da6a5be2e)
#
SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="-Isrc/ $CPPFLAGS"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include "uct/api/uct.h"]],
                                   [[uct_completion_callback_t func = NULL;]
                                   [func(NULL, UCS_OK);]])],
                  [AC_MSG_RESULT([uct_completion_callback_t has a status argument])
                   AC_DEFINE([HAVE_UCT_COMP_CB_STATUS_ARG], [],
                             [Does uct_completion_callback_t have a "status" argument])],
                  [AC_MSG_RESULT([uct_completion_callback_t has no status argument])])
CPPFLAGS="$SAVE_CPPFLAGS"

ucg_modules=":builtin"
m4_include([src/ucg/base/configure.m4])
m4_include([src/ucg/builtin/configure.m4])
AC_DEFINE_UNQUOTED([ucg_MODULES], ["${ucg_modules}"], [UCG loadable modules])

AC_CONFIG_FILES([src/ucg/Makefile
                 src/ucg/api/ucg_version.h
                 src/ucg/base/ucg_version.c])
