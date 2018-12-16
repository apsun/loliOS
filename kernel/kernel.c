#include "multiboot.h"
#include "lib.h"
#include "debug.h"
#include "x86_desc.h"
#include "i8259.h"
#include "idt.h"
#include "paging.h"
#include "process.h"
#include "scheduler.h"
#include "pit.h"
#include "ps2.h"
#include "rtc.h"
#include "terminal.h"
#include "filesys.h"
#include "taux.h"
#include "sb16.h"
#include "loopback.h"
#include "ne2k.h"
#include "null.h"
#include "zero.h"
#include "random.h"

/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags, bit) ((flags) & (1 << (bit)))

void
entry(unsigned long magic, unsigned long addr)
{
    /* Initialize terminals */
    terminal_init();

    /* Clear the screen */
    terminal_clear();

    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%08x\n", magic);
        return;
    }

    /* Set MBI to the address of the Multiboot information structure. */
    multiboot_info_t *mbi = (multiboot_info_t *)addr;

    /* Print out the flags. */
    printf("flags = 0x%08x\n", mbi->flags);

    /* Are mem_* valid? */
    if (CHECK_FLAG(mbi->flags, 0))
        printf("mem_lower = %uKB, mem_upper = %uKB\n", mbi->mem_lower, mbi->mem_upper);

    /* Is boot_device valid? */
    if (CHECK_FLAG(mbi->flags, 1))
        printf("boot_device = 0x%08x\n", mbi->boot_device);

    /* Is the command line passed? */
    if (CHECK_FLAG(mbi->flags, 2))
        printf("cmdline = %s\n", (char *)mbi->cmdline);

    /* Save starting address of the filesystem module */
    uint32_t fs_start = 0;

    /* Check loaded modules. */
    if (CHECK_FLAG(mbi->flags, 3)) {
        uint32_t mod_count = 0;
        module_t *mod = (module_t *)mbi->mods_addr;

        /*
         * For now we can assume that we only have a single
         * filesystem module loaded. Ensure that the entire
         * filesystem lies within the kernel page.
         */
        assert(mbi->mods_count == 1);
        if (mod->mod_end > KERNEL_PAGE_END) {
            panic("Total filesystem size is too large!");
        }
        fs_start = mod->mod_start;

        while (mod_count < mbi->mods_count) {
            printf("Module %d loaded at address: 0x%08x\n", mod_count, mod->mod_start);
            printf("Module %d ends at address: 0x%08x\n", mod_count, mod->mod_end);
            printf("First few bytes of module:\n");
            int i;
            for (i = 0; i < 16; i++) {
                printf("0x%02x ", *((uint8_t *)(mod->mod_start + i)));
            }
            printf("\n");
            mod_count++;
            mod++;
        }
    }

    /* Bits 4 and 5 are mutually exclusive! */
    if (CHECK_FLAG(mbi->flags, 4) && CHECK_FLAG(mbi->flags, 5)) {
        printf("Both bits 4 and 5 are set.\n");
        return;
    }

    /* Is the section header table of ELF valid? */
    if (CHECK_FLAG(mbi->flags, 5)) {
        elf_section_header_table_t *elf_sec = &mbi->elf_sec;
        printf(
            "elf_sec: num = %u, size = %u, addr = 0x%08x, shndx = 0x%08x\n",
            elf_sec->num, elf_sec->size, elf_sec->addr, elf_sec->shndx);
    }

    /* Are mmap_* valid? */
    if (CHECK_FLAG(mbi->flags, 6)) {
        printf("mmap_addr = 0x%08x, mmap_length = %u\n", mbi->mmap_addr, mbi->mmap_length);
        memory_map_t *mmap = (memory_map_t *)mbi->mmap_addr;
        while ((uint32_t)mmap < mbi->mmap_addr + mbi->mmap_length) {
            printf(
                " size = 0x%x, base_addr = 0x%08x%08x\n"
                " type = 0x%x, length = 0x%08x%08x\n",
                mmap->size,
                mmap->base_addr_high,
                mmap->base_addr_low,
                mmap->type,
                mmap->length_high,
                mmap->length_low);
            mmap = (memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }

    /* Construct an LDT entry in the GDT */
    {
        seg_desc_t the_ldt_desc;
        the_ldt_desc.granularity    = 0;
        the_ldt_desc.opsize         = 1;
        the_ldt_desc.reserved       = 0;
        the_ldt_desc.avail          = 0;
        the_ldt_desc.present        = 1;
        the_ldt_desc.dpl            = 0x0;
        the_ldt_desc.sys            = 0;
        the_ldt_desc.type           = 0x2;

        SET_LDT_PARAMS(the_ldt_desc, &ldt, ldt_size);
        ldt_desc_ptr = the_ldt_desc;
        lldt(KERNEL_LDT);
    }

    /* Construct a TSS entry in the GDT */
    {
        seg_desc_t the_tss_desc;
        the_tss_desc.granularity    = 0;
        the_tss_desc.opsize         = 0;
        the_tss_desc.reserved       = 0;
        the_tss_desc.avail          = 0;
        the_tss_desc.seg_lim_19_16  = TSS_SIZE & 0x000F0000;
        the_tss_desc.present        = 1;
        the_tss_desc.dpl            = 0x0;
        the_tss_desc.sys            = 0;
        the_tss_desc.type           = 0x9;
        the_tss_desc.seg_lim_15_00  = TSS_SIZE & 0x0000FFFF;

        SET_TSS_PARAMS(the_tss_desc, &tss, tss_size);

        tss_desc_ptr = the_tss_desc;

        tss.ldt_segment_selector = KERNEL_LDT;
        tss.ss0 = KERNEL_DS;
        tss.esp0 = 0x800000;
        ltr(KERNEL_TSS);
    }

    printf("Initializing IDT...\n");
    idt_init();

    printf("Initializing paging...\n");
    paging_init();

    printf("Initializing filesystem...\n");
    fs_init((void *)fs_start);

    printf("Initializing PIC...\n");
    i8259_init();

    printf("Initializing PIT...\n");
    pit_init();

    printf("Initializing PS/2 devices...\n");
    ps2_init();

    printf("Initializing RTC...\n");
    rtc_init();

    printf("Initializing scheduler...\n");
    scheduler_init();

    printf("Initializing processes...\n");
    process_init();

    printf("Seeding random number generator...\n");
    srand(rtc_time());

    printf("Initializing taux controller driver...\n");
    taux_init();

    printf("Initializing Sound Blaster 16 driver..\n");
    sb16_init();

    printf("Initializing loopback driver...\n");
    loopback_init();

    printf("Initializing NE2000 driver...\n");
    ne2k_init();

    printf("Initializing null file driver...\n");
    null_init();

    printf("Initializing zero file driver...\n");
    zero_init();

    printf("Initializing random file driver...\n");
    random_init();

    /* We made it! */
    printf("Boot successful!\n");

#if !(DEBUG_PRINT)
    terminal_clear();
#endif

    /* Execute the first program (`shell') ... */
    process_start_shell();

    /* Shouldn't get here... */
    panic("Should not have returned from shell");
}
