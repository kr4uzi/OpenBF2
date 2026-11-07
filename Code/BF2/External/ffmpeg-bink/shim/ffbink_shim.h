/*
 * [CHOSEN, not recovered] OpenBF2 macOS / Windows x64 port: the minimal libavutil/libavcodec surface that the
 * unmodified FFmpeg 7.1.1 Bink decoders (../libavcodec/bink.c, binkaudio.c) compile against.
 * This file is OpenBF2 port code, not FFmpeg code: it declares only what those two decoders
 * reach and implements it in ffbink_shim.c. Every libavutil/libavcodec header name the decoders
 * include (avcodec.h, get_bits.h, libavutil/tx.h, ...) is a one-line forwarder to this file.
 * See ../README.md for the provenance and licence notes.
 */
#ifndef FFBINK_SHIM_H
#define FFBINK_SHIM_H

#if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES   /* M_PI in <math.h> */
#endif
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- compiler portability (OpenBF2): GCC/clang on macOS, MSVC on Windows x64 --------------- */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#define FFBINK_INLINE      __forceinline
#define FFBINK_UNUSED
#define FFBINK_PRINTF(f, a)
#define FFBINK_NORETURN    __declspec(noreturn)
#define FFBINK_ALIGN(n)    __declspec(align(n))
static __forceinline int ffbink_clz(unsigned v) { unsigned long i; _BitScanReverse(&i, v); return 31 - (int)i; }
#else
#define FFBINK_INLINE      inline __attribute__((always_inline))
#define FFBINK_UNUSED      __attribute__((unused))
#define FFBINK_PRINTF(f, a) __attribute__((format(printf, f, a)))
#define FFBINK_NORETURN
#define FFBINK_ALIGN(n)
#define ffbink_clz(v)      __builtin_clz(v)
#endif

/* ---- libavutil/attributes.h, common.h, macros.h ------------------------------------------- */
#define av_cold
#define av_always_inline FFBINK_INLINE
#define av_unused FFBINK_UNUSED
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define FFSWAP(type, a, b) do { type SWAP_tmp = b; b = a; a = SWAP_tmp; } while (0)
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define MKTAG(a, b, c, d) ((unsigned)(a) | ((unsigned)(b) << 8) | ((unsigned)(c) << 16) | ((unsigned)(d) << 24))

static inline int av_log2(unsigned v)
{
    return v ? 31 - ffbink_clz(v) : 0;
}

