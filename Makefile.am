# libguestfs virt-p2v
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

include $(top_srcdir)/subdir-rules.mk

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = gnulib/lib
SUBDIRS += docs

# bash-completion
SUBDIRS += bash

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(TESTS) $(LIBGUESTFS_TESTS) $(SLOW_TESTS) \
	.gitignore \
	AUTHORS \
	dependencies.m4 \
	generate-p2v-authors.pl \
	generate-p2v-config.pl \
	issue \
	kiwi-config.sh \
	kiwi-config.xml.in \
	launch-virt-p2v \
	libguestfs/README \
	make-physical-machine.sh \
	miniexpect/README \
	p2v.ks.in \
	p2v.service \
	podcheck.pl \
	test-functions.sh \
	test-virt-p2v-docs.sh \
	test-virt-p2v-pxe.sshd_config.in \
	test-virt-p2v-scp.sh \
	test-virt-p2v-ssh.sh \
	valgrind-suppressions \
	virt-p2v.pod \
	virt-p2v-make-disk.in \
	virt-p2v-make-disk.pod \
	virt-p2v-make-kickstart.in \
	virt-p2v-make-kickstart.pod \
	virt-p2v-make-kiwi.in \
	virt-p2v-make-kiwi.pod

# Don't clean ssh_host_rsa_key{,.pub} or id_rsa{,.pub} since those
# consume system entropy to regenerate.
CLEANFILES += \
	$(dependencies_files) \
	$(generated_sources) \
	$(PHYSICAL_MACHINE) $(BLANK_DISK) \
	about-authors.c \
	physical-machine.tmp.* \
	stamp-test-virt-p2v-pxe-data-files \
	stamp-test-virt-p2v-pxe-kernel \
	test-virt-p2v-pxe.authorized_keys \
	test-virt-p2v-pxe.img \
	test-virt-p2v-pxe.vmlinuz \
	test-virt-p2v-pxe.initramfs \
	test-virt-p2v-pxe.sshd_config \
	virt-p2v.img \
	virt-p2v-kernel-config.pod \
	virt-p2v.xz \
	website/*.html

# Although virt-p2v is a regular binary, it is not usually installed
# in /usr/bin since it only functions when contained in an ISO or PXE
# image which is used to boot the physical machine (since otherwise
# virt-p2v would not be able to get a consistent snapshot of the
# physical disks).  Also we don't want the naked binary to appear on
# the host, which would cause various Gtk dependencies to be pulled
# in, so it must be compressed.
virtp2vlibdir = $(libdir)/virt-p2v
virtp2vlib_DATA = virt-p2v.xz

virt-p2v.xz: virt-p2v
	rm -f $@ $@-t
	xz --best --keep --stdout $< > $@-t
	mv $@-t $@

noinst_PROGRAMS = virt-p2v

virt_p2v_SOURCES = \
	miniexpect/miniexpect.c \
	miniexpect/miniexpect.h \
	libguestfs/cleanups.c \
	libguestfs/cleanups.h \
	libguestfs/guestfs-utils.c \
	libguestfs/guestfs-utils.h \
	libguestfs/libxml2-cleanups.c \
	libguestfs/libxml2-writer-macros.h \
	conversion.c \
	cpuid.c \
	disks.c \
	gui.c \
	gui-gtk3-compat.h \
	inhibit.c \
	kernel.c \
	kernel-cmdline.c \
	main.c \
	nbd.c \
	p2v.h \
	p2v-config.h \
	physical-xml.c \
	rtc.c \
	ssh.c \
	utils.c

generated_sources = \
	config.c \
	kernel-config.c \
	p2v-config.h

nodist_virt_p2v_SOURCES = \
	$(generated_sources) \
	about-authors.c

virt_p2v_CPPFLAGS = \
	-DLOCALEBASEDIR=\""$(datadir)/locale"\" \
	-DGTK_DISABLE_DEPRECATED \
	-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_56 \
	-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_56 \
	-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_3_22 \
	-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_3_22 \
	-I$(srcdir)/libguestfs \
	-I$(srcdir)/miniexpect \
	-I$(srcdir)/gnulib/lib -Ignulib/lib

virt_p2v_CFLAGS = \
	-pthread \
	$(WARN_CFLAGS) $(WERROR_CFLAGS) \
	$(PCRE2_CFLAGS) \
	$(LIBXML2_CFLAGS) \
	$(GTK3_CFLAGS) \
	$(DBUS_CFLAGS)

virt_p2v_LDADD = \
	$(PCRE2_LIBS) \
	$(LIBXML2_LIBS) \
	$(GTK3_LIBS) \
	$(DBUS_LIBS) \
	gnulib/lib/libgnu.la \
	-lm

$(generated_sources) virt-p2v-kernel-config.pod: $(srcdir)/generate-p2v-config.pl
	$(AM_V_GEN)rm -f $@ $@-t && $(PERL) $(<) --file=$@ --output=$@-t && mv $@-t $@

about-authors.c: $(srcdir)/generate-p2v-authors.pl $(srcdir)/AUTHORS
	$(AM_V_GEN)rm -f $@ $@-t && $(PERL) $(<) $(srcdir)/AUTHORS > $@-t && mv $@-t $@

# Scripts to build the disk image, USB key, or kickstart.
bin_SCRIPTS = virt-p2v-make-disk virt-p2v-make-kickstart virt-p2v-make-kiwi

dependencies_files = \
	dependencies.archlinux \
	dependencies.debian \
	dependencies.redhat \
	dependencies.suse

$(dependencies_files): dependencies.m4 config.status
	define=`echo $@ | $(SED) 's/dependencies.//;y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`; \
	m4 -D$$define=1 $< > $@-t
	mv $@-t $@

# Support files needed by the virt-p2v-make-* scripts.
virtp2vdatadir = $(datadir)/virt-p2v

virtp2vdata_DATA = \
	$(dependencies_files) \
	issue \
	kiwi-config.sh \
	kiwi-config.xml.in \
	launch-virt-p2v \
	p2v.ks.in \
	p2v.service

# Manual pages and HTML files for the website.
man_MANS = \
	virt-p2v.1 \
	virt-p2v-make-disk.1 \
	virt-p2v-make-kickstart.1 \
	virt-p2v-make-kiwi.1

noinst_DATA = \
	$(top_builddir)/website/virt-p2v.1.html \
	$(top_builddir)/website/virt-p2v-make-disk.1.html \
	$(top_builddir)/website/virt-p2v-make-kickstart.1.html \
	$(top_builddir)/website/virt-p2v-make-kiwi.1.html

virt-p2v.1 $(top_builddir)/website/virt-p2v.1.html: stamp-virt-p2v.pod

stamp-virt-p2v.pod: virt-p2v.pod virt-p2v-kernel-config.pod
	$(PODWRAPPER) \
	  --man virt-p2v.1 \
	  --html $(top_builddir)/website/virt-p2v.1.html \
	  --insert virt-p2v-kernel-config.pod:__KERNEL_CONFIG__ \
	  --license GPLv2+ \
	  --warning safe \
	  $<
	touch $@

virt-p2v-make-disk.1 $(top_builddir)/website/virt-p2v-make-disk.1.html: stamp-virt-p2v-make-disk.pod

stamp-virt-p2v-make-disk.pod: virt-p2v-make-disk.pod
	$(PODWRAPPER) \
	  --man virt-p2v-make-disk.1 \
	  --html $(top_builddir)/website/virt-p2v-make-disk.1.html \
	  --license GPLv2+ \
	  --warning safe \
	  $<
	touch $@

virt-p2v-make-kickstart.1 $(top_builddir)/website/virt-p2v-make-kickstart.1.html: stamp-virt-p2v-make-kickstart.pod

stamp-virt-p2v-make-kickstart.pod: virt-p2v-make-kickstart.pod
	$(PODWRAPPER) \
	  --man virt-p2v-make-kickstart.1 \
	  --html $(top_builddir)/website/virt-p2v-make-kickstart.1.html \
	  --license GPLv2+ \
	  --warning safe \
	  $<
	touch $@

virt-p2v-make-kiwi.1 $(top_builddir)/website/virt-p2v-make-kiwi.1.html: stamp-virt-p2v-make-kiwi.pod

stamp-virt-p2v-make-kiwi.pod: virt-p2v-make-kiwi.pod
	$(PODWRAPPER) \
	  --man virt-p2v-make-kiwi.1 \
	  --html $(top_builddir)/website/virt-p2v-make-kiwi.1.html \
	  --license GPLv2+ \
	  --warning safe \
	  $<
	touch $@

# Run virt-p2v locally either directly or in a VM.
# See p2v-hacking(1) section "Running virt-p2v"

PHYSICAL_MACHINE = $(abs_builddir)/fedora.img
BLANK_DISK = blank-part.img

check_DATA = \
	$(NULL)
if HAVE_LIBGUESTFS
check_DATA += \
	$(PHYSICAL_MACHINE) \
	$(BLANK_DISK)
check_SCRIPTS = make-physical-machine.sh
endif HAVE_LIBGUESTFS

run-virt-p2v-directly: $(PHYSICAL_MACHINE)
	$(top_builddir)/run virt-p2v --test-disk=$(PHYSICAL_MACHINE)

if HAVE_LIBGUESTFS
run-virt-p2v-in-a-vm: virt-p2v.img $(PHYSICAL_MACHINE)
	@echo "*** NOTE: Enable the boot menu and choose USB ***"
	$(shell guestfish get-hv) \
	  -M pc,accel=kvm:tcg \
	  -cpu host \
	  -smp sockets=1,cores=2,threads=2 \
	  -m 2048 \
	  -drive id=hd0,file=$(PHYSICAL_MACHINE),format=raw,if=ide \
	  -device qemu-xhci \
	  -drive id=usb0,file=$<,format=raw,snapshot=on,if=none \
	  -device usb-storage,bootindex=1,drive=usb0 \
	  -boot menu=on \
	  -netdev user,id=net0,net=169.254.0.0/16 \
	  -device virtio-net-pci,netdev=net0 \
	  -netdev user,id=net1 \
	  -device rtl8139,netdev=net1 \
	  -netdev user,id=net2 \
	  -device e1000,netdev=net2 \
	  $(QEMU_OPTIONS) \
	  &

run-virt-p2v-in-an-nvme-vm: virt-p2v.img $(PHYSICAL_MACHINE) $(BLANK_DISK)
	@echo "*** NOTE: Enable the boot menu and choose USB ***"
	$(shell guestfish get-hv) \
	  -M pc,accel=kvm:tcg \
	  -cpu host \
	  -smp sockets=1,cores=2,threads=2 \
	  -m 2048 \
	  -boot menu=on \
	  \
	  -drive id=phys,if=none,format=raw,file=$(PHYSICAL_MACHINE),snapshot=on \
	  -device nvme,drive=phys,serial=01234567 \
	  \
	  -drive id=usb0,if=none,format=raw,file=$<,snapshot=on \
	  -device qemu-xhci,id=usb-bus \
	  -device usb-storage,drive=usb0,bootindex=1,bus=usb-bus.0 \
	  \
	  -drive id=blank1,if=none,format=raw,file=$(BLANK_DISK),snapshot=on \
	  -drive id=blank2,if=none,format=raw,file=$(BLANK_DISK),snapshot=on \
	  -device nvme,id=nvme-ctrl,serial=89abcdef \
	  -device nvme-ns,drive=blank1,bus=nvme-ctrl \
	  -device nvme-ns,drive=blank2,bus=nvme-ctrl \
	  \
	  -netdev user,id=net0,net=169.254.0.0/16 \
	  -device virtio-net-pci,netdev=net0 \
	  \
	  -netdev user,id=net1 \
	  -device rtl8139,netdev=net1 \
	  \
	  -netdev user,id=net2 \
	  -device e1000,netdev=net2 \
	  \
	  $(QEMU_OPTIONS) \
	  &
endif HAVE_LIBGUESTFS

run-virt-p2v-non-gui-conversion: $(PHYSICAL_MACHINE) stamp-test-virt-p2v-pxe-data-files
	SLOW=1 $(top_builddir)/run ./test-virt-p2v-pxe.sh

$(PHYSICAL_MACHINE):
	$(top_builddir)/run ./make-physical-machine.sh $@

$(BLANK_DISK):
	$(top_builddir)/run guestfish -N $@=part exit

# Note before running targets that use this (eg.
# “make run-virt-p2v-in-a-vm”) you can optionally set
# VIRT_P2V_OSVERSION to control the virt-builder osversion.  If not
# set then it defaults to the osversion of your current system.
virt-p2v.img: \
	    $(dependencies_files) \
	    issue \
	    launch-virt-p2v \
	    p2v.service \
	    virt-p2v \
	    virt-p2v-make-disk
	$(top_builddir)/run virt-p2v-make-disk $(VIRT_P2V_OSVERSION) -o $@

# Tests.

TESTS_ENVIRONMENT = $(top_builddir)/run --test

TESTS = \
	test-virt-p2v-cmdline.sh \
	test-virt-p2v-docs.sh

LIBGUESTFS_TESTS = \
	test-virt-p2v-nbdkit.sh

if HAVE_LIBGUESTFS
TESTS += \
	$(LIBGUESTFS_TESTS) \
	$(SLOW_TESTS)
endif HAVE_LIBGUESTFS

check-valgrind:
	make VG="@VG@" check

SLOW_TESTS = \
	test-virt-p2v-pxe.sh

check-slow: stamp-test-virt-p2v-pxe-data-files
	$(MAKE) check TESTS="$(SLOW_TESTS)" SLOW=1

stamp-test-virt-p2v-pxe-data-files: \
	    test-virt-p2v-pxe.authorized_keys \
	    test-virt-p2v-pxe.img \
	    test-virt-p2v-pxe.vmlinuz test-virt-p2v-pxe.initramfs \
	    test-virt-p2v-pxe.sshd_config \
	    test-virt-p2v-pxe.ssh_host_rsa_key \
	    test-virt-p2v-pxe.ssh_host_rsa_key.pub \
	    test-virt-p2v-pxe.id_rsa test-virt-p2v-pxe.id_rsa.pub
	touch $@

test-virt-p2v-pxe.img: \
	    $(dependencies_files) \
	    issue \
	    launch-virt-p2v \
	    p2v.service \
	    test-virt-p2v-pxe.id_rsa \
	    virt-p2v \
	    virt-p2v-make-disk
	$(top_builddir)/run virt-p2v-make-disk \
	    --inject-ssh-identity=test-virt-p2v-pxe.id_rsa \
	    -o $@-t
	mv $@-t $@

test-virt-p2v-pxe.vmlinuz test-virt-p2v-pxe.initramfs: stamp-test-virt-p2v-pxe-kernel

stamp-test-virt-p2v-pxe-kernel: test-virt-p2v-pxe.img
	rm -f $@ vmlinuz initramfs test-virt-p2v-pxe.vmlinuz test-virt-p2v-pxe.initramfs
	$(top_builddir)/run virt-get-kernel --unversioned-names -a $<
	mv vmlinuz test-virt-p2v-pxe.vmlinuz
	mv initramfs test-virt-p2v-pxe.initramfs
	touch $@

test-virt-p2v-pxe.sshd_config: test-virt-p2v-pxe.sshd_config.in
	rm -f $@ $@-t
	@AWK@ \
	  -v "abs_builddir=$(abs_builddir)" \
	  '{ \
	    gsub (/__RANDOM_PORT__/, 10000 + int (1000 * rand())); \
	    gsub (/__abs_builddir__/, abs_builddir); \
	    print; \
	  }' < $< > $@-t
	chmod 0444 $@-t
	mv $@-t $@

test-virt-p2v-pxe.authorized_keys: test-virt-p2v-pxe.id_rsa.pub $(top_builddir)/run
	rm -f $@ $@-t
	$(top_builddir)/run sh -c 'echo -n environment=\"PATH=$$PATH\"\ ' > $@-t
	cat $< >> $@-t
	mv $@-t $@

test-virt-p2v-pxe.ssh_host_rsa_key test-virt-p2v-pxe.ssh_host_rsa_key.pub: stamp-test-virt-p2v-pxe-hostkey

stamp-test-virt-p2v-pxe-hostkey:
	rm -f test-virt-p2v-pxe.ssh_host_rsa_key
	rm -f test-virt-p2v-pxe.ssh_host_rsa_key.pub
	ssh-keygen -t rsa -f test-virt-p2v-pxe.ssh_host_rsa_key -N ''
	touch $@

test-virt-p2v-pxe.id_rsa test-virt-p2v-pxe.id_rsa.pub: stamp-test-virt-p2v-pxe-userkey

stamp-test-virt-p2v-pxe-userkey:
	rm -f test-virt-p2v-pxe.id_rsa
	rm -f test-virt-p2v-pxe.id_rsa.pub
	ssh-keygen -t rsa -f test-virt-p2v-pxe.id_rsa -N ''
	touch $@

# Check no files are missing from EXTRA_DIST rules, and that all
# generated files have been included in the tarball.  (Note you must
# have done 'make dist')
maintainer-check-extra-dist:
	@zcat $(PACKAGE_NAME)-$(VERSION).tar.gz | tar tf - | sort | \
	  sed 's,^$(PACKAGE_NAME)-$(VERSION)/,,' > tarfiles
	@git ls-files | \
	  grep -v \
	    -e '^\.cirrus.yml' \
	    -e '^\.gitlab-ci.yml' \
	    -e '^ci/' | \
	    sort > gitfiles
	@comm -13 tarfiles gitfiles > comm-out
	@echo Checking for differences between EXTRA_DIST and git ...
	@cat comm-out
	@[ ! -s comm-out ]
	@rm tarfiles gitfiles comm-out
	@echo PASS: EXTRA_DIST tests

# Commit everything in current directory to HEAD, and set commit
# message to current version (only for maintainer).
maintainer-commit:
	git commit -a -m "Version $(VERSION)."

# Tag HEAD with current version (only for maintainer).
maintainer-tag:
	git tag -a "v$(VERSION)" -m "Version $(VERSION)" -f
