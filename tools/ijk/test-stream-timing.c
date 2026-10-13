/* Host-side regression for counters shared with the shipped native player. */
#include "ntv_stream_timing.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    NtvStreamTiming s = {0};
    /* HLS arrival bursts do not affect stream rate. 1080i50 has 25 AUs/s. */
    for (int i = 0; i <= 100; ++i) ntv_source_frame(&s, 900.0 + i * .04, 1);
    assert(fabs(s.source_fps - 25) < .01);
    /* Reapplying old source overlays counted the same packet six times. */
    for (int i = 0; i <= 150; ++i)
        for (int duplicate = 0; duplicate < 6; ++duplicate)
            ntv_source_frame(&s, 1000.0 + i * .04, 10);
    assert(fabs(s.source_fps - 25) < .01);
    /* Real high-rate video remains high-rate; no display-only FPS cap. */
    for (int i = 0; i <= 900; ++i)
        ntv_source_frame(&s, 2000.0 + i / 150.0, 11);
    assert(fabs(s.source_fps - 150) < 1);
    for (int i = 0; i <= 200; ++i) ntv_source_frame(&s, 100.0 + i * .02, 2);
    assert(fabs(s.source_fps - 50) < .01);
    /* Reordered Matroska timestamps must not reset source measurement. */
    for (int i = 0; i < 150; i += 3) {
        ntv_source_frame(&s, 500 + (i + 2) * .04, 4);
        ntv_source_frame(&s, 500 + i * .04, 4);
        ntv_source_frame(&s, 500 + (i + 1) * .04, 4);
        ntv_source_frame(&s, 500 + (i + 2) * .04, 4); /* reordered duplicate */
    }
    assert(fabs(s.source_fps - 25) < .01);
    ntv_source_frame(&s, 200, 4); /* true discontinuity, same serial */
    assert(s.source_fps == 0);
    for (int i = 1; i <= 150; ++i) ntv_source_frame(&s, 200 + i * .02, 4);
    assert(fabs(s.source_fps - 50) < .01);
    ntv_source_frame(&s, NAN, 2);
    assert(fabs(s.source_fps - 50) < .01);
    /* Seeking/switching resets the measurement, never averages a PTS jump. */
    ntv_source_frame(&s, 1.0, 3);
    assert(s.source_fps == 0);
    for (int i = 1; i <= 60; ++i) ntv_source_frame(&s, 1 + i / 30.0, 3);
    assert(fabs(s.source_fps - 30) < .01);

    for (int i = 0; i < 200; ++i) {
        ntv_output_frame(&s, 1000 + i * 20, i * .02, 1);
        ntv_output_frame(&s, 1001 + i * 20, i * .02, 1); /* retained repaint */
    }
    assert(s.output_total == 200);
    assert(fabs(ntv_output_fps(&s, 4980) - 50) < .01);
    assert(ntv_output_fps(&s, 5250) == 0); /* stopped/buffering */
    for (int i = 0; i < 100; ++i) ntv_output_frame(&s, 6000 + i * 40, i * .04, 2);
    assert(fabs(ntv_output_fps(&s, 9960) - 25) < .5);
    NtvStreamTiming sparse = {0};
    sparse.source_fps = 1;
    for (int i = 0; i < 6; ++i) ntv_output_frame(&sparse, 1000 + i * 1000, i, 1);
    assert(ntv_output_fps(&sparse, 6500) > .5);
    assert(ntv_output_fps(&sparse, 9100) == 0);
    puts("stream timing: 25/30/50/150 fps, duplicate packets, discontinuity, repaint, stall PASS");
    return 0;
}