static inline uint32_t AV_RL32(const void *p)
{
    const uint8_t *b = (const uint8_t *)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline uint64_t ffbink_rl64(const uint8_t *b)
{
    uint64_t v;
    memcpy(&v, b, sizeof(v));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    v = __builtin_bswap64(v);
#endif
    return v;
}

/* ---- libavutil/intfloat.h --------------------------------------------------------------- */
static inline float av_int2float(uint32_t i)
{
    float f;
    memcpy(&f, &i, sizeof(f));
    return f;
}

/* ---- libavutil/error.h, log.h, avassert.h, emms.h --------------------------------------- */
#define AVERROR(e) (-(e))
#define AVERROR_INVALIDDATA (-(int)MKTAG('I', 'N', 'D', 'A'))
#define AVERROR_EOF (-(int)MKTAG('E', 'O', 'F', ' '))
#define AV_LOG_ERROR 16
#define AV_LOG_WARNING 24
void av_log(void *avcl, int level, const char *fmt, ...) FFBINK_PRINTF(3, 4);
#define av_assert0(cond) do { if (!(cond)) ffbink_assert_fail(#cond, __FILE__, __LINE__); } while (0)
#if defined(_MSC_VER) && !defined(__clang__)
FFBINK_NORETURN void ffbink_assert_fail(const char *what, const char *file, int line);
#else
void ffbink_assert_fail(const char *what, const char *file, int line) __attribute__((noreturn));
#endif
#define emms_c() ((void)0)

/* ---- libavutil/mem.h, mem_internal.h ---------------------------------------------------- */
void *av_malloc(size_t size);
void *av_mallocz(size_t size);
void *av_calloc(size_t nmemb, size_t size);
void  av_free(void *ptr);
void  av_freep(void *ptr);
#if defined(_MSC_VER) && !defined(__clang__)
#define LOCAL_ALIGNED_16(t, v, ...) __declspec(align(16)) t v __VA_ARGS__
#define LOCAL_ALIGNED_32(t, v, ...) __declspec(align(32)) t v __VA_ARGS__
#define DECLARE_ALIGNED(n, t, v) __declspec(align(n)) t v
#else
#define LOCAL_ALIGNED_16(t, v, ...) t __attribute__((aligned(16))) v __VA_ARGS__
#define LOCAL_ALIGNED_32(t, v, ...) t __attribute__((aligned(32))) v __VA_ARGS__
#define DECLARE_ALIGNED(n, t, v) t __attribute__((aligned(n))) v
#endif

/* ---- libavutil/thread.h ----------------------------------------------------------------- */
#if defined(_WIN32)
/* Windows: InitOnceExecuteOnce in ffbink_shim.c (AVOnce has INIT_ONCE's layout, one pointer). */
typedef struct AVOnce { void *ptr; } AVOnce;
#define AV_ONCE_INIT { 0 }
int ffbink_once(AVOnce *control, void (*routine)(void));
#define ff_thread_once(control, routine) ffbink_once(control, routine)
#else
#include <pthread.h>
typedef pthread_once_t AVOnce;
#define AV_ONCE_INIT PTHREAD_ONCE_INIT
#define ff_thread_once(control, routine) pthread_once(control, routine)
#endif

/* ---- libavutil/pixfmt.h, samplefmt.h, channel_layout.h, avutil.h ----------------------- */
enum AVMediaType { AVMEDIA_TYPE_VIDEO = 0, AVMEDIA_TYPE_AUDIO = 1 };
enum AVPixelFormat { AV_PIX_FMT_NONE = -1, AV_PIX_FMT_YUV420P = 0, AV_PIX_FMT_YUVA420P = 33 };
enum AVColorRange { AVCOL_RANGE_UNSPECIFIED = 0, AVCOL_RANGE_MPEG = 1, AVCOL_RANGE_JPEG = 2 };
enum AVSampleFormat { AV_SAMPLE_FMT_NONE = -1, AV_SAMPLE_FMT_FLT = 3, AV_SAMPLE_FMT_FLTP = 8 };
#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
typedef struct AVChannelLayout { int nb_channels; } AVChannelLayout;
static inline void av_channel_layout_uninit(AVChannelLayout *l) { l->nb_channels = 0; }
static inline void av_channel_layout_default(AVChannelLayout *l, int n) { l->nb_channels = n; }

/* ---- libavutil/imgutils.h --------------------------------------------------------------- */
int av_image_check_size(unsigned int w, unsigned int h, int log_offset, void *log_ctx);

/* ---- libavcodec/codec_id.h, packet.h, frame.h ------------------------------------------- */
enum AVCodecID {
    AV_CODEC_ID_NONE = 0,
    AV_CODEC_ID_BINKVIDEO,
    AV_CODEC_ID_BINKAUDIO_RDFT,
    AV_CODEC_ID_BINKAUDIO_DCT,
};
#define AV_INPUT_BUFFER_PADDING_SIZE 64

typedef struct AVPacket {
    const uint8_t *data;   /* borrowed; the caller pads it with AV_INPUT_BUFFER_PADDING_SIZE zeros */
    int            size;
} AVPacket;
void av_packet_unref(AVPacket *pkt);

typedef struct FFBinkBuffer FFBinkBuffer;
typedef struct AVFrame {
    uint8_t  *data[8];
    int       linesize[8];
    uint8_t **extended_data;
    int       width, height;
    int       nb_samples;
    int       format;
    int64_t   pts;
    FFBinkBuffer *buf;     /* one refcounted allocation backs every plane */
} AVFrame;
AVFrame *av_frame_alloc(void);
void av_frame_free(AVFrame **frame);
void av_frame_unref(AVFrame *frame);
int  av_frame_ref(AVFrame *dst, const AVFrame *src);
int  av_frame_replace(AVFrame *dst, const AVFrame *src);

/* ---- libavcodec/avcodec.h, codec.h, codec_internal.h, internal.h, decode.h -------------- */
#define AV_CODEC_CAP_DR1 (1 << 1)
#define FF_CODEC_CAP_INIT_CLEANUP (1 << 1)
#define AV_GET_BUFFER_FLAG_REF (1 << 0)

typedef struct AVCodec {
    const char      *name;
    const char      *long_name;
    enum AVMediaType type;
    enum AVCodecID   id;
    int              capabilities;
} AVCodec;

typedef struct AVCodecInternal {
    AVPacket *in_pkt;      /* the packet binkaudio keeps between receive_frame calls */
    AVPacket  pending;     /* the packet the next ff_decode_get_packet hands out */
    FFBinkBuffer *spare;   /* one recycled video buffer */
} AVCodecInternal;

typedef struct AVCodecContext {
    const AVCodec   *codec;
    void            *priv_data;
    AVCodecInternal *internal;
    unsigned int     codec_tag;
    const uint8_t   *extradata;
    int              extradata_size;
    int              width, height;
    int              flags;
    enum AVPixelFormat pix_fmt;
    enum AVColorRange  color_range;
    int              sample_rate;
    AVChannelLayout  ch_layout;
    enum AVSampleFormat sample_fmt;
} AVCodecContext;

struct AVCodecContext;
typedef struct FFCodec {
    AVCodec p;
    int     priv_data_size;
    int   (*init)(struct AVCodecContext *);
    int   (*close)(struct AVCodecContext *);
    void  (*flush)(struct AVCodecContext *);
    int     caps_internal;
    struct {
        int (*decode)(struct AVCodecContext *, AVFrame *, int *, AVPacket *);
        int (*receive_frame)(struct AVCodecContext *, AVFrame *);
    } cb;
} FFCodec;
#define CODEC_LONG_NAME(str) .p.long_name = (str)
#define FF_CODEC_DECODE_CB(func) .cb.decode = (func)
#define FF_CODEC_RECEIVE_FRAME_CB(func) .cb.receive_frame = (func)

int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags);
int ff_reget_buffer(AVCodecContext *avctx, AVFrame *frame, int flags);
int ff_decode_get_packet(AVCodecContext *avctx, AVPacket *pkt);

