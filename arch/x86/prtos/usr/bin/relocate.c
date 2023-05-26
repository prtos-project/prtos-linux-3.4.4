/*
 * $FILE: relocate.c
 *
 * Relocate the vmlinux ELF to a specific load address
 *
 * $LICENSE:
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

#include <asm/bootparam.h>

#define ROUNDUP(V, A)   (((V) + ((A)-1)) & (-(A)))

#define LINUX_PHYS      CONFIG_PHYSICAL_START

#define EPRINT(...) do { \
    fprintf(stderr, __VA_ARGS__); \
    exit(-1); \
} while (0)

#define READ(fd, buf, size, offset) do { \
    if (lseek(fd, offset, SEEK_SET)<0) \
        EPRINT("Error:%d: Could not seek ELF\n", __LINE__); \
    if (read(fd, buf, size)<(size)) \
        EPRINT("Error:%d: Could not read ELF\n", __LINE__); \
} while(0)

#define WRITE(fd, buf, size, offset) do { \
    if (lseek(fd, offset, SEEK_SET)<0) \
        EPRINT("Error:%d: Could not seek ELF\n", __LINE__); \
    if (write(fd, buf, size)<(size)) \
        EPRINT("Error:%d: Could not write ELF\n", __LINE__); \
} while(0)

struct Elf {
    Elf32_Ehdr *eHdr;
    Elf32_Phdr *pHdr;
    Elf32_Shdr *sHdr;
    Elf32_Sym *symTab;
    unsigned int noSyms;
    char *strTab;
    char *shStrTab;
    char **sections;
};

int RelocateElf(struct Elf *elf, unsigned int loadAddr);
unsigned int FindSymbolValue(struct Elf *elf, char *name);
void RelocateSymbol(struct Elf *elf, char *name, unsigned int value);
void RelocateInitrd(struct Elf *elf);
void FixBootparams(struct Elf *elf);
void LoadElf(int fd, struct Elf *elf);
void StoreElf(int fd, struct Elf *elf);

int RelocateElf(struct Elf *elf, unsigned int loadAddr) {

    Elf32_Rel *rel;
    Elf32_Rela *rela;
    unsigned int off, noElem, val;
    int i, j, relocOffset;

    relocOffset = loadAddr - LINUX_PHYS;
    elf->eHdr->e_entry += relocOffset;

    for (i = 0; i < elf->noSyms; i++) {
        elf->symTab[i].st_value += relocOffset;
    }

    for (i=0; i<elf->eHdr->e_shnum; i++) {
        if (strstr(&elf->shStrTab[elf->sHdr[i].sh_name], "kcrctab"))
            continue;
        switch(elf->sHdr[i].sh_type) {
            case SHT_REL:
                if ((elf->sHdr[elf->sHdr[i].sh_info].sh_flags & SHF_ALLOC)) {
                    noElem = elf->sHdr[i].sh_size / elf->sHdr[i].sh_entsize;
                    rel = (Elf32_Rel *)elf->sections[i];
                    for (j=0; j<noElem; ++j) {
                        if (ELF32_R_TYPE(rel[j].r_info) == R_386_32) {
                            off = rel[j].r_offset - elf->sHdr[elf->sHdr[i].sh_info].sh_addr;
                            memcpy(&val, &elf->sections[elf->sHdr[i].sh_info][off], sizeof(unsigned int));
                            val += relocOffset;
                            memcpy(&elf->sections[elf->sHdr[i].sh_info][off], &val, sizeof(unsigned int));
                        }
                    }
                }
                break;

            case SHT_RELA:
                if ((elf->sHdr[elf->sHdr[i].sh_info].sh_flags & SHF_ALLOC)) {
                    noElem = elf->sHdr[i].sh_size / elf->sHdr[i].sh_entsize;
                    rela = (Elf32_Rela *)elf->sections[i];
                    for (j=0; j<noElem; ++j) {
                        if (ELF32_R_TYPE(rela[j].r_info) == R_386_32) {
                            off = rela[j].r_offset - elf->sHdr[elf->sHdr[i].sh_info].sh_addr;
                            memcpy(&val, &elf->sections[elf->sHdr[i].sh_info][off], sizeof(unsigned int));
                            val += relocOffset;
                            memcpy(&elf->sections[elf->sHdr[i].sh_info][off], &val, sizeof(unsigned int));
                        }
                    }
                }
                break;
        }
    }

    for (i=0; i<elf->eHdr->e_shnum; i++) {       /* XXX: This has to be done here, as we need the original address */
        if (elf->sHdr[i].sh_flags & SHF_ALLOC) { /*      of the section to relocate symbols */
            elf->sHdr[i].sh_addr += relocOffset;
        }
    }

    for (i=0; i<elf->eHdr->e_phnum; i++) {
        if (elf->pHdr[i].p_type == PT_LOAD) {
            elf->pHdr[i].p_paddr += relocOffset;
            elf->pHdr[i].p_vaddr += relocOffset;
        }
    }

    return 0;
}

