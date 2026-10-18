/* LGPL-2.1-or-later. Preserve the stream's actual colour description. */
#ifndef NTV_HDR_CODEC_H
#define NTV_HDR_CODEC_H
#include "ijksdl/android/ijksdl_codec_android_mediaformat.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/intreadwrite.h"
#include "ntv_hevc_color.h"
#include "ntv_h264_color.h"

static const void *ntv_hdr_side_data(AVStream *stream, AVCodecContext *ctx,
        enum AVPacketSideDataType type, size_t minimum)
{
    for (int i = 0; stream && i < stream->nb_side_data; ++i)
        if (stream->side_data[i].type == type && stream->side_data[i].size >= minimum)
            return stream->side_data[i].data;
    for (int i = 0; ctx && i < ctx->nb_coded_side_data; ++i)
        if (ctx->coded_side_data[i].type == type && ctx->coded_side_data[i].size >= minimum)
            return ctx->coded_side_data[i].data;
    return NULL;
}
static unsigned ntv_hdr_scaled(AVRational value, double scale)
{
    double number = value.den ? av_q2d(value) * scale : 0;
    return number > 65535 ? 65535 : number > 0 ? (unsigned)(number + 0.5) : 0;
}
static void ntv_hdr_codec_format(SDL_AMediaFormat *format, AVCodecParameters *par,
        AVStream *stream, AVCodecContext *ctx, int mode, int preferred_transfer)
{
    int matrix = ctx && ctx->colorspace != AVCOL_SPC_UNSPECIFIED ? ctx->colorspace : par->color_space;
    int trc = ctx && ctx->color_trc != AVCOL_TRC_UNSPECIFIED ? ctx->color_trc : par->color_trc;
    int primaries = ctx && ctx->color_primaries != AVCOL_PRI_UNSPECIFIED ? ctx->color_primaries : par->color_primaries;
    int range = ctx && ctx->color_range ? ctx->color_range : par->color_range;
    int source_profile = par->profile;
    int depth = ctx && ctx->bits_per_raw_sample ? ctx->bits_per_raw_sample : par->bits_per_raw_sample;
    NtvHevcColor color;
    if (par->codec_id == AV_CODEC_ID_HEVC)
        ALOGI("nTv HEVC input transfer: stream=%d context=%d",par->color_trc,ctx ? ctx->color_trc : -1);
    if ((par->codec_id == AV_CODEC_ID_HEVC
            && ntv_hevc_extradata_color(par->extradata,par->extradata_size,&color))
            || (par->codec_id == AV_CODEC_ID_H264
            && ntv_h264_extradata_color(par->extradata,par->extradata_size,&color))) {
        if (color.matrix != 2) matrix = color.matrix;
        trc = ntv_hevc_stream_transfer(trc, &color);
        if (color.primaries != 2) primaries = color.primaries;
        if (color.range) range = color.range;
        if (color.depth) { source_profile = color.profile; depth = color.depth; }
    }
    /* Opening FFmpeg's codec may re-export the SPS VUI into codecpar, so retain
     * the independently validated first-access-unit SEI across that step. */
    if (par->codec_id == AV_CODEC_ID_HEVC && ntv_hevc_valid_transfer(preferred_transfer))
        trc = preferred_transfer;
    int standard = matrix == AVCOL_SPC_BT2020_NCL ? 6
            : matrix == AVCOL_SPC_BT709 ? 1
            : matrix == AVCOL_SPC_BT470BG ? 2
            : matrix == AVCOL_SPC_SMPTE170M ? 4
            : matrix == AVCOL_SPC_UNSPECIFIED && primaries == AVCOL_PRI_BT2020 ? 6 : 0;
    int transfer = trc == AVCOL_TRC_SMPTE2084 ? 6
            : trc == AVCOL_TRC_ARIB_STD_B67 ? 7
            : trc == AVCOL_TRC_LINEAR ? 1
            : trc == AVCOL_TRC_BT709 || trc == AVCOL_TRC_SMPTE170M
                || trc == AVCOL_TRC_BT2020_10 || trc == AVCOL_TRC_BT2020_12 ? 3 : 0;
    if (!depth) depth = par->codec_id == AV_CODEC_ID_HEVC && source_profile == FF_PROFILE_HEVC_MAIN_10 ? 10 : 8;
    SDL_AMediaFormat_setInt32(format,"ntv-source-depth",depth);
    SDL_AMediaFormat_setInt32(format,"ntv-hdr-mode",mode);
    SDL_AMediaFormat_setInt32(format,"ntv-source-standard",standard);
    SDL_AMediaFormat_setInt32(format,"ntv-source-transfer",transfer);
    if (mode == 2) { standard = 1; transfer = 3; }
    if (standard) SDL_AMediaFormat_setInt32(format,"color-standard",standard);
    if (transfer) SDL_AMediaFormat_setInt32(format,"color-transfer",transfer);
    if (range != AVCOL_RANGE_UNSPECIFIED)
        SDL_AMediaFormat_setInt32(format,"color-range",range == AVCOL_RANGE_JPEG ? 1 : 2);
    if (par->codec_id == AV_CODEC_ID_HEVC
            && (source_profile == FF_PROFILE_HEVC_MAIN || source_profile == FF_PROFILE_HEVC_MAIN_10))
        SDL_AMediaFormat_setInt32(format,"profile",source_profile); /* Main=1, Main10=2 */
    const AVMasteringDisplayMetadata *mastering = ntv_hdr_side_data(stream,ctx,
            AV_PKT_DATA_MASTERING_DISPLAY_METADATA,sizeof(*mastering));
    const AVContentLightMetadata *light = ntv_hdr_side_data(stream,ctx,
            AV_PKT_DATA_CONTENT_LIGHT_LEVEL,sizeof(*light));
    if ((transfer == 6 || transfer == 7) && (mastering || light)) {
        uint8_t info[25] = {0}; /* CTA-861.3 Type 1, little endian, R/G/B order. */
        if (mastering && mastering->has_primaries) {
            for (int c = 0; c < 3; ++c)
                for (int xy = 0; xy < 2; ++xy)
                    AV_WL16(info + 1 + c*4 + xy*2,ntv_hdr_scaled(mastering->display_primaries[c][xy],50000));
            for (int xy = 0; xy < 2; ++xy)
                AV_WL16(info + 13 + xy*2,ntv_hdr_scaled(mastering->white_point[xy],50000));
        }
        if (mastering && mastering->has_luminance) {
            AV_WL16(info + 17,ntv_hdr_scaled(mastering->max_luminance,1));
            AV_WL16(info + 19,ntv_hdr_scaled(mastering->min_luminance,10000));
        }
        if (light) {
            AV_WL16(info + 21,FFMIN(light->MaxCLL,65535));
            AV_WL16(info + 23,FFMIN(light->MaxFALL,65535));
        }
        SDL_AMediaFormat_setBuffer(format,"hdr-static-info",info,sizeof(info));
    }
    if (transfer == 6 || transfer == 7 || mode == 2)
        ALOGI("nTv HDR: mode=%d standard=%d transfer=%d range=%d mastering=%d light=%d",mode,standard,transfer,
                par->color_range,mastering != NULL,light != NULL);
}
#endif
