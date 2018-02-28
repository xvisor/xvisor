OUTPUT_ARCH(i8086)			/* i386 for 32 bit, i8086 for 16 bit       */

/* Set the variable below to the address you want the "main" section, from bios.S, */
/* to be located. The BIOS should be located at the area just below 4GB (4096 MB). */
main_address = 4096M - 64K;		/* Use the last 4K block                   */

/* Set the BIOS size below (both locations) according to your target flash size    */
MEMORY {
	ROM (rx) : org = 4096M - 64, len = 64K
}

/* You shouldn't have to modify anything below this                                */
SECTIONS {
	ENTRY(init)			/* To avoid antivirus false positives      */
	/* Sanity check on the init entrypoint                                     */
	_assert = ASSERT(init >= 4096M - 64K, 
		"'init' entrypoint too low - it needs to reside in the last 64K.");
	.begin : {	/* NB: ld section labels MUST be 6 letters or less         */
		*(begin)
	} >ROM		/* Places this first section at the beginning of the ROM   */
	/* the --gap-fill option of objcopy will be used to fill the gap to .main  */
	.main main_address : {
		*(main)
	}
	.reset 4096M - 0x10 : {	 	/* First instruction executed after reset  */
		*(reset)
	}
	.igot 0 : {			/* Required on Linux                       */
		*(.igot.plt)
	}
}
