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
# @file test qemu atomthreads
# @author Sanjeev Pandita (san.pandita@gmail.com)
# @brief Automation script to test the atomthreads
# */

set qemu_img [lrange $argv 0 0] 
set xvisor_prompt "XVisor#"
set arm_prompt "arm-test#"
set atomthreads_test_case_list "kern1 kern2 kern3 kern4 mutex1 mutex2 mutex3 mutex4 mutex5 mutex6 mutex7 mutex8 mutex9 queue1 queue2 queue3 queue4 queue5 queue6 queue7 queue8 queue9 queue10 sem1 sem2 sem3 sem4 sem5 sem6 sem7 sem8 sem9 timer1 timer2 timer3 timer4 timer5 timer6 timer7"

# start the test 
spawn qemu-system-arm -M realview-pb-a8 -display none -serial stdio -kernel $qemu_img

expect $xvisor_prompt

send -- "help\r"
expect $xvisor_prompt
set help_out $expect_out(buffer)
if { [string compare $help_out ""] == 0 } {
# only checks Empty lines
    puts "The Help Command Failed \n :: HELP TESTCASE FAIL :: \n\n"

} else {
puts "The Help Command passed \n :: HELP TESTCASE PASS :: \n\n"
}

send -- "version\r"
expect $xvisor_prompt
set version_out $expect_out(buffer)
if { [string first "Version" $version_out] > -1 } {
	puts "The Version Command passed \n :: Version TESTCASE PASS :: \n\n"
} else {
	puts "The Version Command Failed \n :: Version TESTCASE FAIL :: \n\n"
}


send -- "reset\r"
expect $xvisor_prompt
set reset_out $expect_out(buffer)
if { [string first "Initialize Board Final" $reset_out] > -1 } {
        puts "The reset Command passed \n :: RESET TESTCASE PASS :: \n\n"
} else {
        puts "The reset Command Failed \n :: RESET TESTCASE FAIL :: \n\n"
}

send -- "vapool help\r"
expect $xvisor_prompt
set vapool_help_out $expect_out(buffer)
if { [string first "vapool help" $vapool_help_out] > -1 } {
        puts "The vapool help Command passed \n :: VAPOOL HELP TESTCASE PASS :: \n\n"
} else {
        puts "The vapool help Command Failed \n :: VAPOOL HELP TESTCASE FAIL :: \n\n"
}

send -- "vapool stats\r"
expect $xvisor_prompt
set vapool_stats_out $expect_out(buffer)
if { [string first "Total Pages" $vapool_stats_out] > -1 } {
        puts "The vapool stats Command passed \n :: VAPOOL STATS TESTCASE PASS :: \n\n"
} else {
        puts "The vapool stats Command Failed \n :: VAPOOL STATS TESTCASE FAIL :: \n\n"
}

send -- "vapool bitmap\r"
expect $xvisor_prompt
set vapool_bitmap_out $expect_out(buffer)
if { [string first "1 : used" $vapool_bitmap_out] > -1 } {
        puts "The vapool bitmap Command passed \n :: VAPOOL BITMAP TESTCASE PASS :: \n\n"
} else {
        puts "The vapool bitmap Command Failed \n :: VAPOOL BITMAP TESTCASE FAIL :: \n\n"
}

send -- "ram help\r"
expect $xvisor_prompt
set ram_help_out $expect_out(buffer)
if { [string first "ram bitmap" $ram_help_out] > -1 } {
        puts "The ram help Command passed \n :: RAM HELP TESTCASE PASS :: \n\n"
} else {
        puts "The ram help Command Failed \n :: RAM HELP TESTCASE FAIL :: \n\n"
}

send -- "ram stats\r"
expect $xvisor_prompt
set ram_stats_out $expect_out(buffer)
if { [string first "Total Frames " $ram_stats_out] > -1 } {
        puts "The ram stats Command passed \n :: RAM STATS TESTCASE PASS :: \n\n"
} else {
        puts "The ram stats Command Failed \n :: RAM STATS TESTCASE FAIL :: \n\n"
}

