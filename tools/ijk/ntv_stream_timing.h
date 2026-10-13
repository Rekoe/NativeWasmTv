/* LGPL-2.1-or-later. Frame cadence uses media timestamps; output uses wall time.
 * No packet-arrival rate, nominal SPS field rate, or decoder throughput is used. */
#ifndef NTV_STREAM_TIMING_H
#define NTV_STREAM_TIMING_H
#include <stdint.h>
#include <math.h>
#include <string.h>

typedef struct NtvStreamTiming {
    int source_serial, source_count;
    double source_first, source_last;
    int64_t source_slots[40];
    unsigned source_counts[40];
    double source_recent[256];
    unsigned source_recent_count, source_recent_next;
    volatile float source_fps;
    int64_t output_start_ms, output_last_ms;
    int64_t output_slots[20];
    unsigned output_counts[20];
    volatile int64_t output_total;
    int output_serial;
    double output_pts;
} NtvStreamTiming;

static inline int64_t ntv_output_stall_ms(const NtvStreamTiming *s)
{
    /* Sparse camera/video streams must survive their normal inter-frame gap. */
    float fps = s->source_fps > 0 ? s->source_fps : 1.5f;
    int64_t ms = 3000 / fps;
    return ms < 250 ? 250 : ms > 5000 ? 5000 : ms;
}

static inline void ntv_source_frame(NtvStreamTiming *s, double dts, int serial)
{
    if (!isfinite(dts)) return;
    if (!s->source_count || serial != s->source_serial
            || dts < s->source_last - 1.0 || dts > s->source_last + 1.0) {
        s->source_fps = 0;
        memset(s->source_counts, 0, sizeof(s->source_counts));
        memset(s->source_slots, 0, sizeof(s->source_slots));
        s->source_serial = serial;
        s->source_first = s->source_last = dts;
        s->source_count = 0;
        s->source_recent_count = s->source_recent_next = 0;
    }
    /* Repeated packets/samples do not represent new pictures. Keep recent
     * timestamps rather than just the last one, because B pictures reorder. */
    for (unsigned i = 0; i < s->source_recent_count; ++i)
        if (s->source_recent[i] == dts) return;
    s->source_recent[s->source_recent_next] = dts;
    s->source_recent_next = (s->source_recent_next + 1) & 255;
    if (s->source_recent_count < 256) ++s->source_recent_count;
    if (dts > s->source_last) s->source_last = dts;
    ++s->source_count;
    int64_t slot = (int64_t)floor(dts * 10.0 + 0.000001);
    int index = (int)((slot % 40 + 40) % 40);
    if (s->source_slots[index] != slot) {
        s->source_slots[index] = slot;
        s->source_counts[index] = 0;
    }
    ++s->source_counts[index];
    /* Container timestamps may be reordered by B pictures, even in DTS fields.
     * Count completed media-time bins, allowing one second for reordering.
     * Never use download arrival time or multiply encoded frames into fields. */
    int64_t end = (int64_t)floor(s->source_last * 10.0 + 0.000001) - 10;
    int64_t begin = (int64_t)ceil(s->source_first * 10.0 - 0.000001);
    if (begin < end - 20) begin = end - 20;
    if (end - begin < 10) return;
    unsigned count = 0;
    for (int i = 0; i < 40; ++i)
        if (s->source_slots[i] >= begin && s->source_slots[i] < end)
            count += s->source_counts[i];
    float fps = count * 10.0f / (end - begin);
    if (fps >= 1 && fps <= 240) s->source_fps = fps;
}

static inline void ntv_output_frame(NtvStreamTiming *s, int64_t ms, double pts, int serial)
{
    /* Expose/repaint of the retained picture is not a new video frame. */
    if (s->output_start_ms && serial == s->output_serial
            && isfinite(pts) && pts == s->output_pts) return;
    if (!s->output_start_ms || serial != s->output_serial
            || ms - s->output_last_ms > ntv_output_stall_ms(s)) {
        memset(s->output_counts, 0, sizeof(s->output_counts));
        memset(s->output_slots, 0, sizeof(s->output_slots));
        s->output_start_ms = ms;
    }
    s->output_serial = serial;
    s->output_pts = pts;
    s->output_last_ms = ms;
    ++s->output_total;
    int64_t slot = ms / 100;
    int index = slot % 20;
    if (s->output_slots[index] != slot) {
        s->output_slots[index] = slot;
        s->output_counts[index] = 0;
    }
    ++s->output_counts[index];
}

static inline float ntv_output_fps(const NtvStreamTiming *s, int64_t ms)
{
    if (!s->output_start_ms || ms - s->output_last_ms > ntv_output_stall_ms(s)) return 0;
    /* Completed 100 ms bins avoid rounding a partial bin into a fake peak. */
    int64_t end = ms / 100, begin = (s->output_start_ms + 99) / 100;
    if (begin < end - 19) begin = end - 19;
    if (end - begin < 10) return 0;
    unsigned count = 0;
    for (int i = 0; i < 20; ++i)
        if (s->output_slots[i] >= begin && s->output_slots[i] < end)
            count += s->output_counts[i];
    return count * 10.0f / (end - begin);
}
#endif
