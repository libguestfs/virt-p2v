# libguestfs
# Copyright (C) 2009-2019 Red Hat Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

dnl Should we run the gnulib tests?
AC_MSG_CHECKING([if we should run the GNUlib tests])
AC_ARG_ENABLE([gnulib-tests],
    [AS_HELP_STRING([--disable-gnulib-tests],
        [disable running GNU Portability library tests @<:@default=yes@:>@])],
        [ENABLE_GNULIB_TESTS="$enableval"],
        [ENABLE_GNULIB_TESTS=yes])
AM_CONDITIONAL([ENABLE_GNULIB_TESTS],[test "x$ENABLE_GNULIB_TESTS" = "xyes"])
AC_MSG_RESULT([$ENABLE_GNULIB_TESTS])

dnl Check libguestfs tools (needed for create the test images).
AC_CHECK_PROG([GUESTFISH],[guestfish],[guestfish],[no])
AC_CHECK_PROG([VIRT_BUILDER],[virt-builder],[virt-builder],[no])
AM_CONDITIONAL([HAVE_LIBGUESTFS],
    [test "x$GUESTFISH" != "xno" && test "x$VIRT_BUILDER" != "xno"])

dnl Check for valgrind
AC_CHECK_PROG([VALGRIND],[valgrind],[valgrind],[no])
AS_IF([test "x$VALGRIND" != "xno"],[
    # Substitute the whole valgrind command.
    # --read-inline-info=no is a temporary workaround for RHBZ#1662656.
    VG='$(VALGRIND) --vgdb=no --log-file=$(abs_top_builddir)/valgrind-%q{T}-%p.log --leak-check=full --error-exitcode=119 --suppressions=$(abs_top_srcdir)/valgrind-suppressions --trace-children=no --child-silent-after-fork=yes --run-libc-freeres=no --read-inline-info=no'
    ],[
    # No valgrind, so substitute VG with something that will break.
    VG=VALGRIND_IS_NOT_INSTALLED
])
AC_SUBST([VG])
AM_SUBST_NOTMAKE([VG])
