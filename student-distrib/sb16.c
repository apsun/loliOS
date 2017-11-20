#include "sb16.h"
#include "lib.h"
#include "debug.h"
#include "irq.h"

/* Tracks the single open sound file */
static file_obj_t *open_device = NULL;

int32_t
sb16_open(const char *filename, file_obj_t *file)
{
    /* Only allow one open sound file at once */
    if (open_device != NULL) {
        debugf("Device busy, cannot open\n");
        return -1;
    }

    open_device = file;
    return 0;
}

int32_t
sb16_read(file_obj_t *file, void *buf, int32_t nbytes)
{
    return 0;
}

int32_t
sb16_write(file_obj_t *file, const void *buf, int32_t nbytes)
{
    return -1;
}

int32_t
sb16_close(file_obj_t *file)
{
    ASSERT(file == open_device);
    open_device = NULL;
    return 0;
}

int32_t
sb16_ioctl(file_obj_t *file, uint32_t req, uint32_t arg)
{
    return -1;
}

static void
sb16_handle_irq(void)
{

}

/* Initializes the Sound Blaster 16 device */
void
sb16_init(void)
{
    irq_register_handler(IRQ_SB16, sb16_handle_irq);
}