/* ---- libavcodec/blockdsp.h, hpeldsp.h --------------------------------------------------- */
typedef struct BlockDSPContext {
    void (*clear_block)(int16_t *block);
    void (*fill_block_tab[2])(uint8_t *block, uint8_t value, ptrdiff_t line_size, int h);
} BlockDSPContext;
void ff_blockdsp_init(BlockDSPContext *c);

typedef void (*op_pixels_func)(uint8_t *block, const uint8_t *pixels, ptrdiff_t line_size, int h);
typedef struct HpelDSPContext {
    op_pixels_func put_pixels_tab[4][4];
} HpelDSPContext;
void ff_hpeldsp_init(HpelDSPContext *c, int flags);

/* ---- libavcodec/vlc.h ------------------------------------------------------------------- */
typedef int16_t VLCBaseType;
typedef struct VLCElem { VLCBaseType sym, len; } VLCElem;
typedef struct VLC {
    int      bits;
    VLCElem *table;
    int      table_size, table_allocated;
} VLC;
#define VLC_INIT_USE_STATIC 1
#define VLC_INIT_INPUT_LE   4
#define VLC_INIT_OUTPUT_LE  8
#define VLC_INIT_LE         (VLC_INIT_INPUT_LE | VLC_INIT_OUTPUT_LE)
/* Only the single-level, byte-wide lens/codes, LE, static form bink.c uses. */
int ffbink_vlc_init(VLC *vlc, int nb_bits, int nb_codes, const uint8_t *lens, int lens_wrap,
                    int lens_size, const uint8_t *codes, int codes_wrap, int codes_size, int flags);
