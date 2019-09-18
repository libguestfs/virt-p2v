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

dnl Any C libraries required by virt-p2v.

dnl Define a C symbol for the host CPU architecture.
AC_DEFINE_UNQUOTED([host_cpu],["$host_cpu"],[Host architecture.])

dnl Headers.
AC_CHECK_HEADERS([\
    linux/rtc.h])

dnl Which header file defines major, minor, makedev.
AC_HEADER_MAJOR

dnl Check for PCRE (required)
PKG_CHECK_MODULES([PCRE], [libpcre], [], [
    AC_CHECK_PROGS([PCRE_CONFIG], [pcre-config pcre2-config], [no])
    AS_IF([test "x$PCRE_CONFIG" = "xno"], [
        AC_MSG_ERROR([Please install the pcre devel package])
    ])
    PCRE_CFLAGS=`$PCRE_CONFIG --cflags`
    PCRE_LIBS=`$PCRE_CONFIG --libs`
])

dnl libxml2 (required)
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0])

dnl Check for Gtk 2 or 3 library, used by virt-p2v.
AC_MSG_CHECKING([for --with-gtk option])
AC_ARG_WITH([gtk],
    [AS_HELP_STRING([--with-gtk=2|3|check|no],
        [prefer Gtk version 2 or 3. @<:@default=check@:>@])],
    [with_gtk="$withval"
     AC_MSG_RESULT([$withval])],
    [with_gtk="check"
     AC_MSG_RESULT([not set, will check for installed Gtk])]
)

if test "x$with_gtk" = "x3"; then
    PKG_CHECK_MODULES([GTK], [gtk+-3.0], [
        GTK_VERSION=3
    ])
elif test "x$with_gtk" = "x2"; then
    PKG_CHECK_MODULES([GTK], [gtk+-2.0], [
        GTK_VERSION=2
    ], [])
elif test "x$with_gtk" = "xcheck"; then
    PKG_CHECK_MODULES([GTK], [gtk+-3.0], [
        GTK_VERSION=3
    ], [
        PKG_CHECK_MODULES([GTK], [gtk+-2.0], [
            GTK_VERSION=2
        ])
    ])
fi

AC_SUBST([GTK_VERSION])

dnl D-Bus is an optional dependency of virt-p2v.
PKG_CHECK_MODULES([DBUS], [dbus-1], [
    AC_SUBST([DBUS_CFLAGS])
    AC_SUBST([DBUS_LIBS])
    AC_DEFINE([HAVE_DBUS],[1],[D-Bus found at compile time.])
],[
    AC_MSG_WARN([D-Bus not found, virt-p2v will not be able to inhibit power saving during P2V conversions])
])
