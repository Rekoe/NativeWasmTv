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
            || dts <= s->source_last || dts - s->source_last > 1.0) {
        if (serial != s->source_serial) s->source_fps = 0;
        s->source_serial = serial;
        s->source_first = s->source_last = dts;
        s->source_count = 1;
        return;
    }
    s->source_last = dts;
    ++s->source_count;
    double span = dts - s->source_first;
    if (span >= 1.0 && s->source_count >= 8) {
        double fps = (s->source_count - 1) / span;
        if (fps >= 1 && fps <= 240) s->source_fps = fps;
        if (span >= 2.0) {
            s->source_first = dts;
            s->source_count = 1;
        }
    }
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
