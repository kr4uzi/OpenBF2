/*
 * [CHOSEN, not recovered] OpenBF2 macOS port: implementations behind ffbink_shim.h (the
 * libavutil/libavcodec services the vendored FFmpeg Bink decoders call) and the ffbink.h
 * interface. Port code written for OpenBF2, not taken from FFmpeg; see ../README.md.
 */
#include "ffbink_shim.h"
#include "ffbink.h"

#include <stdarg.h>
#include <stdio.h>

extern const FFCodec ff_bink_decoder;
extern const FFCodec ff_binkaudio_dct_decoder;
extern const FFCodec ff_binkaudio_rdft_decoder;

/* ---- logging, assertions, memory ---------------------------------------------------------- */

void av_log(void *avcl, int level, const char *fmt, ...)
{
    (void)avcl;
    static int verbose = -1;
    if (verbose < 0)
        verbose = getenv("BF2_BINK_VERBOSE") != NULL;
    if (level > AV_LOG_WARNING && !verbose)
        return;
    va_list ap;
    va_start(ap, fmt);
    fputs("[bink] ", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void ffbink_assert_fail(const char *what, const char *file, int line)
{
    fprintf(stderr, "[bink] assertion %s failed at %s:%d\n", what, file, line);
    abort();
}

#if defined(_WIN32)
#include <malloc.h>
#include <windows.h>

void *av_malloc(size_t size) { return _aligned_malloc(size ? size : 1, 64); }
static void ffbink_free(void *p) { _aligned_free(p); }

static BOOL CALLBACK ffbink_once_cb(PINIT_ONCE once, PVOID routine, PVOID *ctx)
{
    (void)once; (void)ctx;
    ((void (*)(void))routine)();
    return TRUE;
}

int ffbink_once(AVOnce *control, void (*routine)(void))
{
    return InitOnceExecuteOnce((PINIT_ONCE)control, ffbink_once_cb, (PVOID)routine, NULL) ? 0 : -1;
}
#else
void *av_malloc(size_t size)
{
    void *p = NULL;
    if (posix_memalign(&p, 64, size ? size : 1))
        return NULL;
    return p;
}
static void ffbink_free(void *p) { free(p); }
#endif

void *av_mallocz(size_t size)
{
    void *p = av_malloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

void *av_calloc(size_t nmemb, size_t size)
{
    if (size && nmemb > SIZE_MAX / size)
        return NULL;
    return av_mallocz(nmemb * size);
}

void av_free(void *ptr) { ffbink_free(ptr); }

void av_freep(void *arg)
{
    void **p = (void **)arg;
    ffbink_free(*p);
    *p = NULL;
}

int av_image_check_size(unsigned int w, unsigned int h, int log_offset, void *log_ctx)
{
    (void)log_offset; (void)log_ctx;
    if (w > 0 && h > 0 && (w + 128) * (uint64_t)(h + 128) < INT_MAX / 8)
        return 0;
    return AVERROR(EINVAL);
}

/* ---- packets and frames ------------------------------------------------------------------- */

void av_packet_unref(AVPacket *pkt)
{
    pkt->data = NULL;
    pkt->size = 0;
}

struct FFBinkBuffer {
    int      refs;
    uint8_t *mem;
};

static FFBinkBuffer *buffer_new(size_t size)
{
    FFBinkBuffer *b = (FFBinkBuffer *)calloc(1, sizeof(*b));
    if (!b)
        return NULL;
    b->mem = (uint8_t *)av_mallocz(size);
    if (!b->mem) {
        free(b);
        return NULL;
    }
    b->refs = 1;
    return b;
}

static void buffer_unref(FFBinkBuffer *b)
{
    if (b && --b->refs == 0) {
        av_free(b->mem);
        free(b);
    }
}

AVFrame *av_frame_alloc(void)
{
    return (AVFrame *)calloc(1, sizeof(AVFrame));
}

void av_frame_unref(AVFrame *frame)
{
    buffer_unref(frame->buf);
    memset(frame, 0, sizeof(*frame));
}

void av_frame_free(AVFrame **frame)
{
    if (!*frame)
        return;
    av_frame_unref(*frame);
    free(*frame);
    *frame = NULL;
}

int av_frame_ref(AVFrame *dst, const AVFrame *src)
{
    if (dst == src)
        return 0;
    *dst = *src;
    dst->extended_data = dst->data;
    if (dst->buf)
        dst->buf->refs++;
    return 0;
}

int av_frame_replace(AVFrame *dst, const AVFrame *src)
{
    if (dst == src)
        return 0;
    FFBinkBuffer *old = dst->buf;
    if (src->buf)
        src->buf->refs++;
    *dst = *src;
    dst->extended_data = dst->data;
    buffer_unref(old);
    return 0;
}

/* Video planes are padded to 32x32 luma blocks and 64-byte strides, like FFmpeg's default
 * allocator; audio gets one plane per channel (planar) or one interleaved plane. */
int ff_get_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    (void)flags;
    const int nb_samples = frame->nb_samples;   /* binkaudio sets it before asking */
    av_frame_unref(frame);
    frame->nb_samples = nb_samples;
    if (avctx->codec->type == AVMEDIA_TYPE_VIDEO) {
        const int w  = FFALIGN(avctx->width, 32), h = FFALIGN(avctx->height, 32);
        const int ly = FFALIGN(w, 64), lc = FFALIGN(w / 2, 64);
        const int alpha = avctx->pix_fmt == AV_PIX_FMT_YUVA420P;
        const size_t sy = (size_t)ly * h, sc = (size_t)lc * (h / 2);
        FFBinkBuffer *b = buffer_new(sy * (alpha ? 2 : 1) + 2 * sc);
        if (!b)
            return AVERROR(ENOMEM);
        frame->buf         = b;
        frame->data[0]     = b->mem;
        frame->data[1]     = b->mem + sy;
        frame->data[2]     = b->mem + sy + sc;
        frame->linesize[0] = ly;
        frame->linesize[1] = lc;
        frame->linesize[2] = lc;
        if (alpha) {
            frame->data[3]     = b->mem + sy + 2 * sc;
            frame->linesize[3] = ly;
        }
        frame->width  = avctx->width;
        frame->height = avctx->height;
        frame->format = avctx->pix_fmt;
    } else {
        const int ch = avctx->ch_layout.nb_channels;
        const int planar = avctx->sample_fmt == AV_SAMPLE_FMT_FLTP;
        const int planes = planar ? ch : 1;
        const size_t per = (size_t)FFMAX(nb_samples, 0) * (planar ? 1 : ch);
        if (planes < 1 || planes > 8)
            return AVERROR(EINVAL);
        FFBinkBuffer *b = buffer_new(per * sizeof(float) * planes);
        if (!b)
            return AVERROR(ENOMEM);
        frame->buf = b;
        for (int i = 0; i < planes; i++) {
            frame->data[i]     = b->mem + i * per * sizeof(float);
            frame->linesize[i] = (int)(per * sizeof(float));
        }
        frame->format = avctx->sample_fmt;
    }
    frame->extended_data = frame->data;
    frame->pts = AV_NOPTS_VALUE;
    return 0;
}

/* bink.c's revision-'b' path decodes in place into the reference frame. */
int ff_reget_buffer(AVCodecContext *avctx, AVFrame *frame, int flags)
{
    if (!frame->buf)
        return ff_get_buffer(avctx, frame, flags);
    if (frame->buf->refs == 1)
        return 0;
    AVFrame copy = { { 0 } };
    int ret = ff_get_buffer(avctx, &copy, flags);
    if (ret < 0)
        return ret;
    for (int p = 0; p < 4 && frame->data[p]; p++) {
        const int rows = (p == 1 || p == 2) ? FFALIGN(avctx->height, 32) / 2 : FFALIGN(avctx->height, 32);
        memcpy(copy.data[p], frame->data[p], (size_t)frame->linesize[p] * rows);
    }
    av_frame_unref(frame);
    *frame = copy;
    frame->extended_data = frame->data;
    return 0;
}

int ff_decode_get_packet(AVCodecContext *avctx, AVPacket *pkt)
{
    AVCodecInternal *in = avctx->internal;
    if (!in->pending.data)
        return AVERROR(EAGAIN);
    *pkt = in->pending;
    av_packet_unref(&in->pending);
    return 0;
}

/* ---- blockdsp / hpeldsp: the C reference behaviour of the routines bink.c selects ---------- */

static void clear_block_c(int16_t *block) { memset(block, 0, sizeof(int16_t) * 64); }

static void fill_block16_c(uint8_t *block, uint8_t value, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++, block += line_size)
        memset(block, value, 16);
}

static void fill_block8_c(uint8_t *block, uint8_t value, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++, block += line_size)
        memset(block, value, 8);
}

