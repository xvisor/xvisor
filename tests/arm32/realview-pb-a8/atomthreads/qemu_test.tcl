#!/usr/bin/expect -f
#/**
# Copyright (c) 2011 Sanjeev Pandita.
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
# @file qemu_test.tcl
# @author Sanjeev Pandita (san.pandita@gmail.com)
# @brief Automation script to test the atomthreads
# */

set qemu_img [lrange $argv 0 0] 
set xvisor_prompt "XVisor#"
set arm_prompt "basic#"
set atomthreads_test_case_list "kern1 kern2 kern3 kern4 mutex1 mutex2 mutex3 mutex4 mutex5 mutex6 mutex7 mutex8 mutex9 queue1 queue2 queue3 queue4 queue5 queue6 queue7 queue8 queue9 queue10 sem1 sem2 sem3 sem4 sem5 sem6 sem7 sem8 sem9 timer1 timer2 timer3 timer4 timer5 timer6 timer7"

# start the test 
spawn qemu-system-arm -M realview-pb-a8 -m 512M -display none -serial stdio -kernel $qemu_img

expect $xvisor_prompt

send -- "guest kick guest0\r"
expect $xvisor_prompt
set guest_kick_out $expect_out(buffer)
if { [string first "guest0: Kicked" $guest_kick_out] > -1 } {
        puts "\n :: GUEST KICK TESTCASE PASS :: \n\n"
} else {
        puts "\n :: GUEST KICK TESTCASE FAIL :: \n\n"
}

send -- "vserial bind guest0/uart0\r"
expect $arm_prompt
set vserial_bind_out $expect_out(buffer)
if { [string first "ARM Realview PB-A8 Basic Firmware" $vserial_bind_out] > -1 } {
        puts "\n :: VSERIAL BIND KICK TESTCASE PASS :: \n\n"
} else {
        puts "\n :: VSERIAL BIND TESTCASE FAIL :: \n\n"
}

set address 0x40100000
set test_case_cnt 0
foreach item $atomthreads_test_case_list {
	#Append the every item to $text
	puts "Executing the test case :$item:\n"
	send "\033xq"
	expect $xvisor_prompt
	send -- "guest reset guest0;guest kick guest0; vserial bind guest0/uart0 \r"
	expect $arm_prompt
	set cpy_str [format "copy 0x100000 0x%x 0x20000\r" $address ]	
	send -- "$cpy_str"
	expect $arm_prompt
	send -- "go 0x100000\r"
	expect "Reset your board !!!!!"
	set go_out $expect_out(buffer)
	if { [string first "SUCCESS" $go_out] > -1 } {
	        puts "\n :: $item TESTCASE PASS :: \n\n"
	} else {
	        puts "\n :: $item TESTCASE FAIL :: \n\n"
	}

	incr address 0x20000
}

send \003
expect eof