unsigned int FindSymbolValue(struct Elf *elf, char *name) {
    int i;

    for (i = 0; i < elf->noSyms; i++) {
        if ((!strcmp(&elf->strTab[elf->symTab[i].st_name], name))) {
            return elf->symTab[i].st_value;
        }
    }

    EPRINT("Could not find symbol %s\n", name);

    return 0;
}

void RelocateSymbol(struct Elf *elf, char *name, unsigned int offset) {
    Elf32_Rel *rel;
    Elf32_Rela *rela;
    unsigned int off, noElem, val;
    int i, j;

    for (i = 0; i < elf->noSyms; ++i) {
        if (!strcmp(&elf->strTab[elf->symTab[i].st_name], name)) {
            elf->symTab[i].st_value += offset;
        }
    }

    for (i = 0; i < elf->eHdr->e_shnum; i++) {
        if (strstr(&elf->shStrTab[elf->sHdr[i].sh_name], "kcrctab"))
            continue;
        switch (elf->sHdr[i].sh_type) {
        case SHT_REL:
            if ((elf->sHdr[elf->sHdr[i].sh_info].sh_flags & SHF_ALLOC)) {
                noElem = elf->sHdr[i].sh_size / elf->sHdr[i].sh_entsize;
                rel = (Elf32_Rel *)elf->sections[i];
                for (j = 0; j < noElem; ++j) {
                    if (ELF32_R_TYPE(rel[j].r_info) == R_386_32) {
                        off = ELF32_R_SYM(rel[j].r_info);
                        if (!strcmp(&elf->strTab[elf->symTab[off].st_name], name)) {
                            off = rel[j].r_offset - elf->sHdr[elf->sHdr[i].sh_info].sh_addr;
                            memcpy(&val, &elf->sections[elf->sHdr[i].sh_info][off], sizeof(unsigned int));
                            val += offset;
                            memcpy(&elf->sections[elf->sHdr[i].sh_info][off], &val, sizeof(unsigned int));
                        }
                    }
                }
            }
            break;

        case SHT_RELA:
            if ((elf->sHdr[elf->sHdr[i].sh_info].sh_flags & SHF_ALLOC)) {
                noElem = elf->sHdr[i].sh_size / elf->sHdr[i].sh_entsize;
                rela = (Elf32_Rela *)elf->sections[i];
                for (j = 0; j < noElem; ++j) {
                    if (ELF32_R_TYPE(rela[j].r_info) == R_386_32) {
                        off = ELF32_R_SYM(rela[j].r_info);
                        if (!strcmp(&elf->strTab[elf->symTab[off].st_name], name)) {
                            off = rela[j].r_offset - elf->sHdr[elf->sHdr[i].sh_info].sh_addr;
                            memcpy(&val, &elf->sections[elf->sHdr[i].sh_info][off], sizeof(unsigned int));
                            val += offset;
                            memcpy(&elf->sections[elf->sHdr[i].sh_info][off], &val, sizeof(unsigned int));
                        }
                    }
                }
            }
            break;
        }
    }
}

void RelocateInitrd(struct Elf *elf) {
    unsigned int initrd, initrdPhys;
    int i, idx;

    for (i=0; i<elf->eHdr->e_shnum; i++) {
        if (!strcmp(&elf->shStrTab[elf->sHdr[i].sh_name], ".initramfs")) {
            initrd = FindSymbolValue(elf, "initrd");
            initrdPhys = FindSymbolValue(elf, "initrd_phys");

            elf->sHdr[i].sh_addr = initrd;

            idx = elf->eHdr->e_phnum;
            elf->eHdr->e_phnum++;
            elf->pHdr = realloc(elf->pHdr, elf->eHdr->e_phnum*sizeof(Elf32_Phdr));
            elf->pHdr[idx].p_align = elf->sHdr[i].sh_addralign;
            elf->pHdr[idx].p_filesz = elf->sHdr[i].sh_size;
            elf->pHdr[idx].p_flags = PF_R|PF_W;
            elf->pHdr[idx].p_memsz = elf->sHdr[i].sh_size;
            elf->pHdr[idx].p_offset = elf->sHdr[i].sh_offset;
            elf->pHdr[idx].p_vaddr = initrd;
            elf->pHdr[idx].p_paddr = initrdPhys;
            elf->pHdr[idx].p_type = PT_LOAD;

            RelocateSymbol(elf, "_end", ROUNDUP(elf->sHdr[i].sh_size, 4096));
            return;
        }
    }
    EPRINT("Could not find .initramfs section\n");
}