void ff_blockdsp_init(BlockDSPContext *c)
{
    c->clear_block       = clear_block_c;
    c->fill_block_tab[0] = fill_block16_c;
    c->fill_block_tab[1] = fill_block8_c;
}

static void put_pixels16_c(uint8_t *block, const uint8_t *pixels, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++, block += line_size, pixels += line_size)
        memmove(block, pixels, 16);
}

static void put_pixels8_c(uint8_t *block, const uint8_t *pixels, ptrdiff_t line_size, int h)
{
    for (int i = 0; i < h; i++, block += line_size, pixels += line_size)
        memmove(block, pixels, 8);
}

void ff_hpeldsp_init(HpelDSPContext *c, int flags)
{
    (void)flags;
    memset(c, 0, sizeof(*c));
    c->put_pixels_tab[0][0] = put_pixels16_c;
    c->put_pixels_tab[1][0] = put_pixels8_c;
}

/* ---- VLC: single-level little-endian tables --------------------------------------------- */

/* With VLC_INIT_LE a code is read LSB-first, so every table index whose low `len` bits equal
 * the code maps to that symbol (FFmpeg's vlc.c: OUTPUT_LE, inc = 1 << len). */
int ffbink_vlc_init(VLC *vlc, int nb_bits, int nb_codes, const uint8_t *lens, int lens_wrap,
                    int lens_size, const uint8_t *codes, int codes_wrap, int codes_size, int flags)
{
    if (lens_wrap != 1 || lens_size != 1 || codes_wrap != 1 || codes_size != 1 ||
        (flags & VLC_INIT_LE) != VLC_INIT_LE || !(flags & VLC_INIT_USE_STATIC) ||
        nb_bits < 1 || nb_bits > 15 || vlc->table_allocated < (1 << nb_bits)) {
        av_log(NULL, AV_LOG_ERROR, "unsupported vlc_init form\n");
        return AVERROR(EINVAL);
    }
    const int size = 1 << nb_bits;
    for (int j = 0; j < size; j++) {
        vlc->table[j].sym = 0;
        vlc->table[j].len = 0;
    }
    for (int i = 0; i < nb_codes; i++) {
        const int len = lens[i];
        const int code = codes[i];
        if (!len)
            continue;
        if (len > nb_bits || code >= (1 << len)) {
            av_log(NULL, AV_LOG_ERROR, "invalid vlc code %d/%d\n", code, len);
            return AVERROR(EINVAL);
        }
        for (int j = code; j < size; j += 1 << len) {
            if (vlc->table[j].len) {
                av_log(NULL, AV_LOG_ERROR, "incorrect codes\n");
                return AVERROR_INVALIDDATA;
            }
            vlc->table[j].sym = (VLCBaseType)i;
            vlc->table[j].len = (VLCBaseType)len;
        }
    }
    vlc->bits       = nb_bits;
    vlc->table_size = size;
    return 0;
}

