#include <assert.h>
#include <stdio.h>
#include "ntv_vod_buffer.h"
int main(void) {
    assert(ntv_vod_reserve_ms(0, 10000, 12000, 150000) == 0);
    assert(ntv_vod_reserve_ms(2, 100, 5000, 20000) == 5000);
    assert(ntv_vod_reserve_ms(2, 10000, 12000, 20000) == 8333);
    assert(ntv_vod_reserve_ms(1, 10000, 12000, 20000) == 5333);
    assert(ntv_vod_reserve_ms(2, 30000, 5000, 150000) == 5000);
    assert(ntv_vod_reserve_ms(2, 5000, 100000, 150000) == 60000);
    assert(ntv_vod_reserve_ms(2, 10000, 12000, 1000) == 1000);
    assert(ntv_vod_reserve_ms(2, 10000, 12000, 0) == 0);
    // Five-second fragments downloaded in six seconds: reserve follows the clip,
    // then shrinks as playback approaches the end instead of targeting two minutes.
    int reserve = ntv_vod_reserve_ms(2, 25000, 30000, 150000);
    assert(reserve == 29999 || reserve == 30000);
    assert(ntv_vod_reserve_ms(2, 25000, 30000, 30000) <= 10000);
    puts("VOD reserve policy passed");
    return 0;
}
