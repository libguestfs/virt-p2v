# libguestfs
# Copyright (C) 2013 Red Hat Inc.
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

# subdir-rules.mk should be included in every *subdirectory* Makefile.am.

# Editor backup files
CLEANFILES = *~ *.bak

# Patch original and reject files.
CLEANFILES += *.orig *.rej

# Manual pages - these are all generated from *.pod, so the
# pages themselves should all be removed by 'make clean'.
CLEANFILES += *.1 *.3 *.5 *.8

# Stamp files used when generating man pages.
CLEANFILES += stamp-*.pod

# Files that should be universally removed by 'make distclean'.
DISTCLEANFILES = stamp-*

# custom silent rules
guestfs_am_v_podwrapper = $(guestfs_am_v_podwrapper_@AM_V@)
guestfs_am_v_podwrapper_ = $(guestfs_am_v_podwrapper_@AM_DEFAULT_V@)
guestfs_am_v_podwrapper_0 = @echo "  POD     " $@;

# Test shell scripts should use '$TEST_FUNCTIONS' to get a predefined
# set of helper functions for running tests (see
# tests/test-functions.sh).
#
# Notes:
#
# (1) This is in fact a single command all on one line.  The variables
# are evaluated in test-functions.sh.
#
# (2) We use absolute paths here and in test-functions.sh so that the
# test can change directory freely.  But we also include the
# non-absolute values so they can be used by the test script itself.
export TEST_FUNCTIONS := \
	source $(abs_top_srcdir)/test-functions.sh \
	abs_srcdir="$(abs_srcdir)" \
	abs_builddir="$(abs_builddir)" \
	top_srcdir="$(top_srcdir)" \
	top_builddir="$(top_builddir)" \
	abs_top_srcdir="$(abs_top_srcdir)" \
	abs_top_builddir="$(abs_top_builddir)"
