/* LGPL-2.1-or-later. Source metadata only; never infer it from the output mode. */
#ifndef NTV_VIDEO_DESCRIPTION_H
#define NTV_VIDEO_DESCRIPTION_H
#include "libavutil/pixdesc.h"
#include "ntv_dolby_vision.h"
#define NTV_DESCRIPTION_AVAILABLE (1U << 24)
#define NTV_DESCRIPTION_FRAME (1U << 25)
static unsigned ntv_pixel_depth(int format)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(format);
    unsigned depth = 0;
    if (!desc || (desc->flags & AV_PIX_FMT_FLAG_HWACCEL)) return 0;
    for (int i = 0; i < desc->nb_components; ++i)
        if (desc->comp[i].depth > depth) depth = desc->comp[i].depth;
    return depth < 32 ? depth : 0;
}
static unsigned ntv_colour_value(int frame, int context, int stream)
{
    /* FFmpeg uses 2 for unspecified in all three colour enumerations. */
    return frame > 0 && frame != 2 ? frame
        : context > 0 && context != 2 ? context : stream;
}
static uint32_t ntv_describe_video(AVStream *st, AVCodecContext *ctx, AVFrame *frame)
{
    if (!st) return 0;
    AVCodecParameters *par = st->codecpar;
    unsigned depth = frame ? ntv_pixel_depth(frame->format) : 0;
    if (!depth && ctx) depth = ntv_pixel_depth(ctx->pix_fmt);
    if (!depth) depth = ntv_pixel_depth(par->format);
    if (!depth && par->bits_per_raw_sample > 0 && par->bits_per_raw_sample < 32)
        depth = par->bits_per_raw_sample;
    unsigned primaries = ntv_colour_value(frame ? frame->color_primaries : 2,
            ctx ? ctx->color_primaries : 2, par->color_primaries);
    unsigned transfer = ntv_colour_value(frame ? frame->color_trc : 2,
            ctx ? ctx->color_trc : 2, par->color_trc);
    unsigned matrix = ntv_colour_value(frame ? frame->colorspace : 2,
            ctx ? ctx->colorspace : 2, par->color_space);
    unsigned range = frame && frame->color_range ? frame->color_range
            : ctx && ctx->color_range ? ctx->color_range : par->color_range;
    return NTV_DESCRIPTION_AVAILABLE | depth | ((primaries & 31) << 5)
            | ((transfer & 31) << 10) | ((matrix & 31) << 15)
            | ((range & 3) << 20) | (ntv_dovi_stream(st) ? 1U << 22 : 0);
}
static void ntv_describe_packet(FFPlayer *ffp, AVStream *st, AVCodecContext *ctx)
{
    uint32_t old = __atomic_load_n(&ffp->ntv_video_description, __ATOMIC_RELAXED);
    uint32_t value = ntv_describe_video(st, ctx, NULL);
    /* Raw software frames are authoritative. Packet metadata must not overwrite
     * their updated colour/depth values, even when the two threads race. */
    while (!(old & NTV_DESCRIPTION_FRAME)) {
        if (__atomic_compare_exchange_n(&ffp->ntv_video_description, &old, value,
                    0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
    }
}
#endif
