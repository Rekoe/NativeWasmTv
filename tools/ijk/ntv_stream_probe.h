/* LGPL-2.1-or-later. A parser reads picture headers, never decodes pixels.
 * HLS's outer AVStream often has no parser/field metadata, even for PAFF. */
#ifndef NTV_STREAM_PROBE_H
#define NTV_STREAM_PROBE_H
#include "ntv_video_description.h"
typedef struct NtvHevcProbePackets {
    AVPacket packet[64];
    unsigned count, next;
} NtvHevcProbePackets;
static void ntv_hevc_probe_clear(NtvHevcProbePackets *probe)
{
    while (probe->next < probe->count) av_packet_unref(&probe->packet[probe->next++]);
    probe->next = probe->count = 0;
}
static int ntv_hevc_probe_initial(AVFormatContext *ic, const int *indices, NtvHevcProbePackets *probe)
{
    int index = indices[AVMEDIA_TYPE_VIDEO];
    if (index < 0) return 0;
    AVCodecParameters *par = ic->streams[index]->codecpar;
    if (par->codec_id != AV_CODEC_ID_HEVC && par->codec_id != AV_CODEC_ID_H264) return 0;
    if (ntv_dovi_stream(ic->streams[index])) return 0;
    int h264 = par->codec_id == AV_CODEC_ID_H264;
    NtvHevcColor signal;
    if (h264) {
        /* Existing extradata is enough; do not wait for a redundant live keyframe. */
        if (ntv_h264_extradata_color(par->extradata,par->extradata_size,&signal)) return 0;
    } else if (!ntv_hevc_extradata_color(par->extradata,par->extradata_size,&signal)
            || signal.primaries != 9 || (signal.transfer != 2 && signal.transfer != 14 && signal.transfer != 15)) return 0;
    int selected[] = {index,indices[AVMEDIA_TYPE_AUDIO],indices[AVMEDIA_TYPE_SUBTITLE]};
    enum AVDiscard discard[3];
    for (int n = 0; n < 3; ++n) if (selected[n] >= 0) {
        discard[n] = ic->streams[selected[n]]->discard;
        ic->streams[selected[n]]->discard = AVDISCARD_DEFAULT;
    }
    /* FFmpeg 3.4's container probe can keep the SDR-compatible VUI even though
     * the first access unit carries an HLG Alternative Transfer SEI. Read that
     * header before configuring MediaCodec; retain every packet in order. */
    unsigned bytes = 0; int preferred = 0;
    while (probe->count < 64 && bytes < 4*1024*1024) {
        AVPacket *pkt = &probe->packet[probe->count];
        if (av_read_frame(ic,pkt) < 0) { av_packet_unref(pkt); break; }
        ++probe->count; bytes += pkt->size;
        if (pkt->stream_index != index) continue;
        if (h264 && ntv_h264_packet_color(pkt->data,pkt->size,par->extradata,par->extradata_size,&signal)) {
            par->bits_per_raw_sample = signal.depth;
            if (par->color_primaries == 2) par->color_primaries = signal.primaries;
            if (par->color_trc == 2) par->color_trc = signal.transfer;
            if (par->color_space == 2) par->color_space = signal.matrix;
            if (!par->color_range) par->color_range = signal.range;
            break;
        }
        if (!h264 && ntv_hevc_packet_color(pkt->data,pkt->size,par->extradata,par->extradata_size,&signal)
                && signal.preferred_transfer) {
            par->color_trc = signal.preferred_transfer;
            preferred = signal.preferred_transfer;
            break;
        }
        if (pkt->flags & AV_PKT_FLAG_KEY) break;
    }
    for (int n = 0; n < 3; ++n) if (selected[n] >= 0)
        ic->streams[selected[n]]->discard = discard[n];
    av_log(ic,AV_LOG_INFO,"nTv source header probe: packets=%u transfer=%d\n",probe->count,par->color_trc);
    return preferred;
}
static int ntv_hevc_probe_read(AVFormatContext *ic, AVPacket *pkt, NtvHevcProbePackets *probe)
{
    if (probe->next < probe->count) {
        av_packet_move_ref(pkt,&probe->packet[probe->next++]);
        return 0;
    }
    return av_read_frame(ic,pkt);
}
static void ntv_probe_dimensions(AVFormatContext *ic, int index)
{
    if (index < 0) return;
    AVCodecParameters *par = ic->streams[index]->codecpar;
    if (par->width > 0 && par->height > 0) return;
    int h264 = par->codec_id == AV_CODEC_ID_H264;
    if (!h264 && par->codec_id != AV_CODEC_ID_HEVC) return;
    NtvHevcColor signal = {0};
    int found = h264 ? ntv_h264_extradata_color(par->extradata,par->extradata_size,&signal)
            : ntv_hevc_extradata_color(par->extradata,par->extradata_size,&signal);
    found = found && signal.width > 0 && signal.height > 0;
    unsigned bytes = 0;
    for (unsigned n = 0; !found && n < 64 && bytes < 4*1024*1024; ++n) {
        AVPacket pkt = {0}; av_init_packet(&pkt);
        if (av_read_frame(ic,&pkt) < 0) { av_packet_unref(&pkt); break; }
        bytes += pkt.size;
        if (pkt.stream_index == index) found = h264
                ? ntv_h264_packet_color(pkt.data,pkt.size,par->extradata,par->extradata_size,&signal)
                : ntv_hevc_packet_color(pkt.data,pkt.size,par->extradata,par->extradata_size,&signal);
        av_packet_unref(&pkt);
        found = found && signal.width > 0 && signal.height > 0;
    }
    if (found && signal.width > 0 && signal.height > 0) {
        par->width = signal.width; par->height = signal.height;
    }
}
static void ntv_measure_source(FFPlayer *ffp, AVStream *st, AVPacket *pkt, int serial)
{
    int structure = AV_PICTURE_STRUCTURE_UNKNOWN;
    int h264 = st->codecpar->codec_id == AV_CODEC_ID_H264;
    int hevc = st->codecpar->codec_id == AV_CODEC_ID_HEVC;
    if (h264 || hevc) {
        if (!ffp->ntv_source_context || ffp->ntv_source_parser_serial != serial
                || ffp->ntv_source_context->codec_id != st->codecpar->codec_id) {
            if (ffp->ntv_source_parser) av_parser_close(ffp->ntv_source_parser);
            avcodec_free_context(&ffp->ntv_source_context);
            ffp->ntv_source_parser = h264 ? av_parser_init(AV_CODEC_ID_H264) : NULL;
            ffp->ntv_source_context = avcodec_alloc_context3(NULL);
            ffp->ntv_source_parser_serial = serial;
            ffp->ntv_source_preferred_transfer = 0;
            ffp->ntv_source_avc_header = 0;
            if (ffp->ntv_source_context) {
                if (ffp->ntv_source_parser) ffp->ntv_source_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
                avcodec_parameters_to_context(ffp->ntv_source_context, st->codecpar);
            }
        }
        if ((hevc || h264) && ffp->ntv_source_context) {
            NtvHevcColor color;
            if (hevc ? ntv_hevc_packet_color(pkt->data, pkt->size,
                        st->codecpar->extradata,st->codecpar->extradata_size,&color)
                    : ntv_h264_packet_color(pkt->data,pkt->size,
                        st->codecpar->extradata,st->codecpar->extradata_size,&color)) {
                AVCodecContext *ctx = ffp->ntv_source_context;
                if (color.depth) {
                    if (h264) ffp->ntv_source_avc_header = 1;
                    ctx->pix_fmt = AV_PIX_FMT_NONE;
                    ctx->bits_per_raw_sample = color.depth;
                    ctx->color_primaries = color.primaries;
                    ctx->color_trc = color.transfer;
                    ctx->colorspace = color.matrix;
                    ctx->color_range = color.range;
                    if (hevc && color.scan == 1) ctx->field_order = AV_FIELD_PROGRESSIVE;
                }
                if (color.preferred_transfer) ffp->ntv_source_preferred_transfer = color.preferred_transfer;
                if (ffp->ntv_source_preferred_transfer) ctx->color_trc = ffp->ntv_source_preferred_transfer;
            }
        }
        if (ffp->ntv_source_parser && ffp->ntv_source_context) {
            uint8_t *parsed;
            int parsed_size;
            int ret = av_parser_parse2(ffp->ntv_source_parser, ffp->ntv_source_context,
                    &parsed, &parsed_size, pkt->data, pkt->size, pkt->pts, pkt->dts, pkt->pos);
            if (ret >= 0) {
                structure = ffp->ntv_source_parser->picture_structure;
                if (ffp->ntv_source_parser->field_order != AV_FIELD_UNKNOWN)
                    ffp->ntv_source_context->field_order = ffp->ntv_source_parser->field_order;
            }
        }
    }
    ntv_describe_packet(ffp, st, h264 || hevc
            ? ffp->ntv_source_context : NULL);
    if (!ffp->ntv_timing.source_count)
        av_log(ffp, AV_LOG_INFO, "nTv cadence: picture_structure=%d field_order=%d\n",
                structure, st->codecpar->field_order);
    /* Two separately timestamped fields form one frame. Complete interlaced
     * pictures (MBAFF) and progressive pictures remain one frame per packet. */
    if (structure == AV_PICTURE_STRUCTURE_BOTTOM_FIELD) return;
    int64_t stamp = pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
    ntv_source_frame(&ffp->ntv_timing,
            stamp == AV_NOPTS_VALUE ? NAN : stamp * av_q2d(st->time_base), serial);
}
#endif
