#include <attrib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

/*
 * Socket address to download the video from. I would prefer to
 * get rid of this and read from stdin (composed with `nc`), but
 * doing so adds a non-negligible performance penalty.
 */
#define SERVER_IP IP(10, 0, 2, 2)
#define SERVER_PORT 8989

/*
 * ELVI format magic bytes.
 */
#define ELVI_MAGIC 0x49564c45

/*
 * ELVI format header definition.
 */
typedef struct {
    uint32_t magic;
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_bits_per_pixel;
    uint32_t audio_sample_rate;
    uint32_t audio_channel_count;
    uint32_t audio_bits_per_sample;
    uint32_t max_audio_size;
} __packed elvi_header_t;

/*
 * Combined audio size and sample data buffer.
 */
typedef struct {
    uint32_t audio_size;
    uint8_t audio_data[];
} __packed elvi_audio_buf_t;

/*
 * If true, user hit CTRL-C and we should be exiting gracefully.
 */
static volatile bool interrupted = false;

__cdecl static void
sigint_handler(int signum)
{
    interrupted = true;
}

static int
read_all(int fd, void *buf, int nbytes)
{
    char *bufp = buf;
    int total = 0;
    while (!interrupted && total < nbytes) {
        int ret = read(fd, &bufp[total], nbytes - total);
        if (ret == -EAGAIN || ret == -EINTR) {
            continue;
        } else if (ret < 0) {
            return ret;
        } else if (ret == 0) {
            break;
        }
        total += ret;
    }
    return total;
}

static int
play(int fd)
{
    int ret = 1;
    char error[256];
    elvi_header_t hdr;
    elvi_audio_buf_t *audio_buf = NULL;
    int soundfd = -1;
    char *fbmem = NULL;

    error[0] = '\0';
#define FAIL(...) snprintf(error, sizeof(error), __VA_ARGS__)

    if (read_all(fd, &hdr, sizeof(hdr)) < (int)sizeof(hdr)) {
        fprintf(stderr, "Could not read header\n");
        goto exit;
    }

    if (hdr.magic != ELVI_MAGIC) {
        fprintf(stderr, "ELVI magic mismatch; got 0x%08x\n", hdr.magic);
        goto exit;
    }

    if ((hdr.max_audio_size & 3) != 0 || hdr.max_audio_size > INT_MAX - sizeof(elvi_audio_buf_t)) {
        fprintf(stderr, "Invalid max audio size\n");
        goto exit;
    }

    audio_buf = malloc(sizeof(elvi_audio_buf_t) + hdr.max_audio_size);
    if (audio_buf == NULL) {
        fprintf(stderr, "Max audio size too large\n");
        goto exit;
    }

    soundfd = create("sound", OPEN_RDWR);
    if (soundfd < 0) {
        fprintf(stderr, "Could not open sound file\n");
        goto exit;
    }

    if (ioctl(soundfd, SOUND_SET_BITS_PER_SAMPLE, hdr.audio_bits_per_sample) < 0 ||
        ioctl(soundfd, SOUND_SET_NUM_CHANNELS, hdr.audio_channel_count) < 0 ||
        ioctl(soundfd, SOUND_SET_SAMPLE_RATE, hdr.audio_sample_rate) < 0)
    {
        fprintf(stderr, "Could not set audio parameters\n");
        goto exit;
    }

    if (fbmap(
        (void **)&fbmem,
        (int)hdr.video_width,
        (int)hdr.video_height,
        (int)hdr.video_bits_per_pixel) < 0)
    {
        fprintf(stderr, "Could not map framebuffer\n");
        goto exit;
    }

    /*
     * We use the sound device to synchronize our A/V streams. Our sequence
     * of operations looks like this:
     *
     * Audio(0); Wait(-1); Video(0);
     * Audio(1); Wait(0); Video(1);
     * Audio(2); Wait(1); Video(2);
     * ...
     *
     * Essentially, at any given frame, we first write the audio samples
     * for that frame while the audio for the previous frame is still
     * playing. This ensures that we have gapless playback. Then, we wait
     * for the previous frame to complete playback. As soon as the audio
     * for the current frame starts playing, we flip the video buffer to
     * show the video for the current frame. For the first frame, Wait(-1)
     * is a no-op.
     */
    int fbsize = hdr.video_width * hdr.video_height * ((hdr.video_bits_per_pixel + 1) / 8);
    int flip = 0;
    while (!interrupted) {
        /* Read pixels into video memory back buffer */
        int rret = read_all(fd, &fbmem[flip * fbsize], fbsize);
        if (rret == 0) {
            break;
        } else if (rret < 0) {
            FAIL("Could not read video data\n");
            goto exit;
        }

        /* Read audio size and samples */
        int audio_nread = sizeof(elvi_audio_buf_t) + hdr.max_audio_size;
        if (read_all(fd, audio_buf, audio_nread) < audio_nread) {
            FAIL("Could not read audio samples\n");
            goto exit;
        }

        if (audio_buf->audio_size > hdr.max_audio_size) {
            FAIL("Audio size is larger than hdr.max_audio_size\n");
            goto exit;
        }

        /* Copy audio samples to sound device */
        int audio_size = (int)audio_buf->audio_size;
        if (write(soundfd, audio_buf->audio_data, audio_size) < audio_size) {
            FAIL("Partial write of audio data\n");
            goto exit;
        }

        /* Wait for previous frame's audio to complete playback */
        if (read(soundfd, NULL, 0) < 0) {
            FAIL("Could not wait for audio sync\n");
            goto exit;
        }

        /* Flip video front and back buffers */
        flip = fbflip(fbmem);
    }

    ret = 0;

exit:
    if (fbmem != NULL) {
        fbunmap(fbmem);
    }
    if (soundfd >= 0) {
        close(soundfd);
    }
    if (audio_buf != NULL) {
        free(audio_buf);
    }
    if (!interrupted && error[0] != '\0') {
        fprintf(stderr, "%s", error);
    }
    return ret;
}

int
main(void)
{
    int ret = 1;
    int fd = -1;

    sigaction(SIGINT, sigint_handler);

    fd = socket(SOCK_TCP);
    if (fd < 0) {
        fprintf(stderr, "socket() failed\n");
        goto exit;
    }

    sock_addr_t addr;
    addr.ip = SERVER_IP;
    addr.port = SERVER_PORT;
    if (connect(fd, &addr) < 0) {
        fprintf(stderr, "connect() failed\n");
        goto exit;
    }

    ret = play(fd);

exit:
    if (fd >= 0) {
        close(fd);
    }
    return ret;
}
