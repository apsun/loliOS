#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define EXEC_NAME "execfuzz.child"
#define EXEC_SIZE 8192

#define ELF_MAGIC 0x464c457f
#define ELF_CLASS_32 1
#define ELF_DATA_2LSB 1
#define ELF_TYPE_EXEC 2
#define ELF_MACHINE_386 3
#define ELF_VERSION_CURRENT 1
#define ELF_PROGRAM_TYPE_LOAD 1
#define ELF_PROGRAM_TYPE_NOTE 4
#define ELF_NOCOMPAT_NAME "loliOS"
#define ELF_NOCOMPAT_TYPE 1337

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
} __attribute__((packed)) elf_hdr_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) elf_prog_hdr_t;

typedef struct {
    uint32_t namesz;
    uint32_t descsz;
    uint32_t type;
} __attribute__((packed)) elf_note_hdr_t;

static size_t
randsize(size_t max)
{
    if (urand() & 1) {
        return urand();
    } else {
        return urand() % (max + 1);
    }
}

static size_t
randchoice(size_t a, size_t b)
{
    if (urand() & 1) {
        return a;
    } else {
        return b;
    }
}

static void
make_fake_elf(char buf[EXEC_SIZE])
{
    /*
     * Prefill some reasonable values so we can get past the
     * basic constant checks and spend time actually testing
     * edge case code paths.
     */
    elf_hdr_t *hdr = (elf_hdr_t *)buf;
    hdr->magic = ELF_MAGIC;
    hdr->class = ELF_CLASS_32;
    hdr->data = ELF_DATA_2LSB;
    hdr->ident_version = ELF_VERSION_CURRENT;
    hdr->type = ELF_TYPE_EXEC;
    hdr->machine = ELF_MACHINE_386;
    hdr->version = ELF_VERSION_CURRENT;
    hdr->entry = randsize(EXEC_SIZE);
    hdr->phoff = randsize(EXEC_SIZE);
    hdr->ehsize = sizeof(elf_hdr_t);
    hdr->phentsize = sizeof(elf_prog_hdr_t);
    hdr->phnum = randsize(2);

    uint32_t i;
    for (i = 0; i < hdr->phnum; ++i) {
        if (hdr->phoff >= EXEC_SIZE - (i + 1) * sizeof(elf_prog_hdr_t)) {
            break;
        }

        elf_prog_hdr_t *phdr = (elf_prog_hdr_t *)&buf[hdr->phoff] + i;
        phdr->type = randchoice(ELF_PROGRAM_TYPE_LOAD, ELF_PROGRAM_TYPE_NOTE);
        phdr->offset = randsize(EXEC_SIZE);
        phdr->filesz = randsize(EXEC_SIZE);
        phdr->memsz = randsize(EXEC_SIZE);
        phdr->vaddr = 0x8000000 + randsize(0x400000);

        if ((urand() & 1) &&
            phdr->type == ELF_PROGRAM_TYPE_NOTE &&
            phdr->offset < EXEC_SIZE - sizeof(elf_note_hdr_t))
        {
            elf_note_hdr_t *nhdr = (elf_note_hdr_t *)&buf[phdr->offset];
            nhdr->namesz = sizeof(ELF_NOCOMPAT_NAME);
            nhdr->descsz = 0;
            nhdr->type = ELF_NOCOMPAT_TYPE;
            if (phdr->offset < EXEC_SIZE - sizeof(elf_note_hdr_t) - nhdr->namesz) {
                memcpy(nhdr + 1, ELF_NOCOMPAT_NAME, nhdr->namesz);
            }
        }
    }
}

int
main(void)
{
    FILE *randf = fopen("random", "r");
    if (randf == NULL) {
        fprintf(stderr, "Failed to open random file\n");
        return 1;
    }

    FILE *elff = fopen(EXEC_NAME, "w");
    if (elff == NULL) {
        fprintf(stderr, "Failed to create child binary\n");
        return 1;
    }

    int iter = 0;
    while (1) {
        printf("%d\n", ++iter);

        /* Fill buffer with random data */
        char buf[EXEC_SIZE];
        fread(buf, 1, EXEC_SIZE, randf);
        make_fake_elf(buf);

        /* Flush buffer out to file */
        fseek(elff, 0, SEEK_SET);
        fwrite(buf, 1, EXEC_SIZE, elff);

        int pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Failed to fork\n");
            return 1;
        } else if (pid > 0) {
            /* We just care about exec(), kill child immediately */
            kill(pid, SIGKILL);
            wait(&pid);
        } else {
            exec(EXEC_NAME);
            exit(1);
        }
    }
}
