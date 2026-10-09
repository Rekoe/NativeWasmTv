/* LGPL-2.1-or-later. Bounded reserve for finite HLS, measured in media time. */
#ifndef NTV_VOD_BUFFER_H
#define NTV_VOD_BUFFER_H
#include <stdint.h>

static int ntv_vod_reserve_ms(int mode, int64_t cached_ms, int64_t elapsed_ms,
                              int64_t remaining_ms)
{
    if (mode <= 0 || remaining_ms <= 0) return 0;
    int base = mode == 2 ? 5000 : 2000;
    if (remaining_ms < base) base = (int)remaining_ms;
    // Wait for at least one representative fragment, not the first few packets.
    if (cached_ms < 5000 || elapsed_ms < 1000) return base;
    // Bound before multiplying: a very long clip must not overflow int64_t.
    int64_t deficit = cached_ms >= elapsed_ms ? 0
        : (int64_t)((double)remaining_ms * (1.0 - (double)cached_ms / elapsed_ms));
    int64_t reserve = deficit + base;
    if (reserve < base) reserve = base;
    if (reserve > 60000) reserve = 60000;
    if (reserve > remaining_ms) reserve = remaining_ms;
    return (int)reserve;
}
#endif
