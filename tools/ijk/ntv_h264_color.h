/* LGPL-2.1-or-later. Bounded AVC SPS/VUI probe, independent of Android APIs. */
#ifndef NTV_H264_COLOR_H
#define NTV_H264_COLOR_H
#include "ntv_hevc_color.h"

/* Reuse the bounded bit reader and common colour fields; never decode pixels. */
static int ntv_h264_se(NtvHevcBits *b)
{
    unsigned v = ntv_hevc_ue(b);
    return (v & 1) ? (int)((v + 1) / 2) : -(int)(v / 2);
}
static int ntv_h264_sps_color(const uint8_t *nal, unsigned size, NtvHevcColor *out)
{
    if (!nal || size < 5 || size > 4096 || (nal[0] & 0x80) || (nal[0] & 31) != 7) return 0;
    uint8_t rbsp[4096]; unsigned bytes = 0, zeros = 0;
    for (unsigned i = 1; i < size; ++i) {
        if (zeros == 2 && nal[i] == 3) {
            if (i + 1 == size || nal[i + 1] > 3) return 0;
            zeros = 0; continue;
        }
        rbsp[bytes++] = nal[i]; zeros = nal[i] == 0 ? zeros + 1 : 0;
    }
    NtvHevcBits b = {rbsp, bytes * 8, 0, 0};
    NtvHevcColor color = {8,2,2,2,1,0,0};
    color.profile = ntv_hevc_bits(&b,8);
    ntv_hevc_skip(&b,16);
    if (ntv_hevc_ue(&b) > 31) return 0;
    switch (color.profile) {
        case 100: case 110: case 122: case 244: case 44: case 83: case 86:
        case 118: case 128: case 138: case 139: case 134: case 135: {
            unsigned chroma = ntv_hevc_ue(&b);
            if (chroma > 3) return 0;
            if (chroma == 3) ntv_hevc_skip(&b,1);
            unsigned depth = ntv_hevc_ue(&b), chroma_depth = ntv_hevc_ue(&b);
            if (depth > 6 || chroma_depth > 6 || (chroma && depth != chroma_depth)) return 0;
            color.depth = depth + 8;
            ntv_hevc_skip(&b,1);
            if (ntv_hevc_bits(&b,1)) for (unsigned i = 0; i < (chroma == 3 ? 12U : 8U); ++i) {
                if (!ntv_hevc_bits(&b,1)) continue;
                int last = 8, next = 8;
                for (unsigned j = 0; j < (i < 6 ? 16U : 64U) && !b.error; ++j) {
                    if (next) {
                        int delta = ntv_h264_se(&b);
                        if (delta < -128 || delta > 127) return 0;
                        next = (last + delta + 256) & 255;
                    }
                    if (next) last = next;
                }
            }
            break;
        }
        case 66: case 77: case 88: break;
        default: return 0;
    }
    if (ntv_hevc_ue(&b) > 12) return 0;
    unsigned poc = ntv_hevc_ue(&b);
    if (poc == 0) {if (ntv_hevc_ue(&b) > 12) return 0;}
    else if (poc == 1) {
        ntv_hevc_skip(&b,1); ntv_h264_se(&b); ntv_h264_se(&b);
        unsigned count = ntv_hevc_ue(&b);
        if (count > 255) return 0;
        for (unsigned i = 0; i < count && !b.error; ++i) ntv_h264_se(&b);
    } else if (poc > 2) return 0;
    if (ntv_hevc_ue(&b) > 32) return 0;
    ntv_hevc_skip(&b,1);
    if (ntv_hevc_ue(&b) > 4095 || ntv_hevc_ue(&b) > 4095) return 0;
    if (!ntv_hevc_bits(&b,1)) ntv_hevc_skip(&b,1);
    ntv_hevc_skip(&b,1);
    if (ntv_hevc_bits(&b,1)) for (int i = 0; i < 4; ++i) ntv_hevc_ue(&b);
    if (ntv_hevc_bits(&b,1)) {
        if (ntv_hevc_bits(&b,1) && ntv_hevc_bits(&b,8) == 255) ntv_hevc_skip(&b,32);
        if (ntv_hevc_bits(&b,1)) ntv_hevc_skip(&b,1);
        if (ntv_hevc_bits(&b,1)) {
            ntv_hevc_skip(&b,3);
            color.range = ntv_hevc_bits(&b,1) ? 2 : 1;
            if (ntv_hevc_bits(&b,1)) {
                color.primaries = ntv_hevc_bits(&b,8);
                color.transfer = ntv_hevc_bits(&b,8);
                color.matrix = ntv_hevc_bits(&b,8);
            }
        }
    }
    if (b.error) return 0;
    *out = color; return 1;
}
static int ntv_h264_annexb_color(const uint8_t *data, unsigned size, NtvHevcColor *out)
{
    if (!data) return 0;
    for (unsigned i = 0; i + 4 < size; ++i) {
        if (data[i] || data[i+1] || data[i+2] != 1) continue;
        unsigned begin = i + 3, type = data[begin] & 31;
        if ((type >= 1 && type <= 5) || type == 19 || type == 20 || type == 21) break;
        unsigned end = begin + 1;
        while (end + 2 < size && (data[end] || data[end+1] || data[end+2] != 1)) ++end;
        if (end + 2 >= size) end = size;
        unsigned nal_end = end;
        while (nal_end > begin + 1 && !data[nal_end-1]) --nal_end;
        if (type == 7 && ntv_h264_sps_color(data+begin,nal_end-begin,out)) return 1;
        i = end - 1;
    }
    return 0;
}
static int ntv_h264_extradata_color(const uint8_t *data, unsigned size, NtvHevcColor *out)
{
    if (!data || size < 4) return 0;
    if (data[0] != 1) return ntv_h264_annexb_color(data,size,out);
    if (size < 7) return 0;
    unsigned pos = 6, count = data[5] & 31;
    NtvHevcColor color; int found = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (size - pos < 2) return 0;
        unsigned length = (data[pos] << 8) | data[pos+1]; pos += 2;
        if (length > size - pos) return 0;
        if (ntv_h264_sps_color(data+pos,length,&color)) found = 1;
        pos += length;
    }
    if (!found) return 0;
    *out = color; return 1;
}
static int ntv_h264_packet_color(const uint8_t *data, unsigned size,
        const uint8_t *extra, unsigned extra_size, NtvHevcColor *out)
{
    if (!data) return 0;
    if (!extra || extra_size < 7 || extra[0] != 1) return ntv_h264_annexb_color(data,size,out);
    unsigned n = (extra[4] & 3) + 1, pos = 0;
    while (pos < size) {
        if (size - pos < n) return 0;
        unsigned length = 0;
        for (unsigned i = 0; i < n; ++i) length = (length << 8) | data[pos++];
        if (!length || length > size - pos) return 0;
        unsigned type = data[pos] & 31;
        if ((type >= 1 && type <= 5) || type == 19 || type == 20 || type == 21) break;
        if (type == 7 && ntv_h264_sps_color(data+pos,length,out)) return 1;
        pos += length;
    }
    return 0;
}
#endif
