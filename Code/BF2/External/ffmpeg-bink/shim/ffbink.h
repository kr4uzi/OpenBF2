/*
 * [CHOSEN, not recovered] OpenBF2 macOS port: a small C interface over the vendored FFmpeg Bink
 * decoders, used by Code/RendDX9Abi/BinkFFmpeg.cpp (the Bink API subset BF2 calls). Container
 * parsing stays on the C++ side; this layer turns one frame's video bytes into YUV planes and one
 * audio-track packet into float samples. Port code, not FFmpeg code.
 */
#ifndef FFBINK_H
#define FFBINK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bytes of zeroed slack every packet buffer handed to the decoders must carry after its data. */
#define FFBINK_PACKET_PADDING 64

typedef struct FFBinkVideo FFBinkVideo;
typedef struct FFBinkAudio FFBinkAudio;

typedef struct FFBinkPicture {
    const uint8_t *plane[4];   /* Y, U, V, A (A only for alpha movies) */
    int            linesize[4];
    int            width, height;
    int            has_alpha;
    int            full_range; /* 1 for 'k' revisions (JPEG range), else MPEG range */
} FFBinkPicture;

/* codec_tag: the file's first four bytes as a little-endian dword ("BIKi" -> 'B'|'I'<<8|...).
 * video_flags: the header dword at offset 40 (FFmpeg's video extradata). Returns NULL on failure. */
FFBinkVideo *ffbink_video_open(uint32_t codec_tag, uint32_t video_flags, int width, int height);
/* Decodes one frame's video packet (data must carry FFBINK_PACKET_PADDING zeros past size).
 * The picture stays valid until the next decode or close. Returns 0 or a negative error. */
int  ffbink_video_decode(FFBinkVideo *v, const uint8_t *data, int size, FFBinkPicture *out);
/* Restarts the frame counter, as FFmpeg's flush does when playback seeks to the first frame. */
void ffbink_video_flush(FFBinkVideo *v);
void ffbink_video_close(FFBinkVideo *v);

/* use_dct: header track flag 0x1000; channels 1 or 2. Returns NULL on failure (an RDFT track is
 * reported and refused; see README.md). */
FFBinkAudio *ffbink_audio_open(uint32_t codec_tag, int sample_rate, int channels, int use_dct);
/* Decodes one audio-track packet (the bytes after its size dword, starting with the reported
 * decoded size). sink receives planar float samples, [-1, 1] nominal, per decoded block.
 * Returns the number of samples per channel produced, or a negative error. */
typedef void (*ffbink_audio_sink)(void *user, const float *const *planes, int channels, int samples);
int  ffbink_audio_decode(FFBinkAudio *a, const uint8_t *data, int size, ffbink_audio_sink sink, void *user);
void ffbink_audio_flush(FFBinkAudio *a);
void ffbink_audio_close(FFBinkAudio *a);

#ifdef __cplusplus
}
#endif

#endif /* FFBINK_H */
