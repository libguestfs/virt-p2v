# virt-p2v
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

# The major, minor, and release fields MUST be numbers.  Packagers can
# add extra information using --with-extra="..." which may be any
# freeform string.
m4_define([p2v_major],   [1])
m4_define([p2v_minor],   [40])
m4_define([p2v_release], [0])

AC_INIT([virt-p2v],p2v_major.p2v_minor.p2v_release)

# Headings within the configure script output.
m4_define([HEADING],
          [AS_ECHO
           AS_ECHO(["--- $1 ---"])])

AC_CONFIG_AUX_DIR([build-aux])

AC_USE_SYSTEM_EXTENSIONS

dnl Initialize automake.
AM_INIT_AUTOMAKE(foreign subdir-objects tar-pax) dnl NB: Do not [quote] this parameter.

m4_ifndef([AM_SILENT_RULES], [m4_define([AM_SILENT_RULES],[])])
AM_SILENT_RULES([yes]) # make --enable-silent-rules the default.

AC_CONFIG_MACRO_DIR([m4])

dnl Extra string, a freeform string defined by packagers.
AC_ARG_WITH([extra],
    [AS_HELP_STRING([--with-extra],
                    [extra version string (for use by packagers)])],
    [p2v_extra="$withval"],
    [p2v_extra=]
)

AC_MSG_NOTICE([virt-p2v version p2v_major.p2v_minor.p2v_release$p2v_extra])

dnl Split up the version string.
AC_DEFINE([PACKAGE_VERSION_MAJOR],[p2v_major],[Major version number.])
AC_DEFINE([PACKAGE_VERSION_MINOR],[p2v_minor],[Minor version number.])
AC_DEFINE([PACKAGE_VERSION_RELEASE],[p2v_release],[Release number.])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_EXTRA],["$p2v_extra"],[Extra version string.])
PACKAGE_VERSION_FULL="p2v_major.p2v_minor.p2v_release${p2v_extra}"
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_FULL],["$PACKAGE_VERSION_FULL"],[Full version string.])
AC_SUBST([PACKAGE_VERSION_FULL])

dnl Early gnulib initialization.
HEADING([Configuring Gnulib])
gl_EARLY
gl_INIT

dnl Check for external programs required to either build or run
dnl virt-p2v.
HEADING([Checking for external programs])
m4_include([m4/p2v-progs.m4])

dnl The C compiler environment.
HEADING([Checking for the C compiler environment])
m4_include([m4/p2v-c.m4])

dnl Any C libraries required by virt-p2v.
HEADING([Checking for libraries used by virt-p2v])
m4_include([m4/p2v-libraries.m4])

dnl All the programs used for tests.
HEADING([Checking for programs for tests])
m4_include([m4/p2v-tests.m4])

dnl Produce output files.
HEADING([Generating output files])

AC_CONFIG_HEADERS([config.h])

dnl http://www.mail-archive.com/automake@gnu.org/msg10204.html
AC_CONFIG_FILES([virt-p2v-make-disk],
                [chmod +x,-w virt-p2v-make-disk])
AC_CONFIG_FILES([virt-p2v-make-kickstart],
                [chmod +x,-w virt-p2v-make-kickstart])
AC_CONFIG_FILES([virt-p2v-make-kiwi],
                [chmod +x,-w virt-p2v-make-kiwi])
AC_CONFIG_FILES([podwrapper.pl],
                [chmod +x,-w podwrapper.pl])
AC_CONFIG_FILES([run],
                [chmod +x,-w run])

AC_CONFIG_FILES([Makefile
                 gnulib/lib/Makefile
                 gnulib/tests/Makefile])

AC_CONFIG_COMMANDS([p2v-config.h],
                   [${ac_srcdir}/generate-p2v-config.pl --file=p2v-config.h --output=p2v-config.h])

AC_OUTPUT
