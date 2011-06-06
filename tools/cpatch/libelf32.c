
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include "libelf32.h"

static uint32_t Elf32_Bswap32(uint32_t data)
{
	return (((data & 0xFF) << 24) |
		((data & 0xFF00) << 8) |
		((data & 0xFF0000) >> 8) |
		((data & 0xFF000000) >> 24));
}

static uint16_t Elf32_Bswap16(uint16_t data)
{
	return (((data & 0xFF) << 8) |
		((data & 0xFF00) >> 8));
}

static int Elf32_Is_CPU_LE(void)
{
	uint32_t tval = 1;
	if(((uint8_t*)&tval)[0]==1)
		return 1;
	return 0;
}

static void Elf32_Ehdr_To_CPU(Elf32_File *elf, Elf32_Ehdr *hdr)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		hdr->e_type=Elf32_Bswap16(hdr->e_type);
		hdr->e_machine=Elf32_Bswap16(hdr->e_machine);
		hdr->e_version=Elf32_Bswap32(hdr->e_version);
		hdr->e_entry=Elf32_Bswap32(hdr->e_entry);  /* Entry point */
		hdr->e_phoff=Elf32_Bswap32(hdr->e_phoff);
		hdr->e_shoff=Elf32_Bswap32(hdr->e_shoff);
		hdr->e_flags=Elf32_Bswap32(hdr->e_flags);
		hdr->e_ehsize=Elf32_Bswap16(hdr->e_ehsize);
		hdr->e_phentsize=Elf32_Bswap16(hdr->e_phentsize);
		hdr->e_phnum=Elf32_Bswap16(hdr->e_phnum);
		hdr->e_shentsize=Elf32_Bswap16(hdr->e_shentsize);
		hdr->e_shnum=Elf32_Bswap16(hdr->e_shnum);
		hdr->e_shstrndx=Elf32_Bswap16(hdr->e_shstrndx);
	}
}

static void Elf32_Phdr_To_CPU(Elf32_File *elf, Elf32_Phdr *phdr)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		phdr->p_type=Elf32_Bswap32(phdr->p_type);
		phdr->p_offset=Elf32_Bswap32(phdr->p_offset);
		phdr->p_vaddr=Elf32_Bswap32(phdr->p_vaddr);
		phdr->p_paddr=Elf32_Bswap32(phdr->p_paddr);
		phdr->p_filesz=Elf32_Bswap32(phdr->p_filesz);
		phdr->p_memsz=Elf32_Bswap32(phdr->p_memsz);
		phdr->p_flags=Elf32_Bswap32(phdr->p_flags);
		phdr->p_align=Elf32_Bswap32(phdr->p_align);
	}
}

static void Elf32_Shdr_To_CPU(Elf32_File *elf, Elf32_Shdr *shdr)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		shdr->sh_name=Elf32_Bswap32(shdr->sh_name);
		shdr->sh_type=Elf32_Bswap32(shdr->sh_type);
		shdr->sh_flags=Elf32_Bswap32(shdr->sh_flags);
		shdr->sh_addr=Elf32_Bswap32(shdr->sh_addr);
		shdr->sh_offset=Elf32_Bswap32(shdr->sh_offset);
		shdr->sh_size=Elf32_Bswap32(shdr->sh_size);
		shdr->sh_link=Elf32_Bswap32(shdr->sh_link);
		shdr->sh_info=Elf32_Bswap32(shdr->sh_info);
		shdr->sh_addralign=Elf32_Bswap32(shdr->sh_addralign);
		shdr->sh_entsize=Elf32_Bswap32(shdr->sh_entsize);
	}
}

Elf32_File *Elf32_Open(const char *filename, int is_be)
{
	int fd, ph, sh;
	size_t bytcnt;
	Elf32_File *elf;

	if ((fd = open(filename, O_RDWR, 0)) < 0)
		printf("Error: open  %s  failed ", filename);

	elf = malloc(sizeof(Elf32_File));

	elf->fd = fd;
	elf->is_be = is_be;
	lseek(fd, 0, SEEK_SET);
	elf->hdr = malloc(sizeof(Elf32_Ehdr));
	bytcnt = read(fd, elf->hdr, sizeof(Elf32_Ehdr));
	Elf32_Ehdr_To_CPU(elf, elf->hdr);

	elf->phdr = malloc(sizeof(Elf32_Phdr)*elf->hdr->e_phnum);
	for(ph=0;ph<elf->hdr->e_phnum;ph++) {
		lseek(fd, elf->hdr->e_phoff + elf->hdr->e_phentsize * ph, SEEK_SET);
		bytcnt = read(fd, &elf->phdr[ph], sizeof(Elf32_Phdr));
		Elf32_Phdr_To_CPU(elf, &elf->phdr[ph]);
	}

	elf->shdr = malloc(sizeof(Elf32_Shdr)*elf->hdr->e_shnum);
	for(sh=0;sh<elf->hdr->e_shnum;sh++) {
		lseek(fd, elf->hdr->e_shoff + elf->hdr->e_shentsize * sh, SEEK_SET);
		bytcnt = read(fd, &elf->shdr[sh], sizeof(Elf32_Shdr));
		Elf32_Shdr_To_CPU(elf, &elf->shdr[sh]);
	}

	elf->strtbl = malloc(elf->shdr[elf->hdr->e_shstrndx].sh_size);
	lseek(fd, elf->shdr[elf->hdr->e_shstrndx].sh_offset, SEEK_SET);
	bytcnt = read(fd, elf->strtbl, elf->shdr[elf->hdr->e_shstrndx].sh_size);

	return elf;
}

