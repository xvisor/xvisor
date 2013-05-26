/**
 * Copyright (c) 2012 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file cpu_interrupt_handler.S
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Low-level interrupt handling routines.
 */

#include <cpu_asm_macros.h>

.section ".text", "ax"

/*
 * Stack State at the entry of exception.
 *
 *      |               |
 *      |               |
 *      +---------------+
 *      |      SS       | +40
 *      +---------------+
 *      |     RSP       | +32
 *      +---------------+
 *      |    RFLAGS     | +24
 *      +---------------+
 *      |      CS       | +16
 *      +---------------+
 *      |     RIP       | +08
 *      +---------------+
 *      |  HW Err Code  | +00
 *      +---------------+ <--- Here and above CU saves.
 *      |     r15       |
 *      |     r14       |
 *      |     r13       |
 *      |     r12       |
 *      |     r11       |
 *      |     r10       |
 *      |     r9        |
 *      |     r8        |
 *      |     rbp       |
 *      |     rsi       |
 *      |     rdi       |
 *      |     rdx       |
 *      |     rcx       |
 *      |     rbx       |
 *      |     rax       |
 *      +---------------+
 */
EXCEPTION_HANDLER(_exception_div_error) /* division error */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_debug) /* debug exception */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_nmi) /* NMI */
END_EXCEPTION_HANDLER

.extern do_breakpoint
EXCEPTION_HANDLER(_exception_bp) /* Breat point */
	movq	$3, %rdi /* first arg as int num */
	movq	%rsp, %rsi /* pt_regs */
	callq	do_breakpoint
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_ovf) /* overflow */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_bounds) /* bounds */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_inval_opc) /* invalid opc. */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_no_dev) /* dev not avail */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_double_fault) /* double fault */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_coproc_overrun) /* coproc seg overrn */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_inval_tss) /* invalid tss */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_missing_seg) /* seg not present */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_missing_stack) /* stack seg. */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_gpf) /* GPF */
	movq	$13, %rdi /* first arg as int num */
	movq	%rsp, %rsi /* pt_regs */
	callq	do_gpf
END_EXCEPTION_HANDLER

.extern do_page_fault
EXCEPTION_HANDLER(_exception_page_fault) /* page fault */
	movq  120(%rsp), %rdi /* error code as first argument */
	movq  %rsp, %rsi /* trap frame */
	callq do_page_fault
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_coproc_err) /* coproc error */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_align_check) /* alignment check */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_machine_check) /* machine check */
END_EXCEPTION_HANDLER

EXCEPTION_HANDLER(_exception_simd_err) /* SIMD coproc error */
END_EXCEPTION_HANDLER

.extern do_generic_int_handler
EXCEPTION_HANDLER(_generic_handler) /* other interrupts */
	movq	%rsp, %rdi
	callq	do_generic_int_handler
END_EXCEPTION_HANDLER

