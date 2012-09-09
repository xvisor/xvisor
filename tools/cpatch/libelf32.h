
#ifndef __LIBELF32_H_
#define __LIBELF32_H_

#include <inttypes.h>

/* Types required by elf.h */
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

#include <elf.h>

typedef struct elf32_file {
	int fd;
	int is_be;
	Elf32_Ehdr *hdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr;
	char *strtbl;
} Elf32_File;

Elf32_File *Elf32_Open(const char *filename, int is_be);
int Elf32_Close(Elf32_File *elf);

int Elf32_Phdr_Count(Elf32_File *elf);
Elf32_Phdr* Elf32_Phdr_Get(Elf32_File *elf, int phindex);

int Elf32_Shdr_Count(Elf32_File *elf);
Elf32_Shdr* Elf32_Shdr_Get(Elf32_File *elf, int shindex);
const char* Elf32_Shdr_Name(Elf32_File *elf, int shindex);
Elf32_Shdr* Elf32_Shdr_Find(Elf32_File *elf, const char *shname);
int Elf32_Shdr_Read32(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Word *word);
int Elf32_Shdr_Read16(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Half *half);
int Elf32_Shdr_Write32(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Word word);
int Elf32_Shdr_Write16(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Half half);

#endif
