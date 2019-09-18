#!/bin/bash -
# libguestfs
# Copyright (C) 2014-2019 Red Hat Inc.
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

# Most of the tests written in shell script source this file for
# useful functions.
#
# To include this file, the test must do:
#
#   $TEST_FUNCTIONS
#
# (this macro is defined in subdir-rules.mk).

# Clean up the environment in every test script.
unset CDPATH
export LANG=C

# When test-functions.sh is invoked, a list of variables is passed as
# parameters, so we eval those to define the variables.
while [ $# -ge 1 ]; do eval "$1"; shift; done

# Skip if $SKIP_<script_name> environment variable is set.
# Every test should call this function first.
skip_if_skipped ()
{
    local v
    if [ -n "$1" ]; then
        v="SKIP_$(basename $1 | tr a-z.- A-Z__)"
    else
        v="SKIP_$(basename $0 | tr a-z.- A-Z__)"
    fi
    if [ -n "${!v}" ]; then
        echo "$(basename $0): test skipped because \$$v is set"
        exit 77
    fi
    echo "$(basename $0): info: you can skip this test by setting $v=1"
}

# Skip if the current libguestfs backend is $1.
# eg. skip_if_backend uml
skip_if_backend ()
{
    local b="$(guestfish get-backend)"
    case "$1" in
        # Some magic happens for $1 == libvirt.
        libvirt)
            if [ "$b" = "libvirt" ] || [[ "$b" =~ ^libvirt: ]]; then
                echo "$(basename $0): test skipped because the current backend is $b"
                exit 77
            fi
            ;;
        *)
            if [ "$b" = "$1" ]; then
                echo "$(basename $0): test skipped because the current backend is $b"
                exit 77
            fi
            ;;
    esac
}

# Skip if the current arch != $1.
skip_unless_arch ()
{
    local m="$(uname -m)"
    case "$1" in
        # Some magic happens for some architectures.
        arm)
            if [[ ! "$m" =~ ^arm ]]; then
                echo "$(basename $0): test skipped because the current architecture ($m) is not arm (32 bit)"
                exit 77
            fi
            ;;
        i?86)
            if [[ ! "$m" =~ ^i?86 ]]; then
                echo "$(basename $0): test skipped because the current architecture ($m) is not $1"
                exit 77
            fi
            ;;
        *)
            if [ "$m" != "$1" ]; then
                echo "$(basename $0): test skipped because the current architecture ($m) is not $1"
                exit 77
            fi
            ;;
    esac
}

# Run an external command and skip if the command fails.  This can be
# used to test if a command exists.  Normally you should use
# `cmd --help' or `cmd --version' or similar.
skip_unless ()
{
    if ! "$@"; then
        echo "$(basename $0): test skipped because $1 is not available"
        exit 77
    fi
}

# Slow tests should always call this function.  (See p2v-hacking(1)).
slow_test ()
{
    if [ -z "$SLOW" ]; then
        echo "$(basename $0): use 'make check-slow' to run this test"
        exit 77
    fi
}