BUILD_IRQ(32,do_generic_int_handler)
BUILD_IRQ(33,do_generic_int_handler)
BUILD_IRQ(34,do_generic_int_handler)
BUILD_IRQ(35,do_generic_int_handler)
BUILD_IRQ(36,do_generic_int_handler)
BUILD_IRQ(37,do_generic_int_handler)
BUILD_IRQ(38,do_generic_int_handler)
BUILD_IRQ(39,do_generic_int_handler)
BUILD_IRQ(40,do_generic_int_handler)
BUILD_IRQ(41,do_generic_int_handler)
BUILD_IRQ(42,do_generic_int_handler)
BUILD_IRQ(43,do_generic_int_handler)
BUILD_IRQ(44,do_generic_int_handler)
BUILD_IRQ(45,do_generic_int_handler)
BUILD_IRQ(46,do_generic_int_handler)
BUILD_IRQ(47,do_generic_int_handler)
BUILD_IRQ(48,do_generic_int_handler)
BUILD_IRQ(49,do_generic_int_handler)
BUILD_IRQ(50,do_generic_int_handler)
BUILD_IRQ(51,do_generic_int_handler)
BUILD_IRQ(52,do_generic_int_handler)
BUILD_IRQ(53,do_generic_int_handler)
BUILD_IRQ(54,do_generic_int_handler)
BUILD_IRQ(55,do_generic_int_handler)
BUILD_IRQ(56,do_generic_int_handler)
BUILD_IRQ(57,do_generic_int_handler)
BUILD_IRQ(58,do_generic_int_handler)
BUILD_IRQ(59,do_generic_int_handler)
BUILD_IRQ(60,do_generic_int_handler)
BUILD_IRQ(61,do_generic_int_handler)
BUILD_IRQ(62,do_generic_int_handler)
BUILD_IRQ(63,do_generic_int_handler)
BUILD_IRQ(64,do_generic_int_handler)
BUILD_IRQ(65,do_generic_int_handler)
BUILD_IRQ(66,do_generic_int_handler)
BUILD_IRQ(67,do_generic_int_handler)
BUILD_IRQ(68,do_generic_int_handler)
BUILD_IRQ(69,do_generic_int_handler)
BUILD_IRQ(70,do_generic_int_handler)
BUILD_IRQ(71,do_generic_int_handler)
BUILD_IRQ(72,do_generic_int_handler)
BUILD_IRQ(73,do_generic_int_handler)
BUILD_IRQ(74,do_generic_int_handler)
BUILD_IRQ(75,do_generic_int_handler)
BUILD_IRQ(76,do_generic_int_handler)
BUILD_IRQ(77,do_generic_int_handler)
BUILD_IRQ(78,do_generic_int_handler)
BUILD_IRQ(79,do_generic_int_handler)
BUILD_IRQ(80,do_generic_int_handler)
BUILD_IRQ(81,do_generic_int_handler)
BUILD_IRQ(82,do_generic_int_handler)
BUILD_IRQ(83,do_generic_int_handler)
BUILD_IRQ(84,do_generic_int_handler)
BUILD_IRQ(85,do_generic_int_handler)
BUILD_IRQ(86,do_generic_int_handler)
BUILD_IRQ(87,do_generic_int_handler)
BUILD_IRQ(88,do_generic_int_handler)
BUILD_IRQ(89,do_generic_int_handler)
BUILD_IRQ(90,do_generic_int_handler)
BUILD_IRQ(91,do_generic_int_handler)
BUILD_IRQ(92,do_generic_int_handler)
BUILD_IRQ(93,do_generic_int_handler)
BUILD_IRQ(94,do_generic_int_handler)
BUILD_IRQ(95,do_generic_int_handler)
BUILD_IRQ(96,do_generic_int_handler)
BUILD_IRQ(97,do_generic_int_handler)
BUILD_IRQ(98,do_generic_int_handler)
BUILD_IRQ(99,do_generic_int_handler)
BUILD_IRQ(100,do_generic_int_handler)
BUILD_IRQ(101,do_generic_int_handler)
BUILD_IRQ(102,do_generic_int_handler)
BUILD_IRQ(103,do_generic_int_handler)
BUILD_IRQ(104,do_generic_int_handler)
BUILD_IRQ(105,do_generic_int_handler)
BUILD_IRQ(106,do_generic_int_handler)
BUILD_IRQ(107,do_generic_int_handler)
BUILD_IRQ(108,do_generic_int_handler)
BUILD_IRQ(109,do_generic_int_handler)
BUILD_IRQ(110,do_generic_int_handler)
BUILD_IRQ(111,do_generic_int_handler)
BUILD_IRQ(112,do_generic_int_handler)
BUILD_IRQ(113,do_generic_int_handler)
BUILD_IRQ(114,do_generic_int_handler)
BUILD_IRQ(115,do_generic_int_handler)
BUILD_IRQ(116,do_generic_int_handler)
BUILD_IRQ(117,do_generic_int_handler)
BUILD_IRQ(118,do_generic_int_handler)
BUILD_IRQ(119,do_generic_int_handler)
BUILD_IRQ(120,do_generic_int_handler)
BUILD_IRQ(121,do_generic_int_handler)
BUILD_IRQ(122,do_generic_int_handler)
BUILD_IRQ(123,do_generic_int_handler)
BUILD_IRQ(124,do_generic_int_handler)
BUILD_IRQ(125,do_generic_int_handler)
BUILD_IRQ(126,do_generic_int_handler)
BUILD_IRQ(127,do_generic_int_handler)
BUILD_IRQ(128,do_generic_int_handler)
BUILD_IRQ(129,do_generic_int_handler)
BUILD_IRQ(130,do_generic_int_handler)
BUILD_IRQ(131,do_generic_int_handler)
BUILD_IRQ(132,do_generic_int_handler)
BUILD_IRQ(133,do_generic_int_handler)
BUILD_IRQ(134,do_generic_int_handler)
BUILD_IRQ(135,do_generic_int_handler)
BUILD_IRQ(136,do_generic_int_handler)
BUILD_IRQ(137,do_generic_int_handler)
BUILD_IRQ(138,do_generic_int_handler)
BUILD_IRQ(139,do_generic_int_handler)
BUILD_IRQ(140,do_generic_int_handler)
BUILD_IRQ(141,do_generic_int_handler)
BUILD_IRQ(142,do_generic_int_handler)
BUILD_IRQ(143,do_generic_int_handler)
BUILD_IRQ(144,do_generic_int_handler)
BUILD_IRQ(145,do_generic_int_handler)
BUILD_IRQ(146,do_generic_int_handler)
BUILD_IRQ(147,do_generic_int_handler)
BUILD_IRQ(148,do_generic_int_handler)
BUILD_IRQ(149,do_generic_int_handler)
BUILD_IRQ(150,do_generic_int_handler)
BUILD_IRQ(151,do_generic_int_handler)
BUILD_IRQ(152,do_generic_int_handler)
BUILD_IRQ(153,do_generic_int_handler)
BUILD_IRQ(154,do_generic_int_handler)
BUILD_IRQ(155,do_generic_int_handler)
BUILD_IRQ(156,do_generic_int_handler)
BUILD_IRQ(157,do_generic_int_handler)
BUILD_IRQ(158,do_generic_int_handler)
BUILD_IRQ(159,do_generic_int_handler)
BUILD_IRQ(160,do_generic_int_handler)
BUILD_IRQ(161,do_generic_int_handler)
BUILD_IRQ(162,do_generic_int_handler)
BUILD_IRQ(163,do_generic_int_handler)
BUILD_IRQ(164,do_generic_int_handler)
BUILD_IRQ(165,do_generic_int_handler)
BUILD_IRQ(166,do_generic_int_handler)
BUILD_IRQ(167,do_generic_int_handler)
BUILD_IRQ(168,do_generic_int_handler)
BUILD_IRQ(169,do_generic_int_handler)
BUILD_IRQ(170,do_generic_int_handler)
BUILD_IRQ(171,do_generic_int_handler)
BUILD_IRQ(172,do_generic_int_handler)
BUILD_IRQ(173,do_generic_int_handler)
BUILD_IRQ(174,do_generic_int_handler)
BUILD_IRQ(175,do_generic_int_handler)
BUILD_IRQ(176,do_generic_int_handler)
BUILD_IRQ(177,do_generic_int_handler)
BUILD_IRQ(178,do_generic_int_handler)
BUILD_IRQ(179,do_generic_int_handler)
BUILD_IRQ(180,do_generic_int_handler)
BUILD_IRQ(181,do_generic_int_handler)
BUILD_IRQ(182,do_generic_int_handler)
BUILD_IRQ(183,do_generic_int_handler)
BUILD_IRQ(184,do_generic_int_handler)
BUILD_IRQ(185,do_generic_int_handler)
BUILD_IRQ(186,do_generic_int_handler)
BUILD_IRQ(187,do_generic_int_handler)
BUILD_IRQ(188,do_generic_int_handler)
BUILD_IRQ(189,do_generic_int_handler)
BUILD_IRQ(190,do_generic_int_handler)
BUILD_IRQ(191,do_generic_int_handler)
BUILD_IRQ(192,do_generic_int_handler)
BUILD_IRQ(193,do_generic_int_handler)
BUILD_IRQ(194,do_generic_int_handler)
BUILD_IRQ(195,do_generic_int_handler)
BUILD_IRQ(196,do_generic_int_handler)
BUILD_IRQ(197,do_generic_int_handler)
BUILD_IRQ(198,do_generic_int_handler)
BUILD_IRQ(199,do_generic_int_handler)
BUILD_IRQ(200,do_generic_int_handler)
BUILD_IRQ(201,do_generic_int_handler)
BUILD_IRQ(202,do_generic_int_handler)
BUILD_IRQ(203,do_generic_int_handler)
BUILD_IRQ(204,do_generic_int_handler)
BUILD_IRQ(205,do_generic_int_handler)
BUILD_IRQ(206,do_generic_int_handler)
BUILD_IRQ(207,do_generic_int_handler)
BUILD_IRQ(208,do_generic_int_handler)
BUILD_IRQ(209,do_generic_int_handler)
BUILD_IRQ(210,do_generic_int_handler)
BUILD_IRQ(211,do_generic_int_handler)
BUILD_IRQ(212,do_generic_int_handler)
BUILD_IRQ(213,do_generic_int_handler)
BUILD_IRQ(214,do_generic_int_handler)
BUILD_IRQ(215,do_generic_int_handler)
BUILD_IRQ(216,do_generic_int_handler)
BUILD_IRQ(217,do_generic_int_handler)
BUILD_IRQ(218,do_generic_int_handler)
BUILD_IRQ(219,do_generic_int_handler)
BUILD_IRQ(220,do_generic_int_handler)
BUILD_IRQ(221,do_generic_int_handler)
BUILD_IRQ(222,do_generic_int_handler)
BUILD_IRQ(223,do_generic_int_handler)
BUILD_IRQ(224,do_generic_int_handler)
BUILD_IRQ(225,do_generic_int_handler)
BUILD_IRQ(226,do_generic_int_handler)
BUILD_IRQ(227,do_generic_int_handler)
BUILD_IRQ(228,do_generic_int_handler)
BUILD_IRQ(229,do_generic_int_handler)
BUILD_IRQ(230,do_generic_int_handler)
BUILD_IRQ(231,do_generic_int_handler)
BUILD_IRQ(232,do_generic_int_handler)
BUILD_IRQ(233,do_generic_int_handler)
BUILD_IRQ(234,do_generic_int_handler)
BUILD_IRQ(235,do_generic_int_handler)
BUILD_IRQ(236,do_generic_int_handler)
BUILD_IRQ(237,do_generic_int_handler)
BUILD_IRQ(238,do_generic_int_handler)
BUILD_IRQ(239,do_generic_int_handler)
BUILD_IRQ(240,do_generic_int_handler)
BUILD_IRQ(241,do_generic_int_handler)
BUILD_IRQ(242,do_generic_int_handler)
BUILD_IRQ(243,do_generic_int_handler)
BUILD_IRQ(244,do_generic_int_handler)
BUILD_IRQ(245,do_generic_int_handler)
BUILD_IRQ(246,do_generic_int_handler)
BUILD_IRQ(247,do_generic_int_handler)
BUILD_IRQ(248,do_generic_int_handler)
BUILD_IRQ(249,do_generic_int_handler)
BUILD_IRQ(250,do_generic_int_handler)
BUILD_IRQ(251,do_generic_int_handler)
BUILD_IRQ(252,do_generic_int_handler)
BUILD_IRQ(253,do_generic_int_handler)
BUILD_IRQ(254,do_generic_int_handler)
BUILD_IRQ(255,do_generic_int_handler)
