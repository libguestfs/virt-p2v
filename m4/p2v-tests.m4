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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

dnl Check libguestfs tools.
AC_CHECK_PROG([GUESTFISH],[guestfish],[guestfish],[no])
AC_CHECK_PROG([VIRT_BUILDER],[virt-builder],[virt-builder],[no])
AC_CHECK_PROG([VIRT_V2V],[virt-v2v],[virt-v2v],[no])
AM_CONDITIONAL([HAVE_LIBGUESTFS],
    [test "x$GUESTFISH" != "xno" && test "x$VIRT_BUILDER" != "xno" && test "x$VIRT_V2V" != "xno"])

dnl Check for valgrind
AC_CHECK_PROG([VALGRIND],[valgrind],[valgrind],[no])
AS_IF([test "x$VALGRIND" != "xno"],[
    # Substitute the whole valgrind command.
    # --read-inline-info=no is a temporary workaround for RHBZ#1662656.
    VG='$(VALGRIND) --vgdb=no --leak-check=full --error-exitcode=119 --suppressions=$(abs_top_srcdir)/valgrind-suppressions --trace-children=no --child-silent-after-fork=yes --run-libc-freeres=no --read-inline-info=no'
    ],[
    # No valgrind, so substitute VG with something that will break.
    VG=VALGRIND_IS_NOT_INSTALLED
])
AC_SUBST([VG])
AM_SUBST_NOTMAKE([VG])
