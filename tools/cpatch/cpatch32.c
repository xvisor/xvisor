
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "libelf32.h"

#define CPATCH32_SEPARATOR		','
#define CPATCH32_MAX_LINE_SIZE		256
#define CPATCH32_MAX_TOKEN_SIZE		64
#define CPATCH32_MAX_TOKEN_COUNT	64

void strtrim(char *str) 
{
	size_t   len;
	char     *frontp;
	char     *endp;

	/* Sanity check */
	if (str == NULL) {
		return;
	}

	/* handle empty string special case */
	if (str[0] == '\0') {
		return;
	}

	len  = strlen(str);
	frontp = str - 1;
	endp = str + len;

	/*
	 * Move the front and back pointers to address the first
         * non-whitespace characters from each end.
	 */
	while (isspace(*(++frontp)));
	while ((endp > frontp) && isspace(*(--endp)));

	if (frontp != str) {
		/* Move the string to the original ptr */
		memmove(str, frontp, endp - frontp + 1);
	}

	/* end the string */
	*(str + (endp - frontp + 1)) = '\0';
}

unsigned int strsplit(char *str, char sep, char *tok[], unsigned int maxtok,
		      unsigned int maxtoksz)
{
	char delim[2] = {sep, '\0'};
	char *token;
	unsigned int cnt = 0;

	while ((cnt < maxtok) && (token = strtok(str, delim))) {
		str = NULL;
		strncpy(tok[cnt], token, maxtoksz);
		tok[cnt][maxtoksz - 1] = '\0';
		cnt++;
	}

	return cnt;
}

unsigned int xtoi(char *str)
{
	return (unsigned int)strtol(str, NULL, 16);
}

int main (int argc, char **argv)
{
	Elf32_File *elf;
	Elf32_Shdr *cursh = NULL;
	char line[CPATCH32_MAX_LINE_SIZE];
	char *toks[CPATCH32_MAX_TOKEN_COUNT];
	FILE *script;
	int ret=0;
	unsigned int tok, tokcnt;

	if ( argc < 3) {
		printf (" Usage: %s <elf_file> <elf_is_be> "
			"[<elf_patch_script>]\n", argv[0]);
		ret = -1;
		goto err_return;
	}

	for(tok=0;tok<CPATCH32_MAX_TOKEN_COUNT;tok++) {
		toks[tok] = malloc(CPATCH32_MAX_TOKEN_SIZE);
	}

	elf = Elf32_Open(argv[1], atoi(argv[2]));
	if(!elf) {
		printf("Error: %s ELF cannot be opened\n", argv[1]);
		ret = -1;
		goto err_noelf;
	}

	if(argc >= 4) {
		script = fopen(argv[3],"r");
	} else {
		script = stdin;
	}
	if(!script) {
		printf("Error: %s script cannot be opened\n", argv[3]);
		ret = -1;
		goto err_noscript;
	}

	while(fgets(line, sizeof(line), script) != NULL)  { /* read a line */

		strtrim(line);

		/* If line is a comment then skip to next line */
		if(line[0]=='#') {
			continue;
		}

		/* Split the line based on seperator */
		tokcnt = strsplit(line, CPATCH32_SEPARATOR, toks,
				  CPATCH32_MAX_TOKEN_COUNT,
				  CPATCH32_MAX_TOKEN_SIZE);
		if(tokcnt==0) {
			continue;
		}

		/* Trim the tokens of a line */
		for(tok=0;tok<tokcnt;tok++) {
			strtrim(toks[tok]);
		}

		if(strcmp(toks[0], "section")==0 && 1<tokcnt) {
			cursh = Elf32_Shdr_Find(elf, toks[1]);
			if(cursh) {
				printf("Patching %s ", toks[1]);
				printf("(Address: 0x%08x, ", cursh->sh_addr);
				printf("Offset: 0x%08x, ", cursh->sh_offset);
				printf("Size: 0x%x)\n", cursh->sh_size);
			}
		} else if(cursh && strcmp(toks[0], "write32")==0 && 2<tokcnt) {
			Elf32_Word old_word, new_word;
			Elf32_Addr addr = xtoi(toks[1]);

			if(Elf32_Shdr_Read32(elf, cursh, addr, &old_word)) {
				printf("    0x%08x: Failed to read\n", addr);
				continue;
			}

			new_word = xtoi(toks[2]);

			if(Elf32_Shdr_Write32(elf, cursh, addr, new_word)) {
				printf("    0x%08x: Failed to write\n", addr);
				continue;
			}

			printf("    0x%08x: 0x%08x -> 0x%08x\n", addr,
			       old_word, new_word);
		} else if(cursh && strcmp(toks[0], "write16")==0 && 2<tokcnt) {
			Elf32_Half old_half, new_half;
			Elf32_Addr addr = xtoi(toks[1]);

			if(Elf32_Shdr_Read16(elf, cursh, addr, &old_half)) {
				printf("    0x%08x: Failed to read\n", addr);
				continue;
			}

			new_half = xtoi(toks[2]);

			if(Elf32_Shdr_Write16(elf, cursh, addr, new_half)) {
				printf("    0x%08x: Failed to write\n", addr);
				continue;
			}

			printf("    0x%08x: 0x%04x -> 0x%04x\n", addr,
			       old_half, new_half);
		}
	}

	fclose(script);
err_noscript:
	Elf32_Close(elf);
err_noelf:
	for(tok=0;tok<CPATCH32_MAX_TOKEN_COUNT;tok++) {
		free(toks[tok]);
	}
err_return:
	return ret;
}
