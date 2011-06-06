
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "libelf32.h"

#define CPATCH32_SEPRATOR		','
#define CPATCH32_MAX_LINE_SIZE		256
#define CPATCH32_MAX_TOKEN_SIZE		64
#define CPATCH32_MAX_TOKEN_COUNT	64

void strtrim(char *str) 
{
	int i, len = strlen(str);
	if(len==0) return;
	while(str[0]==' ' || str[0]=='\t') {
		for(i=0;i<len;i++) {
			str[i] = str[i+1];
		}
		len--;
	}
	if(len==0) return;
	while(str[len-1]==' ' || str[0]=='\t' || str[len-1]=='\n') {
		str[len-1] = '\0';
		len--;
		if(len==0) break;
	}
}

int strsplit(char *str, char sep, char **tok, int maxtok) 
{
	int cnt=0, pos=0, chpos=0;
	tok[pos][chpos] = '\0';
	while(*str && pos<maxtok) {
		if(*str==sep) {
			tok[pos][chpos] = '\0';
			cnt++;
			pos++;
			chpos = 0;
			tok[pos][chpos] = '\0';
		} else {
			tok[pos][chpos] = *str;
			chpos++;
		}
		str++;
	}
	cnt++;
	if(tok[cnt-1][0]=='\0') cnt--;
	return cnt;
}

unsigned int xtoi(char *str)
{
	unsigned int ret = 0;
	if(*str=='0' && *(str+1)=='x') {
		str++;
		str++;
	}
	while(*str) {
		if('0'<=*str && *str<='9') {
			ret = ret*16 + (*str - '0');
		} else if('a'<=*str && *str<='f') {
			ret = ret*16 + (10 + *str - 'a');
		} else if('A'<=*str && *str<='F') {
			ret = ret*16 + (10 + *str - 'A');
		}
		str++;
	}
	return ret;
}

int main (int argc, char **argv)
{
	Elf32_File *elf;
	Elf32_Shdr *cursh;
	Elf32_Addr addr;
	Elf32_Word old_word, new_word;
	Elf32_Half old_half, new_half;
	char line[CPATCH32_MAX_LINE_SIZE];
	char *toks[CPATCH32_MAX_TOKEN_COUNT];
	FILE *script;
	int ret=0, tok, tokcnt;

	if ( argc < 3) {
		printf (" Usage: %s <elf_file> <elf_is_be> [<elf_patch_script>]\n", argv[0]);
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

	if(argv[3]!=NULL) {
		script = fopen(argv[3],"r");
	} else {
		script = stdin;
	}
	if(!script) {
		printf("Error: %s script cannot be opened\n", argv[3]);
		ret = -1;
		goto err_noscript;
	}

	cursh = NULL;
	while(fgets(line, sizeof(line), script) != NULL)  { /* read a line */
		strtrim(line);
		/* If line is a comment then skip to next line */
		if(line[0]=='#') {
			continue;
		}
		/* Split the line based on seperator */
		tokcnt = strsplit(line, CPATCH32_SEPRATOR, toks, CPATCH32_MAX_TOKEN_COUNT);
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
			addr = xtoi(toks[1]);
			if(Elf32_Shdr_Read32(elf, cursh, addr, &old_word)) {
				printf("    0x%08x: Failed to read\n", addr);
				continue;
			}
			new_word = xtoi(toks[2]);
			if(Elf32_Shdr_Write32(elf, cursh, addr, new_word)) {
				printf("    0x%08x: Failed to write\n", addr);
				continue;
			}
			printf("    0x%08x: 0x%08x -> 0x%08x\n", addr, old_word, new_word);
		} else if(cursh && strcmp(toks[0], "write16")==0 && 2<tokcnt) {
			addr = xtoi(toks[1]);
			if(Elf32_Shdr_Read16(elf, cursh, addr, &old_half)) {
				printf("    0x%08x: Failed to read\n", addr);
				continue;
			}
			new_half = xtoi(toks[2]);
			if(Elf32_Shdr_Write16(elf, cursh, addr, new_half)) {
				printf("    0x%08x: Failed to write\n", addr);
				continue;
			}
			printf("    0x%08x: 0x%04x -> 0x%04x\n", addr, old_half, new_half);
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
