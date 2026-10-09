/* LGPL-2.1-or-later. A parser reads picture headers, never decodes pixels.
 * HLS's outer AVStream often has no parser/field metadata, even for PAFF. */
#ifndef NTV_STREAM_PROBE_H
#define NTV_STREAM_PROBE_H
#include "ntv_video_description.h"
static void ntv_measure_source(FFPlayer *ffp, AVStream *st, AVPacket *pkt, int serial)
{
    int structure = AV_PICTURE_STRUCTURE_UNKNOWN;
    if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
        if (!ffp->ntv_source_parser || ffp->ntv_source_parser_serial != serial) {
            if (ffp->ntv_source_parser) av_parser_close(ffp->ntv_source_parser);
            avcodec_free_context(&ffp->ntv_source_context);
            ffp->ntv_source_parser = av_parser_init(AV_CODEC_ID_H264);
            ffp->ntv_source_context = avcodec_alloc_context3(NULL);
            ffp->ntv_source_parser_serial = serial;
            if (ffp->ntv_source_parser && ffp->ntv_source_context) {
                ffp->ntv_source_parser->flags |= PARSER_FLAG_COMPLETE_FRAMES;
                avcodec_parameters_to_context(ffp->ntv_source_context, st->codecpar);
            }
        }
        if (ffp->ntv_source_parser && ffp->ntv_source_context) {
            uint8_t *parsed;
            int parsed_size;
            int ret = av_parser_parse2(ffp->ntv_source_parser, ffp->ntv_source_context,
                    &parsed, &parsed_size, pkt->data, pkt->size, pkt->pts, pkt->dts, pkt->pos);
            if (ret >= 0) structure = ffp->ntv_source_parser->picture_structure;
        }
    }
    ntv_describe_packet(ffp, st, st->codecpar->codec_id == AV_CODEC_ID_H264
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
