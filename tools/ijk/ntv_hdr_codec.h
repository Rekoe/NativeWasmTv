/* LGPL-2.1-or-later. Preserve the stream's actual colour description. */
#ifndef NTV_HDR_CODEC_H
#define NTV_HDR_CODEC_H
#include "ijksdl/android/ijksdl_codec_android_mediaformat.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/intreadwrite.h"

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
        AVStream *stream, AVCodecContext *ctx, int mode)
{
    int standard = par->color_space == AVCOL_SPC_BT2020_NCL ? 6
            : par->color_space == AVCOL_SPC_BT709 ? 1
            : par->color_space == AVCOL_SPC_BT470BG ? 2
            : par->color_space == AVCOL_SPC_SMPTE170M ? 4 : 0;
    int transfer = par->color_trc == AVCOL_TRC_SMPTE2084 ? 6
            : par->color_trc == AVCOL_TRC_ARIB_STD_B67 ? 7
            : par->color_trc == AVCOL_TRC_LINEAR ? 1
            : par->color_trc == AVCOL_TRC_BT709 || par->color_trc == AVCOL_TRC_SMPTE170M
                || par->color_trc == AVCOL_TRC_BT2020_10 || par->color_trc == AVCOL_TRC_BT2020_12 ? 3 : 0;
    SDL_AMediaFormat_setInt32(format,"ntv-hdr-mode",mode);
    SDL_AMediaFormat_setInt32(format,"ntv-source-standard",standard);
    SDL_AMediaFormat_setInt32(format,"ntv-source-transfer",transfer);
    if (mode == 2) { standard = 1; transfer = 3; }
    if (standard) SDL_AMediaFormat_setInt32(format,"color-standard",standard);
    if (transfer) SDL_AMediaFormat_setInt32(format,"color-transfer",transfer);
    if (par->color_range != AVCOL_RANGE_UNSPECIFIED)
        SDL_AMediaFormat_setInt32(format,"color-range",par->color_range == AVCOL_RANGE_JPEG ? 1 : 2);
    if (par->codec_id == AV_CODEC_ID_HEVC && par->profile == FF_PROFILE_HEVC_MAIN_10)
        SDL_AMediaFormat_setInt32(format,"profile",2); /* MediaCodec HEVCProfileMain10 */
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
