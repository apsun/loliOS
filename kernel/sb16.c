#include "sb16.h"
#include "lib.h"
#include "debug.h"
#include "list.h"
#include "file.h"
#include "paging.h"
#include "irq.h"
#include "dma.h"
#include "scheduler.h"
#include "signal.h"

/* DMA channels */
#define SB16_DMA8_CHANNEL 1
#define SB16_DMA16_CHANNEL 5

/* I/O port numbers */
#define SB16_IOBASE 0x220
#define SB16_PORT_RESET (SB16_IOBASE + 0x6)
#define SB16_PORT_CAN_WRITE (SB16_IOBASE + 0xC)
#define SB16_PORT_CAN_READ (SB16_IOBASE + 0xE)
#define SB16_PORT_INTACK_16BIT (SB16_IOBASE + 0xF)
#define SB16_PORT_INTACK_8BIT (SB16_IOBASE + 0xE)
#define SB16_PORT_WRITE_DATA (SB16_IOBASE + 0xC)
#define SB16_PORT_READ_DATA (SB16_IOBASE + 0xA)

/* Playback commands and flags */
#define SB16_CMD_SAMPLE_RATE 0x41
#define SB16_CMD_BEGIN_CMD_16BIT 0xB0
#define SB16_CMD_BEGIN_CMD_8BIT 0xC0
#define SB16_CMD_BEGIN_MODE_STEREO (1 << 5)
#define SB16_CMD_BEGIN_MODE_SIGNED (1 << 4)

/* Sample DMA buffer */
#define SB16_BUFFER_SIZE (SB16_PAGE_END - SB16_PAGE_START)
#define SB16_HALF_BUFFER_SIZE (SB16_BUFFER_SIZE / 2)

/* Tracks the single open sound file */
static file_obj_t *open_device = NULL;

/* Tracks the currently active sample buffer */
static uint8_t *audio_buf = (uint8_t *)SB16_PAGE_START;
static int audio_buf_count = 0;

/* Playback parameters (default = 11kHz, mono, 8bit) */
static int sample_rate = 11025;
static int num_channels = 1;
static int bits_per_sample = 8;

/* Whether there is currently audio being played */
static bool is_playing = false;

/* Sleep queue for audio playback */
static list_declare(sleep_queue);

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

/*
 * Resets the SB16 DSP state. Returns a boolean indicating
 * whether the reset was successful (i.e. the device actually
 * exists).
 */
static bool
sb16_reset(void)
{
    outb(1, SB16_PORT_RESET);
    outb(0, SB16_PORT_RESET);
    return (sb16_in() == 0xAA);
}

/* Sets the SB16 sample rate to the current global value */
static void
sb16_write_sample_rate(void)
{
    sb16_out(SB16_CMD_SAMPLE_RATE);
    sb16_out((sample_rate >> 8) & 0xff);
    sb16_out((sample_rate >> 0) & 0xff);
}

/* Begins audio playback, must not be called during playback */
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
        len /= 2;
    }

    if (num_channels == 1) {
        /* Mono */
        mode &= ~SB16_CMD_BEGIN_MODE_STEREO;
    } else if (num_channels == 2) {
        /* Stereo */
        mode |= SB16_CMD_BEGIN_MODE_STEREO;
        len /= 2;
    }

    /* SB16 takes the length minus 1 */
    len--;

    /* Packet byte order is cmd, mode, LO(len), HI(len) */
    sb16_out(cmd);
    sb16_out(mode);
    sb16_out((len >> 0) & 0xff);
    sb16_out((len >> 8) & 0xff);

    /* Start DMA transfer */
    dma_start(audio_buf, audio_buf_count, channel, DMA_OP_READ | DMA_MODE_SINGLE);

    is_playing = true;
}

/* Swaps the active audio buffer */
static void
sb16_swap_buffers(void)
{
    audio_buf = (uint8_t *)((uint32_t)audio_buf ^ SB16_HALF_BUFFER_SIZE);
    audio_buf_count = 0;
}

/* Acquires exclusive access to the Sound Blaster 16 device */
int
sb16_open(file_obj_t *file)
{
    /* Only allow one open sound file at a time, since no mixer support */
    if (open_device != NULL) {
        debugf("Device busy, cannot open\n");
        return -1;
    }

    open_device = file;
    return 0;
}

