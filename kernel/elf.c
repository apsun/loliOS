#include "elf.h"
#include "types.h"
#include "debug.h"
#include "string.h"
#include "paging.h"
#include "filesys.h"

#define ELF_MAGIC 0x464c457f
#define ELF_CLASS_32 1
#define ELF_DATA_2LSB 1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_386 3
#define ELF_VERSION_CURRENT 1
#define ELF_PROGRAM_TYPE_LOAD 1

/*
 * Offset into the user page at which we load executables when
 * using compatibility mode.
 */
#define ELF_COMPAT_OFFSET 0x48000

/*
 * Performs some basic sanity checks on the file. Note that this
 * does not guarantee that the file can be successfully loaded.
 * Returns true if the file looks like a valid ELF executable.
 */
bool
elf_is_valid(int inode_idx)
{
    elf_hdr_t hdr;
    if (fs_read_data(inode_idx, 0, &hdr, sizeof(elf_hdr_t), memcpy) != sizeof(elf_hdr_t)) {
        debugf("Could not read ELF header\n");
        return false;
    }

    if (hdr.magic != ELF_MAGIC) {
        debugf("Not an ELF file (magic = %08x)\n", hdr.magic);
        return false;
    }

    if (hdr.class != ELF_CLASS_32) {
        debugf("Not a 32-bit ELF file (class = %d)\n", hdr.class);
        return false;
    }

    if (hdr.data != ELF_DATA_2LSB) {
        debugf("Not a little-endian ELF file (data = %d)\n", hdr.data);
        return false;
    }

    if (hdr.ident_version != ELF_VERSION_CURRENT) {
        debugf("Invalid ELF version (%d)\n", hdr.ident_version);
        return false;
    }

    if (hdr.type != ELF_TYPE_EXEC) {
        debugf("Not an executable file (type = %d)\n", hdr.type);
        return false;
    }

    if (hdr.machine != ELF_MACHINE_386) {
        debugf("Not an i386 executable (machine = %d)\n", hdr.machine);
        return false;
    }

    if (hdr.version != ELF_VERSION_CURRENT) {
        debugf("Invalid ELF version (%d)\n", hdr.version);
        return false;
    }

    if (hdr.ehsize != sizeof(elf_hdr_t)) {
        debugf("ELF header size mismatch (%d != %d)\n", hdr.ehsize, sizeof(elf_hdr_t));
        return false;
    }

    if (hdr.phentsize != sizeof(elf_phdr_t)) {
        debugf("ELF program header size mismatch (%d != %d)\n", hdr.phentsize, sizeof(elf_phdr_t));
        return false;
    }

    return true;
}

/*
 * Loads an ELF file using the old memcpy method. Does not support
 * expanding .bss.
 */
static uintptr_t
elf_load_impl_compat(elf_hdr_t *hdr, int inode_idx, uintptr_t paddr)
{
    /*
     * Implementation note: For whatever reason, the PT_LOAD segment
     * that contains the .data section was linked incorrectly in the
     * original userspace programs. Although the virtual address is
     * specified correctly, the file offset field in the program header
     * is off by 0x1000, which leads to globals being initialized with
     * garbage (usually zeros). The only program that is impacted by
     * this is `counter` since it calls itoa(), which uses a global
     * lookup table string.
     *
     * This avoids the issue by ignoring ELF segments altogether and
     * falling back to the dumb "just memcpy everything" loader. This
     * only works with binaries that have been run through `elfconvert`
     * (i.e. .bss must be pre-expanded on disk).
     */
    char *vaddr = (char *)TEMP_PAGE_START + ELF_COMPAT_OFFSET;
    if (fs_read_data(inode_idx, 0, vaddr, MB(4), memcpy) < 0) {
        debugf("Failed to read program\n");
        return 0;
    }
    return hdr->entry;
}

/*
 * Loads an ELF file, properly handling .bss.
 */
static uintptr_t
elf_load_impl(elf_hdr_t *hdr, int inode_idx, uintptr_t paddr)
{
    /* Load the program segments into memory */
    int i;
    for (i = 0; i < (int)hdr->phnum; ++i) {
        elf_phdr_t phdr;
        if (fs_read_data(inode_idx, hdr->phoff + i * sizeof(phdr), &phdr, sizeof(phdr), memcpy) != sizeof(phdr)) {
            debugf("Could not read ELF program header\n");
            return 0;
        }

        /* Ignore anything that doesn't need to be loaded into memory */
        if (phdr.type != ELF_PROGRAM_TYPE_LOAD) {
            continue;
        }

        /* Limit ourselves to the 128-132MB page for now */
        if (phdr.vaddr < USER_PAGE_START || phdr.vaddr + phdr.memsz >= USER_PAGE_END) {
            debugf("Program segment exceeds memory bounds\n");
            return 0;
        }

        /* This is invalid according to the ELF spec */
        if (phdr.filesz > phdr.memsz) {
            debugf("Program segment file size is larger than memory size\n");
            return 0;
        }

        /*
         * Read segment into memory. Note that filesz may be less than memsz,
         * in which case the extra space is filled with zeros (we already
         * memset the page to zeros, so it's a op-op).
         */
        uintptr_t vaddr = TEMP_PAGE_START + (phdr.vaddr - USER_PAGE_START);
        if (fs_read_data(inode_idx, phdr.offset, (void *)vaddr, phdr.filesz, memcpy) != (int)phdr.filesz) {
            debugf("Failed to read program segment\n");
            return 0;
        }
    }

    /*
     * Return the entry point address. No need to do validation on this
     * value; the program will just fault on the first instruction if
     * it's invalid.
     */
    return hdr->entry;
}

/*
 * Loads a program into the user page, returning the virtual
 * address of the entry point. This does not clobber any page
 * mappings. 0 is returned if the program is invalid.
 *
 * You must validate the program with elf_is_valid() before
 * calling this function.
 */
uintptr_t
elf_load(int inode_idx, uintptr_t paddr, bool compat)
{
    elf_hdr_t hdr;
    if (fs_read_data(inode_idx, 0, &hdr, sizeof(elf_hdr_t), memcpy) != sizeof(elf_hdr_t)) {
        debugf("Could not read ELF header\n");
        return 0;
    }

    /*
     * Access the user page through a temporary address to avoid
     * clobbering page mappings.
     */
    paging_page_map(TEMP_PAGE_START, paddr, false);

    /* Clear the user page for security and to handle .bss */
    memset((void *)TEMP_PAGE_START, 0, MB(4));

    uintptr_t ret;
    if (!compat) {
        ret = elf_load_impl(&hdr, inode_idx, paddr);
    } else {
        ret = elf_load_impl_compat(&hdr, inode_idx, paddr);
    }

    paging_page_unmap(TEMP_PAGE_START);
    return ret;
}
