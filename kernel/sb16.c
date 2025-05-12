#include "sb16.h"
#include "types.h"
#include "debug.h"
#include "math.h"
#include "portio.h"
#include "list.h"
#include "file.h"
#include "paging.h"
#include "irq.h"
#include "dma.h"
#include "wait.h"
#include "poll.h"

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

/* Size of half of the sample buffer */
#define SB16_HALF_BUFFER_SIZE 0x2000

/* Tracks the single open sound file */
static file_obj_t *sb16_open_device = NULL;

/*
 * Sample data buffer (split into halves for gapless playback).
 * Needs to be word-aligned to perform 16-bit DMA.
 */
__aligned(2)
static uint8_t sb16_buf[2][SB16_HALF_BUFFER_SIZE];

/* Which buffer is being written to */
static volatile int sb16_buf_flip = 0;

/* Number of bytes in the buffer being written to */
static volatile int sb16_buf_count = 0;

/* Whether there is currently audio being played */
static volatile bool sb16_is_playing = false;

/* Playback parameters (default = 11kHz, mono, 8bit) */
static int sb16_sample_rate = 11025;
static int sb16_num_channels = 1;
static int sb16_bits_per_sample = 8;

/* Queue for writing audio samples */
static list_define(sb16_write_queue);

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
    sb16_out((sb16_sample_rate >> 8) & 0xff);
    sb16_out((sb16_sample_rate >> 0) & 0xff);
}

