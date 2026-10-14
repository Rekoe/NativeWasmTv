/* LGPL-2.1-or-later. Source metadata only; never infer it from the output mode. */
#ifndef NTV_VIDEO_DESCRIPTION_H
#define NTV_VIDEO_DESCRIPTION_H
#include "libavutil/pixdesc.h"
#include "ntv_dolby_vision.h"
#include "ntv_hevc_color.h"
#include "ntv_h264_color.h"
#define NTV_DESCRIPTION_AVAILABLE (1U << 24)
#define NTV_DESCRIPTION_FRAME (1U << 25)
#define NTV_DESCRIPTION_INFERRED (1U << 26)
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
static void ntv_restore_frame_transfer(AVStream *st, AVFrame *frame)
{
    if (!st || !frame || ntv_hevc_valid_transfer(frame->color_trc)
            || st->codecpar->codec_id != AV_CODEC_ID_HEVC) return;
    NtvHevcColor signal;
    if (st->codecpar->extradata_size > 0 && ntv_hevc_extradata_color(
                st->codecpar->extradata, st->codecpar->extradata_size, &signal))
        frame->color_trc = ntv_hevc_frame_transfer(frame->color_trc, signal.transfer);
}
static uint32_t ntv_describe_video_header(AVStream *st, AVCodecContext *ctx, AVFrame *frame, int packet_header)
{
    if (!st) return 0;
    AVCodecParameters *par = st->codecpar;
    NtvHevcColor signal;
    int hevc = par->codec_id == AV_CODEC_ID_HEVC && par->extradata_size > 0
            && ntv_hevc_extradata_color(par->extradata, par->extradata_size, &signal);
    int header = hevc || (par->codec_id == AV_CODEC_ID_H264
            && ntv_h264_extradata_color(par->extradata,par->extradata_size,&signal));
    /* The packet probe can observe a newer in-band AVC SPS than extradata.
     * Its private context keeps that source header without touching the codec. */
    if (!hevc && packet_header && par->codec_id == AV_CODEC_ID_H264 && ctx) {
        signal.depth = ctx->bits_per_raw_sample;
        signal.primaries = ctx->color_primaries;
        signal.transfer = ctx->color_trc;
        signal.matrix = ctx->colorspace;
        signal.range = ctx->color_range;
        signal.preferred_transfer = 0;
        header = 1;
    }
    unsigned depth = frame ? ntv_pixel_depth(frame->format) : 0;
    if (!depth && header) depth = signal.depth;
    if (!depth && ctx) depth = ntv_pixel_depth(ctx->pix_fmt);
    if (!depth && ctx && ctx->bits_per_raw_sample > 0 && ctx->bits_per_raw_sample < 32)
        depth = ctx->bits_per_raw_sample;
    if (!depth) depth = ntv_pixel_depth(par->format);
    if (!depth && par->bits_per_raw_sample > 0 && par->bits_per_raw_sample < 32)
        depth = par->bits_per_raw_sample;
    unsigned primaries = ntv_colour_value(frame ? frame->color_primaries : 2,
            header ? signal.primaries : 2,
            ntv_colour_value(ctx ? ctx->color_primaries : 2, 2, par->color_primaries));
    unsigned transfer = ntv_colour_value(frame ? ntv_hevc_frame_transfer(
                frame->color_trc, hevc ? signal.transfer : 2) : 2,
            header ? ntv_hevc_stream_transfer(ctx ? ctx->color_trc : par->color_trc, &signal) : 2,
            ntv_colour_value(ctx ? ctx->color_trc : 2, 2, par->color_trc));
    unsigned matrix = ntv_colour_value(frame ? frame->colorspace : 2,
            header ? signal.matrix : 2,
            ntv_colour_value(ctx ? ctx->colorspace : 2, 2, par->color_space));
    unsigned range = frame && frame->color_range ? frame->color_range
            : header && signal.range ? signal.range
            : ctx && ctx->color_range ? ctx->color_range : par->color_range;
    unsigned inferred = 0;
    /* Ordinary 8-bit AVC/MPEG-2 broadcasts often omit all colour tags. Expose
     * the conservative playback interpretation with a separate provenance bit.
     * Never infer HDR/SDR for HEVC, >8bit, wide-gamut or partially tagged video.
     * These defaults are descriptive only: no codec/container fields are changed. */
    if (depth == 8 && (par->codec_id == AV_CODEC_ID_H264 || par->codec_id == AV_CODEC_ID_MPEG2VIDEO)
            && primaries == 2 && transfer == 2 && matrix == 2 && par->width > 0 && par->height > 0) {
        primaries = matrix = par->height > 576 ? 1 : par->height > 480 ? 5 : 6;
        transfer = par->height > 576 ? 1 : 6;
        inferred = NTV_DESCRIPTION_INFERRED;
    }
    /* Scan order is source metadata, independent of decoder output cadence. */
    unsigned order = frame ? (frame->interlaced_frame
            ? (frame->top_field_first ? AV_FIELD_TT : AV_FIELD_BB) : AV_FIELD_PROGRESSIVE)
            : ctx && ctx->field_order != AV_FIELD_UNKNOWN ? ctx->field_order : par->field_order;
    if (order > AV_FIELD_BT) order = AV_FIELD_UNKNOWN;
    if (order == AV_FIELD_UNKNOWN && hevc && signal.scan)
        order = signal.scan == 1 ? AV_FIELD_PROGRESSIVE : 6; /* source fields; order unknown */
    return NTV_DESCRIPTION_AVAILABLE | inferred | (order << 27) | depth | ((primaries & 31) << 5)
            | ((transfer & 31) << 10) | ((matrix & 31) << 15)
            | ((range & 3) << 20) | (ntv_dovi_stream(st) ? 1U << 22 : 0);
}
static uint32_t ntv_describe_video(AVStream *st, AVCodecContext *ctx, AVFrame *frame)
{
    return ntv_describe_video_header(st,ctx,frame,0);
}
static void ntv_describe_packet(FFPlayer *ffp, AVStream *st, AVCodecContext *ctx)
{
    uint32_t old = __atomic_load_n(&ffp->ntv_video_description, __ATOMIC_RELAXED);
    uint32_t value = ntv_describe_video_header(st,ctx,NULL,ffp->ntv_source_avc_header);
    if (st->codecpar->codec_id == AV_CODEC_ID_HEVC && ffp->ntv_source_preferred_transfer)
        value = (value & ~(31U << 10)) | ((ffp->ntv_source_preferred_transfer & 31U) << 10);
    /* Raw software frames are authoritative. Packet metadata must not overwrite
     * their updated colour/depth values, even when the two threads race. */
    while (!(old & NTV_DESCRIPTION_FRAME)) {
        if (__atomic_compare_exchange_n(&ffp->ntv_video_description, &old, value,
                    0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) break;
    }
}
#endif
