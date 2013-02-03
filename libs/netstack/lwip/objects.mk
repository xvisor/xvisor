#/**
# Copyright (c) 2013 Anup Patel.
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
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of lwip objects to be build
# */

libs-cflags-y+= -Wno-char-subscripts
libs-cppflags-y+= -I$(libs_dir)/netstack/lwip/lwip-1.4.1/src/include
libs-cppflags-y+= -I$(libs_dir)/netstack/lwip/lwip-1.4.1/src/include/ipv4
libs-cppflags-y+= -I$(libs_dir)/netstack/lwip/port/include
libs-objs-$(CONFIG_LWIP)+= netstack/lwip/lwip_lib.o

lwip_lib-y+= lwip-1.4.1/src/core/def.o
lwip_lib-y+= lwip-1.4.1/src/core/dhcp.o
lwip_lib-y+= lwip-1.4.1/src/core/dns.o
lwip_lib-y+= lwip-1.4.1/src/core/init.o
lwip_lib-y+= lwip-1.4.1/src/core/mem.o
lwip_lib-y+= lwip-1.4.1/src/core/memp.o
lwip_lib-y+= lwip-1.4.1/src/core/netif.o
lwip_lib-y+= lwip-1.4.1/src/core/pbuf.o
lwip_lib-y+= lwip-1.4.1/src/core/raw.o
lwip_lib-y+= lwip-1.4.1/src/core/stats.o
lwip_lib-y+= lwip-1.4.1/src/core/sys.o
lwip_lib-y+= lwip-1.4.1/src/core/tcp.o
lwip_lib-y+= lwip-1.4.1/src/core/tcp_in.o
lwip_lib-y+= lwip-1.4.1/src/core/tcp_out.o
lwip_lib-y+= lwip-1.4.1/src/core/timers.o
lwip_lib-y+= lwip-1.4.1/src/core/udp.o

lwip_lib-y+= lwip-1.4.1/src/core/ipv4/autoip.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/icmp.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/igmp.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/inet.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/inet_chksum.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/ip_addr.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/ip.o
lwip_lib-y+= lwip-1.4.1/src/core/ipv4/ip_frag.o

lwip_lib-y+= lwip-1.4.1/src/core/snmp/asn1_dec.o
lwip_lib-y+= lwip-1.4.1/src/core/snmp/asn1_enc.o
lwip_lib-y+= lwip-1.4.1/src/core/snmp/mib2.o
lwip_lib-y+= lwip-1.4.1/src/core/snmp/mib_structs.o
lwip_lib-y+= lwip-1.4.1/src/core/snmp/msg_in.o
lwip_lib-y+= lwip-1.4.1/src/core/snmp/msg_out.o

lwip_lib-y+= lwip-1.4.1/src/netif/etharp.o

lwip_lib-y+= lwip-1.4.1/src/api/api_lib.o
lwip_lib-y+= lwip-1.4.1/src/api/api_msg.o
lwip_lib-y+= lwip-1.4.1/src/api/err.o
lwip_lib-y+= lwip-1.4.1/src/api/netbuf.o
lwip_lib-y+= lwip-1.4.1/src/api/netdb.o
lwip_lib-y+= lwip-1.4.1/src/api/netifapi.o
lwip_lib-y+= lwip-1.4.1/src/api/sockets.o
lwip_lib-y+= lwip-1.4.1/src/api/tcpip.o

lwip_lib-y+= port/sys_arch.o
lwip_lib-y+= port/netstack.o

%/lwip_lib.o: $(foreach obj,$(lwip_lib-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/lwip_lib.dep: $(foreach dep,$(lwip_lib-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
