#ifndef _ELF_H
#define _ELF_H

#include "types.h"

#ifndef ASM

typedef struct {
    uint32_t magic;
    uint8_t class;
    uint8_t data;
    uint8_t ident_version;
    uint8_t padding[9];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_hdr_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_phdr_t;

/* Performs basic sanity checks */
bool elf_is_valid(int inode_idx);

/* Loads a program into the user page */
uintptr_t elf_load(int inode_idx, uintptr_t paddr);

#endif /* ASM */

#endif /* _ELF_H */