/* ---- tx: the inverse DCT (DCT-III) binkaudio_dct uses ----------------------------------- */

struct AVTXContext {
    int             n;      /* transform length (frame_len) */
    float           scale;
    AVComplexFloat *buf;    /* n */
    AVComplexFloat *tw;     /* n/2 inverse-FFT twiddles e^{+2 pi i k / n} */
    AVComplexFloat *pre;    /* n   pre-rotation e^{+i pi k / 2n} */
    int            *rev;    /* n   bit reversal */
};

static void tx_ifft(AVTXContext *s)
{
    const int n = s->n;
    AVComplexFloat *z = s->buf;
    for (int i = 0; i < n; i++) {
        const int r = s->rev[i];
        if (r > i) {
            AVComplexFloat t = z[i];
            z[i] = z[r];
            z[r] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const int half = len >> 1, step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int k = 0; k < half; k++) {
                const AVComplexFloat w = s->tw[k * step];
                AVComplexFloat *a = &z[i + k], *b = &z[i + k + half];
                const float tr = b->re * w.re - b->im * w.im;
                const float ti = b->re * w.im + b->im * w.re;
                b->re = a->re - tr;
                b->im = a->im - ti;
                a->re += tr;
                a->im += ti;
            }
        }
    }
}

/* out[m] = scale * (X[0] + 2 sum_{k=1}^{n-1} X[k] cos(pi k (2m + 1) / 2n)), m = 0..n-1: the
 * values of FFmpeg's inverse AV_TX_FLOAT_DCT (DCT-III), computed with one n-point complex
 * inverse FFT: V[k] = (X[k] - i X[n-k]) e^{i pi k / 2n} (X[n] = 0), v = IFFT(V) unnormalised,
 * out[2j] = scale Re v[j], out[2j+1] = scale Re v[n-1-j]. bf2_bink_decode_test checks the
 * result against the ffmpeg CLI's decode of the shipped movies. */