send -- "ram bitmap\r"
expect $xvisor_prompt
set ram_bitmap_out $expect_out(buffer)
if { [string first "11111111111" $ram_bitmap_out] > -1 } {
        puts "The ram bitmap Command passed \n :: RAM BITMAP TESTCASE PASS :: \n\n"
} else {
        puts "The ram bitmap Command Failed \n :: RAM BITMAP TESTCASE FAIL :: \n\n"
}

send -- "devtree help\r"
expect $xvisor_prompt
set devtree_help_out $expect_out(buffer)
if { [string first "devtree print" $devtree_help_out] > -1 } {
        puts "The devtree help Command passed \n :: DEVTREE HELP TESTCASE PASS :: \n\n"
} else {
        puts "The devtree help Command Failed \n :: DEVTREE HELP TESTCASE FAIL :: \n\n"
}

send -- "devtree curpath\r"
expect $xvisor_prompt
set devtree_curpath_out $expect_out(buffer)
if { [string first "/" $devtree_curpath_out] > -1 } {
        puts "The devtree curpath Command passed \n :: DEVTREE CURPATH TESTCASE PASS :: \n\n"
} else {
        puts "The devtree curpath Command Failed \n :: DEVTREE CURPATH TESTCASE FAIL :: \n\n"
}

send -- "devtree chpath /\r"
expect $xvisor_prompt
set devtree_chpath_out $expect_out(buffer)
if { [string first "/" $devtree_chpath_out] > -1 } {
        puts "The devtree chpath Command passed \n :: DEVTREE CHPATH TESTCASE PASS :: \n\n"
} else {
        puts "The devtree chpath Command Failed \n :: DEVTREE CHPATH TESTCASE FAIL :: \n\n"
}

send -- "devtree attrib /host/cpus\r"
expect $xvisor_prompt
set devtree_attrib_out $expect_out(buffer)
if { [string first "cpu_freq_mhz" $devtree_attrib_out] > -1 } {
        puts "The devtree attrib Command passed \n :: DEVTREE ATTRIB TESTCASE PASS :: \n\n"
} else {
        puts "The devtree attrib Command Failed \n :: DEVTREE ATTRIB TESTCASE FAIL :: \n\n"
}

send -- "devtree print /\r"
expect $xvisor_prompt
set devtree_print_out $expect_out(buffer)
if { [string first "vmm" $devtree_print_out] > -1 } {
        puts "The devtree print Command passed \n :: DEVTREE PRINT TESTCASE PASS :: \n\n"
} else {
        puts "The devtree print Command Failed \n :: DEVTREE PRINT TESTCASE FAIL :: \n\n"
}

send -- "guest kick -1\r"
expect $xvisor_prompt
set guest_kick_out $expect_out(buffer)
if { [string first "guest0: Kicked" $guest_kick_out] > -1 } {
        puts "The guest kick Command passed \n :: GUEST KICK TESTCASE PASS :: \n\n"
} else {
        puts "The guest kick Command Failed \n :: GUEST KICK TESTCASE FAIL :: \n\n"
}

send -- "vserial bind guest0/uart0\r"
expect $arm_prompt
set vserial_bind_out $expect_out(buffer)
if { [string first "ARM Realview PB-A8 Basic Test" $vserial_bind_out] > -1 } {
        puts "The vserial bind Command passed \n :: VSERIAL BIND KICK TESTCASE PASS :: \n\n"
} else {
        puts "The vserial bind Command Failed \n :: VSERIAL BIND TESTCASE FAIL :: \n\n"
}

send -- "hi\r"
expect $arm_prompt
set hi_out $expect_out(buffer)
if { [string first "hello" $hi_out] > -1 } {
        puts "The hi Command passed \n :: HI TESTCASE PASS :: \n\n"
} else {
        puts "The hi Command Failed \n :: HI TESTCASE FAIL :: \n\n"
}


send -- "hello\r"
expect $arm_prompt
set hello_out $expect_out(buffer)
if { [string first "hi" $hi_out] > -1 } {
        puts "The hello Command passed \n :: HELLO TESTCASE PASS :: \n\n"
} else {
        puts "The hello Command Failed \n :: HELLO TESTCASE FAIL :: \n\n"
}