/* SB16 read() syscall handler, always fails */
int
sb16_read(file_obj_t *file, void *buf, int nbytes)
{
    return -1;
}

/*
 * SB16 write() syscall handler. If audio is not already
 * playing, this will begin playback. To set playback
 * parameters, use ioctl().
 */
int
sb16_write(file_obj_t *file, const void *buf, int nbytes)
{
    if (nbytes < 0) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    pcb_t *pcb = get_executing_pcb();
    int to_write;
    while (1) {
        to_write = nbytes;

        /* Limit writable bytes to one region */
        if (to_write > SB16_HALF_BUFFER_SIZE - audio_buf_count) {
            to_write = SB16_HALF_BUFFER_SIZE - audio_buf_count;
        }

        /* If we're using the 16-bit DMA channel, nbytes must be even */
        if (bits_per_sample != 8) {
            to_write &= ~1;
        }

        /* Do we have anything to write? */
        if (to_write > 0) {
            break;
        }

        /* If we can't write anything, the device must be busy */
        assert(is_playing);

        /* Check if file is in nonblocking mode */
        if (file->nonblocking) {
            return -EAGAIN;
        }

        /* Check for pending signals */
        if (signal_has_pending(pcb->signals)) {
            return -EINTR;
        }

        /* Wait for previous playback to complete */
        scheduler_sleep(&sleep_queue);
    }

    /* Copy sample data into the audio buffer */
    if (!copy_from_user(&audio_buf[audio_buf_count], buf, to_write)) {
        return -1;
    }
    audio_buf_count += to_write;

    /* Start playback immediately if not already playing */
    if (!is_playing) {
        sb16_start_playback();
        sb16_swap_buffers();
    }

    return to_write;
}

/* Releases exclusive access to the Sound Blaster 16 device */
int
sb16_close(file_obj_t *file)
{
    assert(file == open_device);
    open_device = NULL;
    return 0;
}

/* Sets the bits per sample playback parameter */
static int
sb16_ioctl_set_bits_per_sample(int arg)
{
    if (arg == 8 || arg == 16) {
        bits_per_sample = arg;
        return 0;
    }

    debugf("Only 8-bit and 16-bit output supported\n");
    return -1;
}

/* Sets the mono/stereo playback parameter */
static int
sb16_ioctl_set_num_channels(int arg)
{
    if (arg == 1 || arg == 2) {
        num_channels = arg;
        return 0;
    }

    debugf("Only mono or stereo channels supported\n");
    return -1;
}

/* Sets the sample rate playback parameter */
static int
sb16_ioctl_set_sample_rate(int arg)
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
        debugf("Sample rate not supported: %d\n", arg);
        return -1;
    }
}

/*
 * SB16 ioctl() syscall handler. Supports the following
 * arguments:
 *
 * SOUND_SET_BITS_PER_SAMPLE: arg = 8 (unsigned) or 16 (signed)
 * SOUND_SET_NUM_CHANNELS: arg = 1 (mono) or 2 (stereo)
 * SOUND_SET_SAMPLE_RATE: arg = 8000, 11025, etc., 44100
 */
int
sb16_ioctl(file_obj_t *file, int req, int arg)
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

/* SB16 IRQ handler */
static void
sb16_handle_irq(void)
{
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
        scheduler_wake_all(&sleep_queue);
    } else {
        is_playing = false;
    }
}

/* Sound Blaster 16 file ops */
static const file_ops_t sb16_fops = {
    .open = sb16_open,
    .read = sb16_read,
    .write = sb16_write,
    .close = sb16_close,
    .ioctl = sb16_ioctl,
};

/* Initializes the Sound Blaster 16 device */
void
sb16_init(void)
{
    if (sb16_reset()) {
        debugf("Sound Blaster 16 device installed, registering IRQ handler\n");
        irq_register_handler(IRQ_SB16, sb16_handle_irq);
        file_register_type(FILE_TYPE_SOUND, &sb16_fops);
        sb16_write_sample_rate();
    } else {
        debugf("Sound Blaster 16 device not installed\n");
    }
}
