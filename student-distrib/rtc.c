#include "rtc.h"
#include "irq.h"
#include "lib.h"
#include "debug.h"
#include "file.h"
#include "process.h"

/*
 * We need to keep track of all open RTC files somehow,
 * so we can update their interrupt occurred flags (We're
 * assuming each process only has one RTC file open at a time,
 * so this array should be big enough.)
 */
#define MAX_RTC_FILES MAX_PROCESSES
static file_obj_t *rtc_files[MAX_RTC_FILES];

/*
 * Reads the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static uint8_t
read_reg(uint8_t reg)
{
    outb(reg, RTC_PORT_INDEX);
    return inb(RTC_PORT_DATA);
}

/*
 * Writes the value of a RTC register. The reg
 * param must be one of the RTC_REG_* constants.
 */
static void
write_reg(uint8_t reg, uint8_t value)
{
    outb(reg, RTC_PORT_INDEX);
    outb(value, RTC_PORT_DATA);
}

/* RTC IRQ handler callback */
static void
handle_rtc_irq(void)
{
    /* Read from register C, ignore value */
    read_reg(RTC_REG_C);

    /* Notify any read calls that we got an interrupt */
    int32_t i;
    for (i = 0; i < MAX_RTC_FILES; ++i) {
        if (rtc_files[i] != NULL) {
            rtc_files[i]->offset = 0;
        }
    }
}

/*
 * Maps an integer frequency value to one of the
 * RTC_A_RS_* constants. Returns RTC_A_RS_NONE if the
 * frequency is not a power of 2 between 2 and 1024.
 */
static uint8_t
rtc_freq_to_rs(int32_t freq)
{
    switch (freq) {
    case 8192:
    case 4096:
    case 2048:
        return RTC_A_RS_NONE;
    case 1024:
        return RTC_A_RS_1024;
    case 512:
        return RTC_A_RS_512;
    case 256:
        return RTC_A_RS_256;
    case 128:
        return RTC_A_RS_128;
    case 64:
        return RTC_A_RS_64;
    case 32:
        return RTC_A_RS_32;
    case 16:
        return RTC_A_RS_16;
    case 8:
        return RTC_A_RS_8;
    case 4:
        return RTC_A_RS_4;
    case 2:
        return RTC_A_RS_2;
    default:
        return RTC_A_RS_NONE;
    }
}

/*
 * Sets the interrupt frequency of the RTC.
 * The input must be a power of 2 between 2
 * and 1024.
 *
 * Returns -1 on error, 0 on success.
 */
static int32_t
rtc_set_frequency(int32_t freq)
{
    /* Convert the integer to an enum value */
    uint8_t rs = rtc_freq_to_rs(freq);
    if (rs == RTC_A_RS_NONE) {
        return -1;
    }

    /* Read register A */
    uint8_t reg_a = read_reg(RTC_REG_A);

    /* Set frequency control bits */
    reg_a &= ~RTC_A_RS;
    reg_a |= rs;

    /* Write register A */
    write_reg(RTC_REG_A, reg_a);

    return 0;
}

/*
 * Open syscall for RTC. Adds the file to the internal
 * file tracker.
 */
int32_t
rtc_open(const uint8_t *filename, file_obj_t *file)
{
    int32_t i;
    for (i = 0; i < MAX_RTC_FILES; ++i) {
        if (rtc_files[i] == NULL) {
            /* RTC frequency should be set to 2 when opened */
            rtc_set_frequency(2);
            
            rtc_files[i] = file;
            return 0;
        }
    }

    /* Too many RTC files open :-( */
    return -1;
}

/*
 * Read syscall for RTC. Waits for the next periodic interrupt
 * to occur, then returns success.
 */
int32_t
rtc_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    /*
     * Wait for the interrupt handler to set this flag to 0.
     * Note that we're reusing the file offset field for this.
     * It's lazy, but it's compact and we don't have any other
     * uses for the field ;-)
     *
     * Note that this flag must be volatile, otherwise the
     * loop will be optimized to an infinite loop.
     */
    volatile uint32_t *waiting_interrupt = &file->offset;
    *waiting_interrupt = 1;

    uint32_t flags;
    sti_and_save(flags);
    while (*waiting_interrupt);
    restore_flags(flags);

    return 0;
}

/*
 * Write syscall for RTC. Sets the global periodic interrupt
 * frequency for the RTC. This change will be visible to other
 * programs.
 *
 * buf must point to a int32_t containing the desired frequency,
 * nbytes must equal sizeof(int32_t). The frequency must be a
 * power of 2 between 2 and 1024. file is ignored.
 */
int32_t
rtc_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    int32_t freq;

    /* Check if we're reading the appropriate size */
    if (nbytes != sizeof(int32_t)) {
        return -1;
    }

    /* Read the frequency */
    if (copy_from_user(&freq, buf, sizeof(int32_t)) < sizeof(int32_t)) {
        return -1;
    }

    return rtc_set_frequency(freq);
}

/*
 * Close syscall for RTC. Removes the file from the internal
 * file tracker.
 */
int32_t
rtc_close(file_obj_t *file)
{
    int32_t i;
    for (i = 0; i < MAX_RTC_FILES; ++i) {
        if (rtc_files[i] == file) {
            rtc_files[i] = NULL;
            return 0;
        }
    }

    /* WTF, the file wasn't open? */
    ASSERT(0);
    return -1;
}

/* Initializes the RTC and enables interrupts */
void
rtc_init(void)
{
    /* Read RTC register B */
    uint8_t reg_b = read_reg(RTC_REG_B);

    /* Enable periodic interrupts */
    reg_b |= RTC_B_PIE;

    /* Use binary format */
    reg_b |= RTC_B_DM;

    /* Write RTC register B */
    write_reg(RTC_REG_B, reg_b);

    /* Set the interrupt frequency to 2Hz by default */
    rtc_set_frequency(2);

    /* Register RTC IRQ handler and enable interrupts */
    irq_register_handler(IRQ_RTC, handle_rtc_irq);
}
