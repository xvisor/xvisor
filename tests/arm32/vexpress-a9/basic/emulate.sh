#!/bin/bash

# Copyright (c) 2011 Jim Huang.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file emulate.sh
# @author Jim Huang (jserv@0xlab.org)
# @brief execuate and validate Xvisor in QEMU

CUR=`dirname $0`
CMDS=`sed -n -e 's/\(^[^#].*\)/\1/p' test.script`

emulate () {
	qemu-system-arm \
		-M vexpress-a9 \
		-m 512M \
		-kernel $1 \
		-serial stdio \
		-parallel none \
		-display none \
		-monitor null <&0 & pid=$!
}

xvisor_qemu () {
	emulate $1 <<< "
$CMDS
"
	echo "Executing Xvisor in QEMU..."
	(sleep $2; kill $pid; sleep 1; kill -KILL $pid)& timer=$!
	if ! wait $pid; then
		kill $timer 2>/dev/null
		echo
		echo "Xvisor failed to execute in $2 seconds, giving up."
		exit -1
	fi
	kill $timer
}

xvisor_qemu $1 15