static void tx_dct3(AVTXContext *s, void *_out, void *_in, ptrdiff_t stride)
{
    (void)stride;
    const int n = s->n;
    const float *x = (const float *)_in;
    float *out = (float *)_out;
    for (int k = 0; k < n; k++) {
        const float a = x[k], b = k ? -x[n - k] : 0.0f;
        const AVComplexFloat p = s->pre[k];
        s->buf[k].re = a * p.re - b * p.im;
        s->buf[k].im = a * p.im + b * p.re;
    }
    tx_ifft(s);
    const float h = s->scale;
    for (int j = 0; j < n / 2; j++) {
        out[2 * j]     = s->buf[j].re * h;
        out[2 * j + 1] = s->buf[n - 1 - j].re * h;
    }
}

int av_tx_init(AVTXContext **ctx, av_tx_fn *tx, enum AVTXType type, int inv, int len,
               const void *scale, uint64_t flags)
{
    (void)flags;
    *ctx = NULL;
    if (type != AV_TX_FLOAT_DCT || !inv) {
        /* [FRONTIER] binkaudio_rdft (and forward transforms) are not part of this subset: no
         * Battlefield 2 movie carries an RDFT track. ffbink_audio_open reports the refusal. */
        return AVERROR(ENOSYS);
    }
    const int n = len * 2;   /* the inverse DCT runs at twice the requested length, as in FFmpeg */
    if (n < 4 || (n & (n - 1)))
        return AVERROR(EINVAL);
    AVTXContext *s = (AVTXContext *)calloc(1, sizeof(*s));
    if (!s)
        return AVERROR(ENOMEM);
    s->n     = n;
    s->scale = *(const float *)scale;
    s->buf   = (AVComplexFloat *)av_malloc(sizeof(AVComplexFloat) * n);
    s->tw    = (AVComplexFloat *)av_malloc(sizeof(AVComplexFloat) * (n / 2));
    s->pre   = (AVComplexFloat *)av_malloc(sizeof(AVComplexFloat) * n);
    s->rev   = (int *)av_malloc(sizeof(int) * n);
    if (!s->buf || !s->tw || !s->pre || !s->rev) {
        av_tx_uninit(&s);
        return AVERROR(ENOMEM);
    }
    const int bits = av_log2((unsigned)n);
    for (int i = 0; i < n; i++) {
        int r = 0;
        for (int b = 0; b < bits; b++)
            r |= ((i >> b) & 1) << (bits - 1 - b);
        s->rev[i] = r;
        s->pre[i].re = (float)cos(M_PI * i / (2.0 * n));
        s->pre[i].im = (float)sin(M_PI * i / (2.0 * n));
    }
    for (int k = 0; k < n / 2; k++) {
        s->tw[k].re = (float)cos(2.0 * M_PI * k / n);
        s->tw[k].im = (float)sin(2.0 * M_PI * k / n);
    }
    *ctx = s;
    *tx  = tx_dct3;
    return 0;
}

void av_tx_uninit(AVTXContext **ctx)
{
    AVTXContext *s = *ctx;
    if (!s)
        return;
    av_free(s->buf);
    av_free(s->tw);
    av_free(s->pre);
    av_free(s->rev);
    free(s);
    *ctx = NULL;
}

/* ---- ffbink.h ------------------------------------------------------------------------------ */

typedef struct FFBinkCodec {
    const FFCodec  *codec;
    AVCodecContext  ctx;
    AVCodecInternal internal;
    AVPacket        in_pkt;
    AVFrame        *frame;
    uint8_t         extradata[4 + AV_INPUT_BUFFER_PADDING_SIZE];
    int             opened;
} FFBinkCodec;

static void codec_close(FFBinkCodec *c)
{
    if (c->opened && c->codec->close)
        c->codec->close(&c->ctx);
    av_frame_free(&c->frame);
    free(c->ctx.priv_data);
    c->ctx.priv_data = NULL;
    c->opened = 0;
}

static int codec_open(FFBinkCodec *c, const FFCodec *codec, uint32_t extradata)
{
    memset(c, 0, sizeof(*c));
    c->codec              = codec;
    c->ctx.codec          = &codec->p;
    c->ctx.internal       = &c->internal;
    c->internal.in_pkt    = &c->in_pkt;
    c->extradata[0]       = (uint8_t)extradata;
    c->extradata[1]       = (uint8_t)(extradata >> 8);
    c->extradata[2]       = (uint8_t)(extradata >> 16);
    c->extradata[3]       = (uint8_t)(extradata >> 24);
    c->ctx.extradata      = c->extradata;
    c->ctx.extradata_size = 4;
    c->ctx.priv_data      = calloc(1, (size_t)codec->priv_data_size);
    c->frame              = av_frame_alloc();
    if (!c->ctx.priv_data || !c->frame)
        return AVERROR(ENOMEM);
    return 0;
}

