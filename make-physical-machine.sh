#!/bin/bash -
# Copyright (C) 2022 Red Hat Inc.
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

set -e -u -C

disk=

cleanup()
{
  set +e
  if test -n "$disk"; then
    rm -f -- "$disk"
    disk=
  fi
}

trap cleanup EXIT

output=$1
outdir=$(dirname -- "$output")
disk=$(mktemp -p "$outdir" physical-machine.tmp.XXXXXXXXXX)
virt-builder --format raw -o "$disk" fedora-35
mv -- "$disk" "$output"
disk=
