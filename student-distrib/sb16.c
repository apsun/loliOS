#include "sb16.h"
#include "lib.h"
#include "debug.h"
#include "paging.h"
#include "irq.h"
#include "dma.h"

#define SOUND_FLUSH 0
#define SB16_IOBASE 0x220
#define SB16_DMA8_CHANNEL 1
#define SB16_DMA16_CHANNEL 5
#define SB16_PORT_RESET (SB16_IOBASE + 0x6)
#define SB16_PORT_CAN_WRITE (SB16_IOBASE + 0xC)
#define SB16_PORT_CAN_READ (SB16_IOBASE + 0xE)
#define SB16_PORT_INTACK_16BIT (SB16_IOBASE + 0xF)
#define SB16_PORT_INTACK_8BIT (SB16_IOBASE + 0xE)
#define SB16_PORT_WRITE_DATA (SB16_IOBASE + 0xC)
#define SB16_PORT_READ_DATA (SB16_IOBASE + 0xA)
#define SB16_CMD_SAMPLE_RATE 0x41
#define SB16_CMD_BEGIN_CMD_16BIT 0xB0
#define SB16_CMD_BEGIN_CMD_8BIT 0xC0
#define SB16_CMD_BEGIN_MODE_STEREO (1 << 5)
#define SB16_CMD_BEGIN_MODE_SIGNED (1 << 4)
#define SB16_BUFFER_SIZE 0x10000
#define SB16_HALF_BUFFER_SIZE (SB16_BUFFER_SIZE / 2)

/* Tracks the single open sound file */
static file_obj_t *open_device = NULL;

static uint8_t *audio_buf = (uint8_t *)DMA_PAGE_START;
static int32_t audio_buf_count = 0;

/* Playback parameters (default = 11kHz, mono, 8bit) */
static int32_t sample_rate = 11025;
static int32_t num_channels = 1;
static int32_t bits_per_sample = 8;

/* Whether there is currently audio being played */
static bool is_playing = false;

/* Writes a single byte to the SB16 DSP */
static void
sb16_out(uint8_t value)
{
    while (inb(SB16_PORT_CAN_WRITE) & 0x80);
    outb(value, SB16_PORT_WRITE_DATA);
}

/* Reads a single byte from the SB16 DSP */
static uint8_t
sb16_in(void)
{
    while (!(inb(SB16_PORT_CAN_READ) & 0x80));
    return inb(SB16_PORT_READ_DATA);
}

/* Resets the SB16 DSP state */
static void
sb16_reset(void)
{
    outb(1, SB16_PORT_RESET);
    outb(0, SB16_PORT_RESET);
    while (sb16_in() != 0xAA);
}

static void
sb16_write_sample_rate(void)
{
    /* Set sample rate */
    sb16_out(SB16_CMD_SAMPLE_RATE);
    sb16_out((sample_rate >> 8) & 0xff);
    sb16_out((sample_rate >> 0) & 0xff);
}

static void
sb16_start_playback(void)
{
    uint8_t cmd = 0;
    uint8_t mode = 0;
    uint16_t len = (uint16_t)audio_buf_count;
    uint8_t channel = 0;

    if (bits_per_sample == 8) {
        /* 8 bit unsigned output */
        channel = SB16_DMA8_CHANNEL;
        cmd |= SB16_CMD_BEGIN_CMD_8BIT;
        mode &= ~SB16_CMD_BEGIN_MODE_SIGNED;
    } else if (bits_per_sample == 16) {
        /* 16 bit signed output */
        channel = SB16_DMA16_CHANNEL;
        cmd |= SB16_CMD_BEGIN_CMD_16BIT;
        mode |= SB16_CMD_BEGIN_MODE_SIGNED;

        /* Length is in samples, not bytes */
        len /= 2;
    }

    /* Start DMA transfer */
    dma_start(audio_buf, audio_buf_count, channel, false);

    if (num_channels == 1) {
        /* Mono */
        mode &= ~SB16_CMD_BEGIN_MODE_STEREO;
    } else if (num_channels == 2) {
        /* Stereo */
        mode |= SB16_CMD_BEGIN_MODE_STEREO;

        /*
         * As per code in QEMU source hw/audio/sb16.c,
         * if we are not using auto-init mode, the length
         * should NOT account for stereo.
         */
    }

    /* SB16 takes the length minus 1 */
    len--;

    /* Packet byte order is cmd, mode, LO(len), HI(len) */

    printf("sb16(cmd=0x%x, mode=0x%x, len=0x%x)\n", cmd, mode, len);
    sb16_out(cmd);
    sb16_out(mode);
    sb16_out((len >> 0) & 0xff);
    sb16_out((len >> 8) & 0xff);

    is_playing = true;
}

