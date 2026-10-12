/* LGPL-2.1-or-later. Bounded HEVC SPS colour/depth probe; no pixel decoding. */
#ifndef NTV_HEVC_COLOR_H
#define NTV_HEVC_COLOR_H
#include <stdint.h>
#include <string.h>

typedef struct NtvHevcColor {
    int depth, primaries, transfer, matrix, range, profile, preferred_transfer;
} NtvHevcColor;
static int ntv_hevc_valid_transfer(int value)
{
    return value == 1 || (value >= 4 && value <= 18);
}
static int ntv_hevc_frame_transfer(int decoded, int sps)
{
    /* Reserved/unspecified Alternative Transfer SEI must not erase valid VUI.
     * Preserve valid decoded overrides, including SDR and PQ/HLG changes. */
    return !ntv_hevc_valid_transfer(decoded) && ntv_hevc_valid_transfer(sps) ? sps : decoded;
}
static int ntv_hevc_stream_transfer(int decoded, const NtvHevcColor *signal)
{
    if (signal->preferred_transfer) return signal->preferred_transfer;
    /* HLG-compatible broadcasts signal BT.2020 SDR in VUI and HLG/PQ via
     * Alternative Transfer SEI. A decoded/container HDR result preserves that
     * valid preference even when the prefix SEI is absent from extradata. */
    if ((signal->transfer == 14 || signal->transfer == 15) && (decoded == 16 || decoded == 18)) return decoded;
    return signal->transfer != 2 ? signal->transfer : decoded;
}
typedef struct NtvHevcBits {
    const uint8_t *data;
    unsigned size, pos;
    int error;
} NtvHevcBits;
static unsigned ntv_hevc_bits(NtvHevcBits *b, unsigned count)
{
    unsigned value = 0;
    if (count > 32 || b->pos > b->size || count > b->size - b->pos) {
        b->error = 1;
        return 0;
    }
    while (count--) {
        value = (value << 1) | ((b->data[b->pos >> 3] >> (7 - (b->pos & 7))) & 1);
        ++b->pos;
    }
    return value;
}
static void ntv_hevc_skip(NtvHevcBits *b, unsigned count)
{
    while (count > 32 && !b->error) { ntv_hevc_bits(b, 32); count -= 32; }
    ntv_hevc_bits(b, count);
}
static unsigned ntv_hevc_ue(NtvHevcBits *b)
{
    unsigned zeros = 0;
    while (!b->error && !ntv_hevc_bits(b, 1)) {
        if (++zeros > 30) { b->error = 1; return 0; }
    }
    return b->error ? 0 : ((1U << zeros) - 1) + ntv_hevc_bits(b, zeros);
}
static int ntv_hevc_sps_color(const uint8_t *nal, unsigned size, NtvHevcColor *out)
{
    if (size < 3 || size > 4096 || (nal[0] & 0x80) || (nal[0] >> 1) != 33
            || !(nal[1] & 7) || ((nal[0] & 1) << 5 | nal[1] >> 3)) return 0;
    uint8_t rbsp[4096];
    unsigned bytes = 0, zeros = 0;
    for (unsigned i = 2; i < size; ++i) {
        if (zeros == 2 && nal[i] == 3) {
            if (i + 1 == size || nal[i + 1] > 3) return 0;
            zeros = 0;
            continue;
        }
        rbsp[bytes++] = nal[i];
        zeros = nal[i] == 0 ? zeros + 1 : 0;
    }
    NtvHevcBits b = { rbsp, bytes * 8, 0, 0 };
    NtvHevcColor value = { 0, 2, 2, 2, 0, 0, 0 };
    ntv_hevc_skip(&b, 4);
    unsigned layers = ntv_hevc_bits(&b, 3);
    if (layers > 6) return 0;
    ntv_hevc_skip(&b, 1 + 3); /* nesting, profile space, tier */
    value.profile = ntv_hevc_bits(&b, 5);
    ntv_hevc_skip(&b, 88); /* compatibility, constraints, level */
    unsigned profiles[7] = {0}, levels[7] = {0};
    for (unsigned i = 0; i < layers; ++i) {
        profiles[i] = ntv_hevc_bits(&b, 1); levels[i] = ntv_hevc_bits(&b, 1);
    }
    if (layers) ntv_hevc_skip(&b, (8 - layers) * 2);
    for (unsigned i = 0; i < layers; ++i) {
        if (profiles[i]) ntv_hevc_skip(&b, 88);
        if (levels[i]) ntv_hevc_skip(&b, 8);
    }
    if (ntv_hevc_ue(&b) > 15) return 0;
    unsigned chroma = ntv_hevc_ue(&b);
    if (chroma > 3) return 0;
    if (chroma == 3) ntv_hevc_skip(&b, 1);
    unsigned width = ntv_hevc_ue(&b), height = ntv_hevc_ue(&b);
    if (!width || !height || width > 65535 || height > 65535) return 0;
    if (ntv_hevc_bits(&b, 1)) for (int i = 0; i < 4; ++i) ntv_hevc_ue(&b);
    unsigned depth = ntv_hevc_ue(&b), chroma_depth = ntv_hevc_ue(&b);
    if (depth > 8 || chroma_depth > 8 || (chroma && depth != chroma_depth)) return 0;
    value.depth = depth + 8;
    unsigned poc = ntv_hevc_ue(&b);
    if (poc > 12) return 0;
    poc += 4;
    unsigned first = ntv_hevc_bits(&b, 1) ? 0 : layers;
    for (unsigned i = first; i <= layers; ++i)
        for (int j = 0; j < 3; ++j) ntv_hevc_ue(&b);
    for (int i = 0; i < 6; ++i) ntv_hevc_ue(&b);
    if (ntv_hevc_bits(&b, 1) && ntv_hevc_bits(&b, 1)) {
        for (int size_id = 0; size_id < 4; ++size_id)
            for (int matrix = 0; matrix < 6; matrix += size_id == 3 ? 3 : 1) {
                if (!ntv_hevc_bits(&b, 1)) ntv_hevc_ue(&b);
                else {
                    if (size_id > 1) ntv_hevc_ue(&b); /* signed Golomb: same bit length */
                    int count = size_id == 0 ? 16 : 64;
                    for (int i = 0; i < count; ++i) ntv_hevc_ue(&b);
                }
            }
    }
    ntv_hevc_skip(&b, 2); /* AMP, SAO */
    if (ntv_hevc_bits(&b, 1)) {
        ntv_hevc_skip(&b, 8); ntv_hevc_ue(&b); ntv_hevc_ue(&b); ntv_hevc_skip(&b, 1);
    }
    unsigned sets = ntv_hevc_ue(&b), deltas[64] = {0};
    if (sets > 64) return 0;
    for (unsigned i = 0; i < sets && !b.error; ++i) {
        if (i && ntv_hevc_bits(&b, 1)) {
            ntv_hevc_skip(&b, 1); ntv_hevc_ue(&b);
            for (unsigned j = 0; j <= deltas[i - 1]; ++j) {
                unsigned used = ntv_hevc_bits(&b, 1);
                if (used || ntv_hevc_bits(&b, 1)) ++deltas[i];
            }
        } else {
            unsigned negative = ntv_hevc_ue(&b), positive = ntv_hevc_ue(&b);
            if (negative > 32 || positive > 32 || negative + positive > 32) return 0;
            deltas[i] = negative + positive;
            for (unsigned j = 0; j < deltas[i]; ++j) { ntv_hevc_ue(&b); ntv_hevc_skip(&b, 1); }
        }
        if (deltas[i] > 32) return 0;
    }
    if (ntv_hevc_bits(&b, 1)) {
        unsigned count = ntv_hevc_ue(&b);
        if (count > 32) return 0;
        ntv_hevc_skip(&b, count * (poc + 1));
    }
    ntv_hevc_skip(&b, 2); /* temporal MVP, strong intra smoothing */
    if (ntv_hevc_bits(&b, 1)) {
        if (ntv_hevc_bits(&b, 1) && ntv_hevc_bits(&b, 8) == 255) ntv_hevc_skip(&b, 32);
        if (ntv_hevc_bits(&b, 1)) ntv_hevc_skip(&b, 1);
        if (ntv_hevc_bits(&b, 1)) {
            ntv_hevc_skip(&b, 3);
            value.range = ntv_hevc_bits(&b, 1) ? 2 : 1; /* FFmpeg JPEG/MPEG */
            if (ntv_hevc_bits(&b, 1)) {
                value.primaries = ntv_hevc_bits(&b, 8);
                value.transfer = ntv_hevc_bits(&b, 8);
                value.matrix = ntv_hevc_bits(&b, 8);
            }
        }
    }
    if (b.error) return 0;
    *out = value;
    return 1;
}
static int ntv_hevc_sei_transfer(const uint8_t *nal, unsigned size)
{
    if (size < 4 || size > 4096 || (nal[0] & 0x80) || (nal[0] >> 1) != 39
            || !(nal[1] & 7) || ((nal[0] & 1) << 5 | nal[1] >> 3)) return 0;
    uint8_t rbsp[4096]; unsigned bytes = 0, zeros = 0;
    for (unsigned i = 2; i < size; ++i) {
        if (zeros == 2 && nal[i] == 3) {
            if (i + 1 == size || nal[i + 1] > 3) return 0;
            zeros = 0; continue;
        }
        rbsp[bytes++] = nal[i]; zeros = nal[i] == 0 ? zeros + 1 : 0;
    }
    if (!bytes || rbsp[bytes - 1] != 0x80) return 0;
    unsigned pos = 0; int preferred = 0;
    while (pos < bytes) {
        if (rbsp[pos] == 0x80 && pos + 1 == bytes) break;
        unsigned type = 0, payload = 0, value;
        do { if (pos >= bytes || type > 65535) return 0; value = rbsp[pos++]; type += value; } while (value == 255);
        do { if (pos >= bytes || payload > 4096) return 0; value = rbsp[pos++]; payload += value; } while (value == 255);
        if (payload > bytes - pos) return 0;
        if (type == 147 && payload == 1 && ntv_hevc_valid_transfer(rbsp[pos])) preferred = rbsp[pos];
        pos += payload;
    }
    return preferred;
}
static int ntv_hevc_annexb_color(const uint8_t *data, unsigned size, NtvHevcColor *out)
{
    NtvHevcColor color = {0,2,2,2,0,0,0}; int found = 0, preferred = 0;
    for (unsigned i = 0; i + 5 < size; ++i) {
        if (data[i] || data[i + 1] || data[i + 2] != 1) continue;
        unsigned begin = i + 3, type = (data[begin] >> 1) & 63;
        if (type < 32) break; /* Do not scan compressed pixels. */
        unsigned end = begin + 2;
        while (end + 2 < size && !(data[end] == 0 && data[end + 1] == 0 && data[end + 2] == 1)) ++end;
        if (end + 2 >= size) end = size;
        unsigned nal_end = end;
        /* A four-byte next start code leaves a trailing zero outside RBSP. */
        while (nal_end > begin + 2 && data[nal_end-1] == 0) --nal_end;
        if (type == 33 && ntv_hevc_sps_color(data + begin, nal_end - begin, &color)) found = 1;
        if (type == 39) {
            int transfer = ntv_hevc_sei_transfer(data + begin, nal_end - begin);
            if (transfer) preferred = transfer;
        }
        i = end - 1;
    }
    if (!found && !preferred) return 0;
    if (preferred) color.transfer = color.preferred_transfer = preferred;
    *out = color; return 1;
}
static int ntv_hevc_extradata_color(const uint8_t *data, unsigned size, NtvHevcColor *out)
{
    if (!data || size < 3) return 0;
    if (data[0] != 1) return ntv_hevc_annexb_color(data, size, out);
    if (size < 23) return 0; /* hvcC arrays */
    NtvHevcColor color = {0,2,2,2,0,0,0}; int found = 0, preferred = 0;
    unsigned pos = 23;
    for (unsigned i = 0; i < data[22]; ++i) {
        if (size - pos < 3) return 0;
        unsigned type = data[pos++] & 63;
        unsigned count = (data[pos] << 8) | data[pos + 1]; pos += 2;
        for (unsigned j = 0; j < count; ++j) {
            if (size - pos < 2) return 0;
            unsigned length = (data[pos] << 8) | data[pos + 1]; pos += 2;
            if (length > size - pos) return 0;
            if (type == 33 && ntv_hevc_sps_color(data + pos, length, &color)) found = 1;
            if (type == 39) {
                int transfer = ntv_hevc_sei_transfer(data + pos, length);
                if (transfer) preferred = transfer;
            }
            pos += length;
        }
    }
    if (!found && !preferred) return 0;
    if (preferred) color.transfer = color.preferred_transfer = preferred;
    *out = color; return 1;
}
static int ntv_hevc_packet_color(const uint8_t *data, unsigned size,
        const uint8_t *extra, unsigned extra_size, NtvHevcColor *out)
{
    if (!data) return 0;
    if (!extra || extra_size < 23 || extra[0] != 1)
        return ntv_hevc_annexb_color(data,size,out);
    /* MP4/Matroska sample NAL lengths are defined by hvcC, not start codes. */
    unsigned length_size = (extra[21] & 3) + 1, pos = 0;
    NtvHevcColor color = {0,2,2,2,0,0,0}; int found = 0, preferred = 0;
    while (pos < size) {
        if (size - pos < length_size) return 0;
        unsigned length = 0;
        for (unsigned i = 0; i < length_size; ++i) length = (length << 8) | data[pos++];
        if (length < 2 || length > size - pos) return 0;
        unsigned type = (data[pos] >> 1) & 63;
        if (type < 32) break;
        if (type == 33 && ntv_hevc_sps_color(data+pos,length,&color)) found = 1;
        if (type == 39) {
            int transfer = ntv_hevc_sei_transfer(data+pos,length);
            if (transfer) preferred = transfer;
        }
        pos += length;
    }
    if (!found && !preferred) return 0;
    if (preferred) color.transfer = color.preferred_transfer = preferred;
    *out = color; return 1;
}
#endif
