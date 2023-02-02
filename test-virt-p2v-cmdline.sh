#!/bin/bash -
# libguestfs virt-p2v test script
# Copyright (C) 2015 Red Hat Inc.
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

# Test virt-p2v command line parsing in non-GUI mode.

set -e

$TEST_FUNCTIONS
skip_if_skipped

out=test-virt-p2v-cmdline.out
rm -f $out

# The Linux kernel command line.
P2V_OPTS=(
  p2v.server=localhost
  p2v.port=123
  p2v.username=user
  p2v.password=secret
  p2v.skip_test_connection
  p2v.name=test
  p2v.vcpu.cores=4
  p2v.memory=1G
  p2v.disks=sda,sdb,sdc
  p2v.removable=sdd
  p2v.interfaces=eth0,eth1
  p2v.o=local
  p2v.oa=sparse
  p2v.oc=qemu:///session
  p2v.of=raw
  p2v.os=/var/tmp
  p2v.network=em1:wired,other
  p2v.dump_config_and_exit
)
$VG virt-p2v --cmdline="${P2V_OPTS[*]}" > $out

# For debugging purposes.
cat $out

# Check the output contains what we expect.
grep "^remote\.server.*localhost" $out
grep "^remote\.port.*123" $out
grep "^auth\.username.*user" $out
grep "^auth\.sudo.*false" $out
grep "^guestname.*test" $out
grep "^vcpu.phys_topo.*false" $out
grep "^vcpu.cores.*4" $out
grep "^memory.*"$((1024*1024*1024)) $out
grep "^disks.*sda sdb sdc" $out
grep "^removable.*sdd" $out
grep "^interfaces.*eth0 eth1" $out
grep "^network_map.*em1:wired other" $out
grep "^output\.type.*local" $out
grep "^output\.allocation.*sparse" $out
grep "^output\.connection.*qemu:///session" $out
grep "^output\.format.*raw" $out
grep "^output\.storage.*/var/tmp" $out

rm $out