static void
sb16_swap_buffers(void)
{
    audio_buf = (uint8_t *)((uint32_t)audio_buf ^ SB16_HALF_BUFFER_SIZE);
    audio_buf_count = 0;
}

int32_t
sb16_open(const char *filename, file_obj_t *file)
{
    /* Only allow one open sound file at a time, since no mixer support */
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
    /* Limit writable bytes to one region */
    if (nbytes > SB16_HALF_BUFFER_SIZE - audio_buf_count) {
        nbytes = SB16_HALF_BUFFER_SIZE - audio_buf_count;
    }

    /* Copy sample data into the audio buffer */
    if (!copy_from_user(&audio_buf[audio_buf_count], buf, nbytes)) {
        return -1;
    }
    audio_buf_count += nbytes;

    /* Start playback immediately if not already playing */
    if (!is_playing && audio_buf_count > 0) {
        sb16_start_playback();
        sb16_swap_buffers();
    }

    return 0;
}

int32_t
sb16_close(file_obj_t *file)
{
    ASSERT(file == open_device);
    open_device = NULL;
    return 0;
}

static int32_t
sb16_ioctl_set_bits_per_sample(uint32_t arg)
{
    if (arg == 8 || arg == 16) {
        bits_per_sample = arg;
        return 0;
    }

    debugf("Only 8-bit and 16-bit output supported\n");
    return -1;
}

static int32_t
sb16_ioctl_set_num_channels(uint32_t arg)
{
    if (arg == 1 || arg == 2) {
        num_channels = arg;
        return 0;
    }

    debugf("Only mono or stereo channels supported\n");
    return -1;
}

static int32_t
sb16_ioctl_set_sample_rate(uint32_t arg)
{
    switch (arg) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 32000:
    case 44100:
        sample_rate = arg;
        sb16_write_sample_rate();
        return 0;
    default:
        debugf("Sample rate not supported: %u\n", arg);
        return -1;
    }
}

int32_t
sb16_ioctl(file_obj_t *file, uint32_t req, uint32_t arg)
{
    if (is_playing) {
        debugf("Cannot change parameters during playback\n");
        return -1;
    }

    switch (req) {
    case SOUND_SET_BITS_PER_SAMPLE:
        return sb16_ioctl_set_bits_per_sample(arg);
    case SOUND_SET_NUM_CHANNELS:
        return sb16_ioctl_set_num_channels(arg);
    case SOUND_SET_SAMPLE_RATE:
        return sb16_ioctl_set_sample_rate(arg);
    default:
        return -1;
    }
}

static void
sb16_handle_irq(void)
{
    printf("sb16 irq\n");

    /* Acknowledge the interuupt */
    if (bits_per_sample == 8) {
        inb(SB16_PORT_INTACK_8BIT);
    } else if (bits_per_sample == 16) {
        inb(SB16_PORT_INTACK_16BIT);
    }

    /* If more samples arrived during playback, restart */
    if (audio_buf_count > 0) {
        sb16_start_playback();
        sb16_swap_buffers();
    } else {
        is_playing = false;
    }
}

/* Initializes the Sound Blaster 16 device */
void
sb16_init(void)
{
    sb16_reset();
    irq_register_handler(IRQ_SB16, sb16_handle_irq);
}
