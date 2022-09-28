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

dnl Check for PCRE2 (required)
PKG_CHECK_MODULES([PCRE2], [libpcre2-8])

dnl libxml2 (required)
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0])

dnl Check for the Glib 2 and Gtk 3 libraries, used by virt-p2v (required).
PKG_CHECK_MODULES([GLIB2], [glib-2.0 >= 2.56])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 3.22])

dnl D-Bus is an optional dependency of virt-p2v.
PKG_CHECK_MODULES([DBUS], [dbus-1], [
    AC_SUBST([DBUS_CFLAGS])
    AC_SUBST([DBUS_LIBS])
    AC_DEFINE([HAVE_DBUS],[1],[D-Bus found at compile time.])
],[
    AC_MSG_WARN([D-Bus not found, virt-p2v will not be able to inhibit power saving during P2V conversions])
])
