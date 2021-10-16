#include "elf.h"
#include "types.h"
#include "debug.h"
#include "math.h"
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
#define ELF_PROGRAM_TYPE_NOTE 4

/*
 * If the ELF file has a PT_NOTE segment containing a note with
 * this name and type, compatibility mode will be disabled.
 */
#define ELF_NOCOMPAT_NAME "loliOS"
#define ELF_NOCOMPAT_TYPE 1337

/*
 * Offset into the user page at which we load executables when
 * using compatibility mode.
 */
#define ELF_COMPAT_OFFSET 0x48000

/* ELF header */
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
} __packed elf_hdr_t;

/* ELF program (segment) header */
typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __packed elf_prog_hdr_t;

/* ELF note header */
typedef struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
} __packed elf_note_hdr_t;

/*
 * Checks whether the given PT_LOAD segment is valid.
 */
static bool
elf_is_valid_load(int inode_idx, elf_prog_hdr_t *phdr)
{
    /* Limit ourselves to the 128-132MB page for now */
    if (phdr->vaddr < USER_PAGE_START ||
        phdr->memsz >= USER_PAGE_END - USER_PAGE_START ||
        phdr->vaddr + phdr->memsz >= USER_PAGE_END)
    {
        debugf("Program segment exceeds memory bounds\n");
        return false;
    }

    /*
     * This is invalid according to the ELF spec.
     * This also implicitly checks that filesz fits in the user page.
     */
    if (phdr->filesz > phdr->memsz) {
        debugf("Program segment file size is larger than memory size\n");
        return false;
    }

    return true;
}

/*
 * Returns true if the given note is a "nocompat" note.
 */
static bool
elf_is_nocompat_note(int inode_idx, elf_note_hdr_t *nhdr, int offset)
{
    /* If name size or type don't match, don't bother checking */
    if (nhdr->namesz != sizeof(ELF_NOCOMPAT_NAME) || nhdr->type != ELF_NOCOMPAT_TYPE) {
        return false;
    }

    /* Read the name (note that namesz includes \0) */
    char name[sizeof(ELF_NOCOMPAT_NAME)];
    if (fs_read_data(inode_idx, offset, name, nhdr->namesz, memcpy) != (int)nhdr->namesz) {
        return false;
    }

    return memcmp(name, ELF_NOCOMPAT_NAME, sizeof(ELF_NOCOMPAT_NAME)) == 0;
}

/*
 * Checks whether the given PT_NOTE segment is valid.
 * If it contains a "nocompat" note, out_compat is set
 * to false.
 */
static bool
elf_is_valid_note(int inode_idx, elf_prog_hdr_t *phdr, bool *out_compat)
{
    uint32_t count = 0;
    while (count < phdr->filesz) {
        elf_note_hdr_t nhdr;

        uint32_t offset = phdr->offset + count;
        if (offset > INT_MAX - sizeof(nhdr)) {
            debugf("Invalid note header offset\n");
            return false;
        }

        if (fs_read_data(inode_idx, offset, &nhdr, sizeof(nhdr), memcpy) != sizeof(nhdr)) {
            debugf("Failed to read note header\n");
            return false;
        }

        if (elf_is_nocompat_note(inode_idx, &nhdr, offset + sizeof(nhdr))) {
            *out_compat = false;
        }

        /*
         * Move to the next note, rounding up name/desc size to multiple of 4.
         * This may overflow; we don't care. Worst case, we read some garbage
         * and accidentally treat the file as valid.
         */
        count += sizeof(nhdr) + round_up(nhdr.namesz, 4) + round_up(nhdr.descsz, 4);
    }

    return true;
}

/*
 * Performs some basic sanity checks on the file. Note that this
 * does not guarantee that the file can be successfully loaded.
 * Returns true if the file looks like a valid ELF executable.
 *
 * out_compat is set to true if the program needs to be loaded
 * in compatibility mode, or false otherwise.
 */
bool
elf_is_valid(int inode_idx, bool *out_compat)
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
        debugf(
            "ELF header size mismatch (%d != %d)\n",
            hdr.ehsize,
            sizeof(elf_hdr_t));
        return false;
    }

    if (hdr.phentsize != sizeof(elf_prog_hdr_t)) {
        debugf(
            "ELF program header size mismatch (%d != %d)\n",
            hdr.phentsize,
            sizeof(elf_prog_hdr_t));
        return false;
    }

    /* Assume program needs compatibility mode unless proven otherwise */
    *out_compat = true;

    uint32_t i;
    for (i = 0; i < hdr.phnum; ++i) {
        uint32_t offset = hdr.phoff + i * sizeof(elf_prog_hdr_t);
        if (offset > INT_MAX) {
            debugf("Invalid program header offset\n");
            return false;
        }

        elf_prog_hdr_t phdr;
        if (fs_read_data(inode_idx, offset, &phdr, sizeof(phdr), memcpy) != sizeof(phdr)) {
            debugf("Could not read ELF program header\n");
            return false;
        }

        if (phdr.type == ELF_PROGRAM_TYPE_LOAD) {
            if (!elf_is_valid_load(inode_idx, &phdr)) {
                return false;
            }
        } else if (phdr.type == ELF_PROGRAM_TYPE_NOTE) {
            if (!elf_is_valid_note(inode_idx, &phdr, out_compat)) {
                return false;
            }
        }
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
    void *vaddr = (void *)(TEMP_PAGE_START + ELF_COMPAT_OFFSET);
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
    uint32_t i;
    for (i = 0; i < hdr->phnum; ++i) {
        uint32_t offset = hdr->phoff + i * sizeof(elf_prog_hdr_t);
        assert(offset < INT_MAX);

        elf_prog_hdr_t phdr;
        int ret = fs_read_data(inode_idx, offset, &phdr, sizeof(phdr), memcpy);
        assert(ret == sizeof(phdr));

        /* Ignore anything that doesn't need to be loaded into memory */
        if (phdr.type != ELF_PROGRAM_TYPE_LOAD) {
            continue;
        }

        /* Ensure that offset and size look sane */
        if (phdr.offset > INT_MAX || phdr.filesz > INT_MAX) {
            debugf("Program header offset/filesz too large\n");
            return 0;
        }

        /*
         * Read segment into memory. Note that filesz may be less than memsz,
         * in which case the extra space is filled with zeros (we already
         * memset the page to zeros, so it's a op-op).
         */
        void *vaddr = (void *)(TEMP_PAGE_START + (phdr.vaddr - USER_PAGE_START));
        if (fs_read_data(inode_idx, phdr.offset, vaddr, phdr.filesz, memcpy) != (int)phdr.filesz) {
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