int Elf32_Close(Elf32_File* elf) 
{
	if(!elf) {
		return -1;
	}
	close(elf->fd);
	free(elf->strtbl);
	free(elf->shdr);
	free(elf->phdr);
	free(elf->hdr);
	free(elf);
	return 0;
}

int Elf32_Phdr_Count(Elf32_File* elf)
{
	if(!elf) {
		return -1;
	}
	return elf->hdr->e_phnum;
}

Elf32_Phdr* Elf32_Phdr_Get(Elf32_File* elf, int phindex)
{
	if(!elf) {
		return NULL;
	}
	if(phindex<0 || elf->hdr->e_phnum<=phindex) {
		return NULL;
	}
	return &elf->phdr[phindex];
}

int Elf32_Shdr_Count(Elf32_File* elf)
{
	if(!elf) {
		return -1;
	}
	return elf->hdr->e_shnum;
}

Elf32_Shdr* Elf32_Shdr_Get(Elf32_File* elf, int shindex)
{
	if(!elf) {
		return NULL;
	}
	if(shindex<0 || elf->hdr->e_shnum<=shindex) {
		return NULL;
	}
	return &elf->shdr[shindex];
}

const char* Elf32_Shdr_Name(Elf32_File* elf, int shindex)
{
	if(!elf) {
		return NULL;
	}
	if(shindex<0 || elf->hdr->e_shnum<=shindex) {
		return NULL;
	}
	return &elf->strtbl[elf->shdr[shindex].sh_name];
}

Elf32_Shdr* Elf32_Shdr_Find(Elf32_File* elf, const char* shname)
{
	int s;
	if(!elf || !shname) {
		return NULL;
	}
	for(s=0;s<elf->hdr->e_shnum;s++) {
		if(strcmp(Elf32_Shdr_Name(elf,s), shname)==0) {
			return Elf32_Shdr_Get(elf,s);
		}
	}
	return NULL;
}

int Elf32_Shdr_Read32(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Word *word)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	size_t bytcnt;
	if(!elf || !shdr || !word) {
		return -1;
	}
	addr &= ~(0x3);
	if(addr<shdr->sh_addr || (shdr->sh_addr+shdr->sh_size)<=addr) {
		return -1;
	}
	lseek(elf->fd, shdr->sh_offset + (addr - shdr->sh_addr), SEEK_SET);
	bytcnt = read(elf->fd, word, sizeof(Elf32_Word));
	if(bytcnt!=sizeof(Elf32_Word)) {
		return -1;
	}
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		*word = Elf32_Bswap32(*word);
	}
	return 0;
}

int Elf32_Shdr_Read16(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Half *half)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	size_t bytcnt;
	if(!elf || !shdr || !half) {
		return -1;
	}
	addr &= ~(0x1);
	if(addr<shdr->sh_addr || (shdr->sh_addr+shdr->sh_size)<=addr) {
		return -1;
	}
	lseek(elf->fd, shdr->sh_offset + (addr - shdr->sh_addr), SEEK_SET);
	bytcnt = read(elf->fd, half, sizeof(Elf32_Half));
	if(bytcnt!=sizeof(Elf32_Half)) {
		return -1;
	}
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		*half = Elf32_Bswap16(*half);
	}
	return 0;
}

int Elf32_Shdr_Write32(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Word word)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	size_t bytcnt;
	if(!elf || !shdr) {
		return -1;
	}
	addr &= ~(0x3);
	if(addr<shdr->sh_addr || (shdr->sh_addr+shdr->sh_size)<=addr) {
		return -1;
	}
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		word = Elf32_Bswap32(word);
	}
	lseek(elf->fd, shdr->sh_offset + (addr - shdr->sh_addr), SEEK_SET);
	bytcnt = write(elf->fd, &word, sizeof(Elf32_Word));
	if(bytcnt!=sizeof(Elf32_Word)) {
		return -1;
	}
	return 0;
}

int Elf32_Shdr_Write16(Elf32_File *elf, Elf32_Shdr *shdr, Elf32_Addr addr, Elf32_Half half)
{
	int is_cpu_le = Elf32_Is_CPU_LE();
	size_t bytcnt;
	if(!elf || !shdr) {
		return -1;
	}
	addr &= ~(0x1);
	if(addr<shdr->sh_addr || (shdr->sh_addr+shdr->sh_size)<=addr) {
		return -1;
	}
	if((is_cpu_le && elf->is_be) || (!is_cpu_le && !elf->is_be)) {
		half = Elf32_Bswap16(half);
	}
	lseek(elf->fd, shdr->sh_offset + (addr - shdr->sh_addr), SEEK_SET);
	bytcnt = write(elf->fd, &half, sizeof(Elf32_Half));
	if(bytcnt!=sizeof(Elf32_Half)) {
		return -1;
	}
	return 0;
}