void FixBootparams(struct Elf *elf) {
    int i;
    unsigned int initrdPhys=0, initrdSize=0;
    struct boot_params *boot_params;

    for (i=0; i<elf->eHdr->e_shnum; i++) {
        if (!strcmp(&elf->shStrTab[elf->sHdr[i].sh_name], ".initramfs")) {
            initrdSize = elf->sHdr[i].sh_size;
            break;
        }
    }

    for (i=0; i<elf->eHdr->e_shnum; i++) {
        if (!strcmp(&elf->shStrTab[elf->sHdr[i].sh_name], ".prtos_boot_params")) {
            initrdPhys = FindSymbolValue(elf, "initrd_phys");
            boot_params = (void *)elf->sections[i];
            boot_params->hdr.ramdisk_image = initrdPhys;
            boot_params->hdr.ramdisk_size = initrdSize;//ROUNDUP(initrdSize, 4096);
            break;
        }
    }
}

void StoreElf(int fd, struct Elf *elf) {
    int i;

    for (i = 0; i < elf->eHdr->e_shnum; i++) {
        WRITE(fd, elf->sections[i], elf->sHdr[i].sh_size, elf->sHdr[i].sh_offset);
    }
    WRITE(fd, elf->eHdr, sizeof(Elf32_Ehdr), 0);
    WRITE(fd, elf->pHdr, sizeof(Elf32_Phdr)*elf->eHdr->e_phnum, elf->eHdr->e_phoff);
    WRITE(fd, elf->sHdr, sizeof(Elf32_Shdr)*elf->eHdr->e_shnum, elf->eHdr->e_shoff);
}

void LoadElf(int fd, struct Elf *elf) {
    int i;

    elf->eHdr = malloc(sizeof(Elf32_Ehdr));
    READ(fd, elf->eHdr, sizeof(Elf32_Ehdr), 0);

    if ((elf->eHdr->e_type != ET_EXEC) || (elf->eHdr->e_machine != EM_386) || (elf->eHdr->e_phentsize != sizeof(Elf32_Phdr)))
        EPRINT("Invalid ELF header\n");

    elf->sHdr = malloc(sizeof(Elf32_Shdr)*elf->eHdr->e_shnum);
    READ(fd, elf->sHdr, sizeof(Elf32_Shdr)*elf->eHdr->e_shnum, elf->eHdr->e_shoff);

    elf->pHdr = malloc(sizeof(Elf32_Shdr)*elf->eHdr->e_phnum);
    READ(fd, elf->pHdr, sizeof(Elf32_Phdr)*elf->eHdr->e_phnum, elf->eHdr->e_phoff);

    elf->sections = malloc(elf->eHdr->e_shnum*sizeof(char *));
    for (i = 0; i < elf->eHdr->e_shnum; i++) {
        elf->sections[i] = malloc(elf->sHdr[i].sh_size);
        READ(fd, elf->sections[i], elf->sHdr[i].sh_size, elf->sHdr[i].sh_offset);
        if (elf->sHdr[i].sh_type == SHT_STRTAB) {
            elf->strTab = elf->sections[i];
        }
        if (elf->sHdr[i].sh_type == SHT_SYMTAB) {
            elf->symTab = (Elf32_Sym *)elf->sections[i];
            elf->noSyms = elf->sHdr[i].sh_size / elf->sHdr[i].sh_entsize;
        }
        if (i == elf->eHdr->e_shstrndx) {
            elf->shStrTab = elf->sections[i];
        }
    }
}

#define OPTIONS "l:ih"
#define HELP    "relocate [OPTIONS] <vmlinux>\n\n" \
                "Options:\n" \
                "    -l <reloc_addr>    Relink kernel to <reloc_addr>\n" \
                "    -i                 Relocate initramfs\n" \
                "    -h                 Print this help\n"

int main(int argc, char *argv[]) {
    int c, fd;
    unsigned int loadAddr, relocInitrd;
    struct Elf elf;

    if (argc < 3) {
        EPRINT(HELP);
    }

    loadAddr = 0;
    relocInitrd = 0;
    while ((c=getopt(argc, argv, OPTIONS)) != -1) {
        switch (c) {
            case 'h':
                EPRINT(HELP);
                break;
            case 'l':
                loadAddr = strtoul(optarg, 0, 0);
                break;
            case 'i':
                relocInitrd = 1;
                break;
            default:
                printf("unknwon option -%c\n", c);
                EPRINT(HELP);
                break;
        }
    }

    if (optind >= argc) {
        EPRINT("linux kernel file not specified\n");
    }

    fd = open(argv[optind], O_RDWR);
    if (fd < 0) {
        EPRINT("Error: could not open file %s\n", argv[optind]);
    }

    memset(&elf, 0, sizeof(elf));
    LoadElf(fd, &elf);

    if (relocInitrd) {
        RelocateInitrd(&elf);
    }
    if (loadAddr) {
        printf("Relocating %s to 0x%x\n", argv[optind], loadAddr);
        RelocateElf(&elf, loadAddr);
    }
    if (relocInitrd) {
        FixBootparams(&elf);
    }

    StoreElf(fd, &elf);

    close(fd);

    return 0;
}