/* Begins audio playback, must not be called during playback */
static void
sb16_start_playback(void)
{
    uint8_t cmd = 0;
    uint8_t mode = 0;
    uint16_t len = (uint16_t)sb16_buf_count;
    uint8_t channel = 0;

    if (sb16_bits_per_sample == 8) {
        /* 8 bit unsigned output */
        channel = SB16_DMA8_CHANNEL;
        cmd |= SB16_CMD_BEGIN_CMD_8BIT;
        mode &= ~SB16_CMD_BEGIN_MODE_SIGNED;
    } else if (sb16_bits_per_sample == 16) {
        /* 16 bit signed output */
        channel = SB16_DMA16_CHANNEL;
        cmd |= SB16_CMD_BEGIN_CMD_16BIT;
        mode |= SB16_CMD_BEGIN_MODE_SIGNED;
        len /= 2;
    }

    if (sb16_num_channels == 1) {
        /* Mono */
        mode &= ~SB16_CMD_BEGIN_MODE_STEREO;
    } else if (sb16_num_channels == 2) {
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

    /* Start DMA transfer, swap to other buffer half */
    dma_start(sb16_buf[sb16_buf_flip], sb16_buf_count, channel, DMA_OP_READ | DMA_MODE_SINGLE);
    sb16_buf_flip = !sb16_buf_flip;
    sb16_buf_count = 0;

    sb16_is_playing = true;
}

/* Acquires exclusive access to the Sound Blaster 16 device */
static int
sb16_open(file_obj_t *file)
{
    /* Only allow one open sound file at a time, since no mixer support */
    if (sb16_open_device != NULL) {
        debugf("Device busy, cannot open\n");
        return -1;
    }

    sb16_open_device = file;
    return 0;
}

/*
 * If both audio buffer slots have data, returns -EAGAIN,
 * otherwise returns 0.
 */
static int
sb16_can_read(void)
{
    return sb16_buf_count == 0 ? 0 : -EAGAIN;
}

/*
 * SB16 read() syscall handler. Blocks until there is at least
 * one empty buffer slot (i.e. when both slots are full, waits
 * until the first one completes playback), then returns 0.
 */
static int
sb16_read(file_obj_t *file, void *buf, int nbytes)
{
    return WAIT_INTERRUPTIBLE(
        sb16_can_read(),
        &sb16_write_queue,
        file->nonblocking);
}

/*
 * Returns the maximum number of bytes that may be written
 * to the SB16 DMA buffer.
 */
static int
sb16_get_writable_bytes(int nbytes)
{
    /* Must write at least one complete sample */
    if (nbytes < 0 || nbytes < sb16_bits_per_sample / 8) {
        return -1;
    } else if (nbytes == 0) {
        return 0;
    }

    /* Limit writable bytes to one region */
    nbytes = min(nbytes, SB16_HALF_BUFFER_SIZE - sb16_buf_count);

    /* If we're using the 16-bit DMA channel, nbytes must be even */
    nbytes &= -(sb16_bits_per_sample / 8);

    /* Do we have anything to write? */
    if (nbytes > 0) {
        return nbytes;
    }

    /* If we can't write anything, the device must be busy */
    assert(sb16_is_playing);

    return -EAGAIN;
}

/*
 * SB16 write() syscall handler. If audio is not already
 * playing, this will begin playback. To set playback
 * parameters, use ioctl().
 */
static int
sb16_write(file_obj_t *file, const void *buf, int nbytes)
{
    /* Wait until buffer is writable */
    nbytes = WAIT_INTERRUPTIBLE(
        sb16_get_writable_bytes(nbytes),
        &sb16_write_queue,
        file->nonblocking);
    if (nbytes <= 0) {
        return nbytes;
    }

    /* Copy sample data into the audio buffer */
    if (!copy_from_user(&sb16_buf[sb16_buf_flip][sb16_buf_count], buf, nbytes)) {
        return -1;
    }
    sb16_buf_count += nbytes;

    /* Start playback immediately if not already playing */
    if (!sb16_is_playing) {
        sb16_start_playback();
    }

    return nbytes;
}

/* Releases exclusive access to the Sound Blaster 16 device */
static void
sb16_close(file_obj_t *file)
{
    assert(file == sb16_open_device);
    sb16_open_device = NULL;
}

/* Sets the bits per sample playback parameter */
static int
sb16_ioctl_set_bits_per_sample(intptr_t arg)
{
    if (arg == 8 || arg == 16) {
        sb16_bits_per_sample = arg;
        return 0;
    }

    debugf("Only 8-bit and 16-bit output supported\n");
    return -1;
}

/* Sets the mono/stereo playback parameter */
static int
sb16_ioctl_set_num_channels(intptr_t arg)
{
    if (arg == 1 || arg == 2) {
        sb16_num_channels = arg;
        return 0;
    }

    debugf("Only mono or stereo channels supported\n");
    return -1;
}

/* Sets the sample rate playback parameter */
static int
sb16_ioctl_set_sample_rate(intptr_t arg)
{
    switch (arg) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 32000:
    case 44100:
        sb16_sample_rate = arg;
        sb16_write_sample_rate();
        return 0;
    default:
        debugf("Sample rate not supported: %d\n", (int)arg);
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
static int
sb16_ioctl(file_obj_t *file, int req, intptr_t arg)
{
    if (sb16_is_playing) {
        debugf("Cannot change parameters during playback\n");
        return -1;
    }

    if ((file->mode & OPEN_WRITE) != OPEN_WRITE) {
        debugf("File not opened in write mode\n");
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

/*
 * SB16 poll() syscall handler. Sets the read bit if there is
 * an empty audio buffer. Sets the write bit if any bytes can
 * be written. Note that if the read bit is set, the write bit
 * is guaranteed to be set, but not necessarily vice versa.
 */
static int
sb16_poll(file_obj_t *file, wait_node_t *readq, wait_node_t *writeq)
{
    int revents = 0;

    revents |= POLL_READ(
        sb16_can_read(),
        &sb16_write_queue,
        readq);

    revents |= POLL_WRITE(
        sb16_get_writable_bytes(INT_MAX),
        &sb16_write_queue,
        writeq);

    return revents;
}

/* SB16 IRQ handler */
static void
sb16_handle_irq(void)
{
    /* Acknowledge the interuupt */
    if (sb16_bits_per_sample == 8) {
        inb(SB16_PORT_INTACK_8BIT);
    } else if (sb16_bits_per_sample == 16) {
        inb(SB16_PORT_INTACK_16BIT);
    }

    /* If more samples arrived during playback, restart */
    if (sb16_buf_count > 0) {
        sb16_start_playback();
        wait_queue_wake(&sb16_write_queue);
    } else {
        sb16_is_playing = false;
    }
}

/* Sound Blaster 16 file ops */
static const file_ops_t sb16_fops = {
    .open = sb16_open,
    .read = sb16_read,
    .write = sb16_write,
    .close = sb16_close,
    .ioctl = sb16_ioctl,
    .poll = sb16_poll,
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
