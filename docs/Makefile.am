# libguestfs documentation
# Copyright (C) 2010-2019 Red Hat Inc.
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

include $(top_srcdir)/subdir-rules.mk

EXTRA_DIST = \
	p2v-building.pod \
	p2v-hacking.pod \
	p2v-release-notes.pod

man_MANS = \
	p2v-building.1 \
	p2v-hacking.1 \
	p2v-release-notes.1
noinst_DATA = \
	$(top_builddir)/website/p2v-building.1.html \
	$(top_builddir)/website/p2v-hacking.1.html \
	$(top_builddir)/website/p2v-release-notes.1.html

p2v-building.1 $(top_builddir)/website/p2v-building.1.html: stamp-p2v-building.pod

stamp-p2v-building.pod: p2v-building.pod
	$(PODWRAPPER) \
	  --section 1 \
	  --man p2v-building.1 \
	  --html $(top_builddir)/website/p2v-building.1.html \
	  --license LGPLv2+ \
	  $<
	touch $@

p2v-hacking.1 $(top_builddir)/website/p2v-hacking.1.html: stamp-p2v-hacking.pod

stamp-p2v-hacking.pod: p2v-hacking.pod
	$(PODWRAPPER) \
	  --section 1 \
	  --man p2v-hacking.1 \
	  --html $(top_builddir)/website/p2v-hacking.1.html \
	  --license LGPLv2+ \
	  $<
	touch $@

p2v-release-notes.1 $(top_builddir)/website/p2v-release-notes.1.html: stamp-p2v-release-notes.pod

stamp-p2v-release-notes.pod: p2v-release-notes.pod
	$(PODWRAPPER) \
	  --section 1 \
	  --man p2v-release-notes.1 \
	  --html $(top_builddir)/website/p2v-release-notes.1.html \
	  --license GPLv2+ \
	  $<
	touch $@
