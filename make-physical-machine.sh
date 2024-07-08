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

GUESTFISH_PID=
disk=

cleanup()
{
  set +e
  if test -n "$GUESTFISH_PID"; then
    guestfish --remote -- exit >/dev/null 2>&1
    GUESTFISH_PID=
  fi
  if test -n "$disk"; then
    rm -f -- "$disk"
    disk=
  fi
}

trap cleanup EXIT

output=$1
outdir=$(dirname -- "$output")
disk=$(mktemp -p "$outdir" physical-machine.tmp.XXXXXXXXXX)
# Delay the SELinux relabeling.
virt-builder --format raw -o "$disk" --root-password password:p2v-phys \
  --no-selinux-relabel fedora-39

# Start a guestfish server on the disk image, so that each of the several
# UUID-manipulation commands below not need a separate guestfish launch.
guestfish_set_pid=$(guestfish --listen --format=raw --add "$disk")
eval "$guestfish_set_pid"
guestfish --remote -- run

# The array below open-codes the partition:filesystem layout of the virt-builder
# disk template used above. Whenever you bump the Fedora version, update the
# array below as needed.
#
# We cannot use inspection either before or after the filesystem UUID changes:
#
# - the UUID of a mounted filesystem cannot be changed (at least with XFS);
#
# - right after the UUID changes, inspection does not work correctly, as the new
#   fs UUIDs are out of sync with the UUID references in those config files that
#   inspection relies upon.
#
# Note that the order of entries is important too: the mount points must be
# listed in dependency order (put the dependees first, the dependants last).
fsdevs=(/dev/sda3:/ /dev/sda2:/boot)

# For each filesystem:
#
# - regenerate the UUID,
#
# - produce a sed command (scriptlet) that performs the same UUID replacement in
#   a text file,
#
# - mount the filesystem.
#
# Note that later we're going to rely on the fact that the generated sed
# commands *require no quoting* on the shell command line.
declare -a sed_script
sed_idx=0
for fsdev in "${fsdevs[@]}"; do
  device=${fsdev%:*}
  mountpoint=${fsdev#*:}

  old_uuid=$(guestfish --remote -- get-uuid "$device")
  guestfish --remote -- set-uuid-random "$device"
  new_uuid=$(guestfish --remote -- get-uuid "$device")

  sed_script[sed_idx++]=-e
  sed_script[sed_idx++]=s/$old_uuid/$new_uuid/ig

  guestfish --remote -- mount "$device" "$mountpoint"
done

# Prepare the UUID replacement shell command for the appliance.
refresh_uuid_refs=(find /boot /etc -type f -print0 '|'
                   xargs -0 -r -- sed -i "${sed_script[@]}" --)

# Passing the shell command to the appliance is where we rely on the fact that
# the sed commands for replacing UUIDs require no quoting.
guestfish --remote -- sh "${refresh_uuid_refs[*]}"

# Tear down the guestfish server before we use virt-customize.
waitpid=$GUESTFISH_PID
guestfish --remote -- exit
GUESTFISH_PID=
while kill -s 0 -- "$waitpid" 2>/dev/null; do
  sleep 1
done

# Reapply the SELinux labels now. Use virt-customize for this, rather than
# guestfish's "selinux-relabel", as virt-customize contains some heavy logic
# related to "specfile". Inspection does work here, because the config files are
# in sync again with the filesystem UUIDs.
virt-customize --format raw --add "$disk" --selinux-relabel

# We're done; rename the temporary disk image to the expected output file.
mv -- "$disk" "$output"
disk=