send -- "help\r"
expect $arm_prompt
set help_out $expect_out(buffer)
if { [string first "reset" $help_out] > -1 } {
        puts "The help Command passed \n :: HELP TESTCASE PASS :: \n\n"
} else {
        puts "The help Command Failed \n :: HELP TESTCASE FAIL :: \n\n"
}

send -- "mmu_setup\r"
expect $arm_prompt
send -- "mmu_state\r"
expect $arm_prompt
set mmu_state_out $expect_out(buffer)
if { [string first "MMU Enabled" $mmu_state_out] > -1 } {
        puts "The mmu_setup Command passed \n :: MMU SETUP & MMU STATE TESTCASE PASS :: \n\n"
} else {
        puts "The mmu_setup Command Failed \n :: MMU SETUP & MMU STATE TESTCASE FAIL :: \n\n"
}

send -- "mmu_cleanup\r"
expect $arm_prompt
send -- "mmu_state\r"
expect $arm_prompt
set mmu_state_out $expect_out(buffer)
if { [string first "MMU Disabled" $mmu_state_out] > -1 } {
        puts "The mmu_cleanup Command passed \n :: MMU CLEANUP & MMU STATE TESTCASE PASS :: \n\n"
} else {
        puts "The mmu_cleanup Command Failed \n :: MMU CLEANUP & MMU STATE TESTCASE FAIL :: \n\n"
}

send -- "mmu_test\r"
expect $arm_prompt
set mmu_test_out $expect_out(buffer)
set first_fail [string first "Fail : 0" $mmu_test_out]
set last_fail [string last "Fail : 0" $mmu_test_out]
if { $last_fail > $first_fail } {
	puts "The mmu_test Command passed \n :: MMU TEST TESTCASE PASS :: \n\n"
} else {
        puts "The mmu_test Command Failed \n :: MMU TEST TESTCASE FAIL :: \n\n"
}

send -- "sysctl\r"
expect $arm_prompt
set sysctl_out $expect_out(buffer)
if { [string first "SYS_24MHz" $sysctl_out] > -1 } {
        puts "The sysctl Command passed \n :: SYSCTL TESTCASE PASS :: \n\n"
} else {
        puts "The sysctl Command Failed \n :: SYSCTL TESTCASE FAIL :: \n\n"
}

send -- "timer\r"
expect $arm_prompt
set timer_out $expect_out(buffer)
if { [string first "Time Stamp:" $timer_out] > -1 } {
        puts "The timer Command passed \n :: TIMER TESTCASE PASS :: \n\n"
} else {
        puts "The timer Command Failed \n :: TIMER TESTCASE FAIL :: \n\n"
}

send -- "dhrystone\r"
expect $arm_prompt
set dhrystone_out $expect_out(buffer)
if { [string first "Dhrystones MIPS:" $dhrystone_out] > -1 } {
        puts "The Dhrystone Command passed \n :: DHRYSTONE TESTCASE PASS :: \n\n"
	set temp_var [string last ":" $dhrystone_out]
	set temp_var [expr $temp_var + 25 ]
	set DMIPS [string range $dhrystone_out $temp_var end ]
	puts "DMIPS is $DMIPS"
} else {
        puts "The Dhrystone Command Failed \n :: DHRYSTONE TESTCASE FAIL :: \n\n"
}

set address 0x40100000
set test_case_cnt 0
foreach item $atomthreads_test_case_list {
	#Append the every item to $text
	puts "Executing the test case :$item:\n"
	send "\033xq"
	expect $xvisor_prompt
	send -- "guest reset 0;guest kick 0 ; vserial bind guest0/uart0 \r"
	expect $arm_prompt
	set cpy_str [format "copy 0x100000 0x%x 0x20000\r" $address ]	
	send -- "$cpy_str"
	expect $arm_prompt
	send -- "go 0x100000\r"
	expect "Reset your board !!!!!"
	set go_out $expect_out(buffer)
	if { [string first "SUCCESS" $go_out] > -1 } {
	        puts "The go Command passed \n :: $item TESTCASE PASS :: \n\n"
	} else {
	        puts "The go Command Failed \n :: $item TESTCASE FAIL :: \n\n"
	}

	incr address 0x20000
}

send \003
expect eof

