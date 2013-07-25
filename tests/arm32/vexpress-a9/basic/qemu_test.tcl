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
# @brief Automation script to test the Xvisor commands and Basic Firmware
# */

set qemu_img [lrange $argv 0 0] 
set xvisor_prompt "XVisor#"
set arm_prompt "basic#"

# start the test 
spawn qemu-system-arm -M vexpress-a9 -display none -serial stdio -kernel $qemu_img

expect $xvisor_prompt
send -- "help\r"
expect $xvisor_prompt
set help_out $expect_out(buffer)
if { [string compare $help_out ""] == 0 } {
# only checks Empty lines
    puts "\n :: HELP TESTCASE FAIL :: \n\n"

} else {
puts "\n :: HELP TESTCASE PASS :: \n\n"
}

send -- "version\r"
expect $xvisor_prompt

set version_out $expect_out(buffer)
if { [string first "Version" $version_out] > -1 } {
	puts "\n :: Version TESTCASE PASS :: \n\n"
} else {
	puts "\n :: Version TESTCASE FAIL :: \n\n"
}


send -- "reset\r"
expect $xvisor_prompt

set reset_out $expect_out(buffer)
if { [string first "Initialize Board Final" $reset_out] > -1 } {
        puts "\n :: RESET TESTCASE PASS :: \n\n"
} else {
        puts "\n :: RESET TESTCASE FAIL :: \n\n"
}

send -- "host help\r"
expect $xvisor_prompt

set host_help_out $expect_out(buffer)
if { [string first "host help" $host_help_out] > -1 } {
        puts "\n :: HOST HELP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HOST HELP TESTCASE FAIL :: \n\n"
}

send -- "host vapool stats\r"
expect $xvisor_prompt
set host_vapool_stats_out $expect_out(buffer)
if { [string first "Total Pages" $host_vapool_stats_out] > -1 } {
        puts "\n :: HOST VAPOOL STATS TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HOST VAPOOL STATS TESTCASE FAIL :: \n\n"
}

send -- "host vapool bitmap\r"
expect $xvisor_prompt
set host_vapool_bitmap_out $expect_out(buffer)
if { [string first "1 : used" $host_vapool_bitmap_out] > -1 } {
        puts "\n :: HOST VAPOOL BITMAP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HOST VAPOOL BITMAP TESTCASE FAIL :: \n\n"
}

send -- "host ram stats\r"
expect $xvisor_prompt
set host_ram_stats_out $expect_out(buffer)
if { [string first "Total Frames " $host_ram_stats_out] > -1 } {
        puts "\n :: HOST RAM STATS TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HOST RAM STATS TESTCASE FAIL :: \n\n"
}

send -- "host ram bitmap\r"
expect $xvisor_prompt
set host_ram_bitmap_out $expect_out(buffer)
if { [string first "11111111111" $host_ram_bitmap_out] > -1 } {
        puts "\n :: HOST RAM BITMAP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HOST RAM BITMAP TESTCASE FAIL :: \n\n"
}

send -- "devtree help\r"
expect $xvisor_prompt
set devtree_help_out $expect_out(buffer)
if { [string first "devtree print" $devtree_help_out] > -1 } {
        puts "\n :: DEVTREE HELP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: DEVTREE HELP TESTCASE FAIL :: \n\n"
}

send -- "devtree attr show /host/cpus\r"
expect $xvisor_prompt
set devtree_attr_show_out $expect_out(buffer)
if { [string first "cpu_freq_mhz" $devtree_attr_show_out] > -1 } {
        puts "\n :: DEVTREE ATTR SHOW TESTCASE PASS :: \n\n"
} else {
        puts "\n :: DEVTREE ATTR SHOW TESTCASE FAIL :: \n\n"
}

send -- "devtree node show /\r"
expect $xvisor_prompt
set devtree_node_show_out $expect_out(buffer)
if { [string first "vmm" $devtree_node_show_out] > -1 } {
        puts "\n :: DEVTREE NODE SHOW TESTCASE PASS :: \n\n"
} else {
        puts "\n :: DEVTREE NODE SHOW TESTCASE FAIL :: \n\n"
}

send -- "devtree node dump /\r"
expect $xvisor_prompt
set devtree_node_dump_out $expect_out(buffer)
if { [string first "vmm" $devtree_node_dump_out] > -1 } {
        puts "\n :: DEVTREE NODE DUMP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: DEVTREE NODE DUMP TESTCASE FAIL :: \n\n"
}

send -- "guest kick -1\r"
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
if { [string first "ARM VExpress-A9 Basic Firmware" $vserial_bind_out] > -1 } {
        puts "\n :: VSERIAL BIND TESTCASE PASS :: \n\n"
} else {
        puts "\n :: VSERIAL BIND TESTCASE FAIL :: \n\n"
}

send -- "hi\r"
expect $arm_prompt
set hi_out $expect_out(buffer)
if { [string first "hello" $hi_out] > -1 } {
        puts "\n :: HI TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HI TESTCASE FAIL :: \n\n"
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
        puts "\n :: HELP TESTCASE PASS :: \n\n"
} else {
        puts "\n :: HELP TESTCASE FAIL :: \n\n"
}

send -- "mmu_setup\r"
expect $arm_prompt
send -- "mmu_state\r"
expect $arm_prompt
set mmu_state_out $expect_out(buffer)
if { [string first "MMU Enabled" $mmu_state_out] > -1 } {
        puts "\n :: MMU SETUP & MMU STATE TESTCASE PASS :: \n\n"
} else {
        puts "\n :: MMU SETUP & MMU STATE TESTCASE FAIL :: \n\n"
}

send -- "mmu_cleanup\r"
expect $arm_prompt
send -- "mmu_state\r"
expect $arm_prompt
set mmu_state_out $expect_out(buffer)
if { [string first "MMU Disabled" $mmu_state_out] > -1 } {
        puts "\n :: MMU CLEANUP & MMU STATE TESTCASE PASS :: \n\n"
} else {
        puts "\n :: MMU CLEANUP & MMU STATE TESTCASE FAIL :: \n\n"
}

send -- "mmu_test\r"
expect $arm_prompt
set mmu_test_out $expect_out(buffer)
set first_fail [string first "Fail : 0" $mmu_test_out]
set last_fail [string last "Fail : 0" $mmu_test_out]
if { $last_fail > $first_fail } {
        puts "\n :: MMU TEST TESTCASE PASS :: \n\n"
} else {
        puts "\n :: MMU TEST TESTCASE FAIL :: \n\n"
}

send -- "timer\r"
expect $arm_prompt
set timer_out $expect_out(buffer)
if { [string first "Time Stamp:" $timer_out] > -1 } {
        puts "\n :: TIMER TESTCASE PASS :: \n\n"
} else {
        puts "\n :: TIMER TESTCASE FAIL :: \n\n"
}

send -- "dhrystone\r"
expect $arm_prompt
set dhrystone_out $expect_out(buffer)
if { [string first "Dhrystones MIPS:" $dhrystone_out] > -1 } {
        puts "\n :: DHRYSTONE TESTCASE PASS :: \n\n"
	set temp_var [string last ":" $dhrystone_out]
	set temp_var [expr $temp_var + 25 ]
	set DMIPS [string range $dhrystone_out $temp_var end ]
	puts "DMIPS is $DMIPS"
} else {
        puts "\n :: DHRYSTONE TESTCASE FAIL :: \n\n"
}

send -- "\n"

expect "#"
send \003
expect eof

