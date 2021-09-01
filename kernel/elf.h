#ifndef _ELF_H
#define _ELF_H

#include "types.h"

#ifndef ASM

/* Performs basic sanity checks */
bool elf_is_valid(int inode_idx, bool *out_compat);

/* Loads a program into the user page */
uintptr_t elf_load(int inode_idx, uintptr_t paddr, bool compat);

#endif /* ASM */

#endif /* _ELF_H */