#define vlc_init ffbink_vlc_init

/* ---- libavcodec/get_bits.h (little-endian reader only) ---------------------------------- */
typedef struct GetBitContext {
    const uint8_t *buffer;
    int index;
    int size_in_bits;
    int size_in_bits_plus8;
} GetBitContext;

static inline int init_get_bits(GetBitContext *s, const uint8_t *buffer, int bit_size)
{
    int ret = 0;
    if (bit_size >= INT_MAX - 64 * 8 || bit_size < 0 || !buffer) {
        bit_size = 0;
        buffer   = NULL;
        ret      = AVERROR_INVALIDDATA;
    }
    s->buffer             = buffer;
    s->size_in_bits       = bit_size;
    s->size_in_bits_plus8 = bit_size + 8;
    s->index              = 0;
    return ret;
}

static inline int init_get_bits8(GetBitContext *s, const uint8_t *buffer, int byte_size)
{
    if (byte_size > INT_MAX / 8 || byte_size < 0)
        byte_size = -1;
    return init_get_bits(s, buffer, byte_size * 8);
}

static inline int get_bits_count(const GetBitContext *s) { return s->index; }
static inline int get_bits_left(const GetBitContext *s) { return s->size_in_bits - s->index; }

/* Peeks up to 32 bits LSB-first. The caller's padding keeps the 8-byte load inside the buffer. */
static inline unsigned ffbink_show(const GetBitContext *s, int n)
{
    if (n <= 0 || !s->buffer)
        return 0;
    const uint64_t v = ffbink_rl64(s->buffer + (s->index >> 3)) >> (s->index & 7);
    return (unsigned)(v & ((n >= 32) ? 0xFFFFFFFFull : ((1ull << n) - 1)));
}

static inline void skip_bits_long(GetBitContext *s, int n)
{
    /* the checked reader clamps the index to [0, size_in_bits + 8] */
    long long idx = (long long)s->index + n;
    if (idx < 0) idx = 0;
    if (idx > s->size_in_bits_plus8) idx = s->size_in_bits_plus8;
    s->index = (int)idx;
}
static inline void skip_bits(GetBitContext *s, int n) { skip_bits_long(s, n); }

static inline unsigned get_bits(GetBitContext *s, int n)
{
    const unsigned v = ffbink_show(s, n);
    skip_bits_long(s, n);
    return v;
}
static inline unsigned get_bits_long(GetBitContext *s, int n) { return get_bits(s, n); }
static inline unsigned get_bits1(GetBitContext *s) { return get_bits(s, 1); }
static inline unsigned show_bits(const GetBitContext *s, int n) { return ffbink_show(s, n); }

static inline int get_vlc2(GetBitContext *s, const VLCElem *table, int bits, int max_depth)
{
    (void)max_depth;   /* bink.c's trees are single-level */
    const VLCElem e = table[ffbink_show(s, bits)];
    skip_bits_long(s, e.len);
    return e.sym;
}

/* ---- libavutil/tx.h --------------------------------------------------------------------- */
enum AVTXType { AV_TX_FLOAT_RDFT = 6, AV_TX_FLOAT_DCT = 9 };
typedef struct AVTXContext AVTXContext;
typedef void (*av_tx_fn)(AVTXContext *s, void *out, void *in, ptrdiff_t stride);
typedef struct AVComplexFloat { float re, im; } AVComplexFloat;
int  av_tx_init(AVTXContext **ctx, av_tx_fn *tx, enum AVTXType type, int inv, int len,
                const void *scale, uint64_t flags);
void av_tx_uninit(AVTXContext **ctx);

#endif /* FFBINK_SHIM_H */