static int codec_init(FFBinkCodec *c)
{
    const int ret = c->codec->init(&c->ctx);
    c->opened = 1;   /* FF_CODEC_CAP_INIT_CLEANUP: close runs after a failed init too */
    return ret;
}

struct FFBinkVideo { FFBinkCodec c; };
struct FFBinkAudio { FFBinkCodec c; };

FFBinkVideo *ffbink_video_open(uint32_t codec_tag, uint32_t video_flags, int width, int height)
{
    FFBinkVideo *v = (FFBinkVideo *)calloc(1, sizeof(*v));
    if (!v)
        return NULL;
    if (codec_open(&v->c, &ff_bink_decoder, video_flags) < 0)
        goto fail;
    v->c.ctx.codec_tag = codec_tag;
    v->c.ctx.width     = width;
    v->c.ctx.height    = height;
    if (codec_init(&v->c) < 0)
        goto fail;
    return v;
fail:
    ffbink_video_close(v);
    return NULL;
}

int ffbink_video_decode(FFBinkVideo *v, const uint8_t *data, int size, FFBinkPicture *out)
{
    AVPacket pkt = { data, size };
    int got = 0;
    av_frame_unref(v->c.frame);
    const int ret = v->c.codec->cb.decode(&v->c.ctx, v->c.frame, &got, &pkt);
    if (ret < 0)
        return ret;
    if (!got)
        return AVERROR_INVALIDDATA;
    const AVFrame *f = v->c.frame;
    for (int p = 0; p < 4; p++) {
        out->plane[p]    = f->data[p];
        out->linesize[p] = f->linesize[p];
    }
    out->width      = v->c.ctx.width;
    out->height     = v->c.ctx.height;
    out->has_alpha  = v->c.ctx.pix_fmt == AV_PIX_FMT_YUVA420P;
    out->full_range = v->c.ctx.color_range == AVCOL_RANGE_JPEG;
    return 0;
}

void ffbink_video_flush(FFBinkVideo *v)
{
    if (v->c.codec->flush)
        v->c.codec->flush(&v->c.ctx);
}

void ffbink_video_close(FFBinkVideo *v)
{
    if (!v)
        return;
    codec_close(&v->c);
    free(v);
}

FFBinkAudio *ffbink_audio_open(uint32_t codec_tag, int sample_rate, int channels, int use_dct)
{
    FFBinkAudio *a = (FFBinkAudio *)calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    /* FFmpeg's demuxer hands the audio decoder the file tag as extradata (version_b test). */
    if (codec_open(&a->c, use_dct ? &ff_binkaudio_dct_decoder : &ff_binkaudio_rdft_decoder,
                   codec_tag) < 0)
        goto fail;
    a->c.ctx.sample_rate = sample_rate;
    av_channel_layout_default(&a->c.ctx.ch_layout, channels);
    if (codec_init(&a->c) < 0) {
        if (!use_dct)
            av_log(NULL, AV_LOG_ERROR, "[FRONTIER] Bink RDFT audio is not in the vendored subset\n");
        goto fail;
    }
    return a;
fail:
    ffbink_audio_close(a);
    return NULL;
}

int ffbink_audio_decode(FFBinkAudio *a, const uint8_t *data, int size, ffbink_audio_sink sink, void *user)
{
    int total = 0;
    a->c.internal.pending.data = data;
    a->c.internal.pending.size = size;
    for (;;) {
        const int ret = a->c.codec->cb.receive_frame(&a->c.ctx, a->c.frame);
        if (ret == AVERROR(EAGAIN))
            break;
        if (ret < 0) {
            av_packet_unref(&a->c.internal.pending);
            av_packet_unref(&a->c.in_pkt);
            return total ? total : ret;
        }
        const int ch = a->c.ctx.ch_layout.nb_channels;
        if (sink)
            sink(user, (const float *const *)a->c.frame->extended_data, ch, a->c.frame->nb_samples);
        total += a->c.frame->nb_samples;
    }
    return total;
}

void ffbink_audio_flush(FFBinkAudio *a)
{
    av_packet_unref(&a->c.internal.pending);
    av_packet_unref(&a->c.in_pkt);
    if (a->c.codec->flush)
        a->c.codec->flush(&a->c.ctx);
}

void ffbink_audio_close(FFBinkAudio *a)
{
    if (!a)
        return;
    codec_close(&a->c);
    free(a);
}
