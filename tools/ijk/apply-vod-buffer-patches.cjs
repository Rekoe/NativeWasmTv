// LGPL-2.1-or-later overlays for finite-HLS adaptive packet reserves.
const fs = require('fs'), path = require('path');
module.exports = function applyVodBuffer(root) {
  function edit(file, before, after) {
    const target=path.join(root,file), text=fs.readFileSync(target,'utf8').replace(/\r\n/g,'\n');
    if(text.includes(after))return;
    if(text.split(before).length!==2)throw new Error('Unexpected VOD buffering upstream: '+file);
    fs.writeFileSync(target,text.replace(before,after));
  }
  fs.copyFileSync(path.join(__dirname,'ntv_vod_buffer.h'),path.join(root,'ijkmedia/ijkplayer/ntv_vod_buffer.h'));
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    int ntv_frame_sync;',
    '    int ntv_vod_buffer_mode;\n    int ntv_vod_buffer_active;\n    int ntv_vod_buffer_last_target_ms;\n    int64_t ntv_vod_buffer_start_us;\n    int ntv_frame_sync;');
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    ffp->ntv_frame_sync = 0;',
    '    ffp->ntv_vod_buffer_mode = 0;\n    ffp->ntv_vod_buffer_active = 0;\n    ffp->ntv_vod_buffer_last_target_ms = 0;\n    ffp->ntv_vod_buffer_start_us = 0;\n    ffp->ntv_frame_sync = 0;');
  edit('ijkmedia/ijkplayer/ff_ffplay_options.h','    { "ntv-frame-sync",',
    '    { "ntv-vod-buffer-mode", "finite HLS reserve: off/balanced/stable",\n        OPTION_OFFSET(ntv_vod_buffer_mode), OPTION_INT(0, 0, 2) },\n    { "ntv-frame-sync",');
  const play='ijkmedia/ijkplayer/ff_ffplay.c';
  if (fs.readFileSync(path.join(root, play), 'utf8').includes('av_clip64(value, 0, 240000)'))
    edit(play, 'av_clip64(value, 0, 240000)', 'av_clip64(value, 0, 2147483000)');
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    int ntv_vod_buffer_mode;',
    '    int ntv_vod_proxy_wait_ms; /* atomic, initial proxy prefetch only */\n    int ntv_vod_buffer_mode;');
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    ffp->ntv_vod_buffer_mode = 0;',
    '    __atomic_store_n(&ffp->ntv_vod_proxy_wait_ms, 0, __ATOMIC_RELAXED);\n    ffp->ntv_vod_buffer_mode = 0;');
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    int ntv_vod_proxy_wait_ms;',
    '    int ntv_vod_proxy_ready; /* atomic, proxy has met its duration reserve */\n    int ntv_vod_proxy_wait_ms;');
  edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    __atomic_store_n(&ffp->ntv_vod_proxy_wait_ms, 0, __ATOMIC_RELAXED);',
    '    __atomic_store_n(&ffp->ntv_vod_proxy_ready, 0, __ATOMIC_RELAXED);\n    __atomic_store_n(&ffp->ntv_vod_proxy_wait_ms, 0, __ATOMIC_RELAXED);');
  edit(play, '        // case FFP_PROP_INT64_SELECTED_VIDEO_STREAM:',
    '        case 22040: /* Exclude prefetch already performed outside the demuxer. */\n'
    + '            if (ffp) __atomic_store_n(&ffp->ntv_vod_proxy_wait_ms, (int)av_clip64(value, 0, 2147483000), __ATOMIC_RELAXED);\n'
    + '            break;\n        // case FFP_PROP_INT64_SELECTED_VIDEO_STREAM:');
  const clearWait = '        __atomic_store_n(&ffp->ntv_vod_proxy_wait_ms, 0, __ATOMIC_RELAXED);';
  const clearReady = '        __atomic_store_n(&ffp->ntv_vod_proxy_ready, 0, __ATOMIC_RELAXED);';
  const clearBlock = '        is->buffering_on = 0;\n' + clearReady + '\n' + clearWait;
  const duplicateClear = '        is->buffering_on = 0;\n' + clearWait + '\n' + clearReady + '\n' + clearWait;
  if (fs.readFileSync(path.join(root, play), 'utf8').includes(duplicateClear)) edit(play, duplicateClear, clearBlock);
  const oldClear = '        is->buffering_on = 0;\n' + clearWait;
  if (fs.readFileSync(path.join(root, play), 'utf8').includes(oldClear)) edit(play, oldClear, clearBlock);
  edit(play, '        is->buffering_on = 0;', clearBlock);
  edit(play, '        case 22040:',
    '        case 22041:\n            if (ffp) __atomic_store_n(&ffp->ntv_vod_proxy_ready, value != 0, __ATOMIC_RELAXED);\n'
    + '            break;\n        case 22040:');
  edit(play,'#include "ntv_stream_probe.h"','#include "ntv_stream_probe.h"\n#include "ntv_vod_buffer.h"');
  edit(play,'    AVFormatContext *ic = NULL;',
    '    AVFormatContext *ic = NULL;\n    ffp->ntv_vod_buffer_start_us = av_gettime_relative();');
  edit(play,'    is->ic = ic;',`    is->ic = ic;
    ffp->ntv_vod_buffer_active = ffp->ntv_vod_buffer_mode > 0 && ic->duration > 0
        && ic->iformat && av_match_name("hls", ic->iformat->name);
    if (ffp->ntv_vod_buffer_active) {
        // Only finite HLS changes. Live/cast latency and small-device byte caps remain.
        ffp->dcc.min_frames = 10000;
        if (ffp->dcc.max_buffer_size >= 32 * 1024 * 1024)
            ffp->dcc.max_buffer_size = (ffp->ntv_vod_buffer_mode == 2 ? 96 : 64) * 1024 * 1024;
        av_log(ffp, AV_LOG_INFO, "NTV VOD reserve mode=%d byteLimit=%d\\n",
               ffp->ntv_vod_buffer_mode, ffp->dcc.max_buffer_size);
        // Initial queues may already contain probe packets and never run dry.
        // Enter buffering explicitly before the decoder/prepared event; include
        // opening/probing time in the initial download-speed measurement.
        int64_t reserve_start_us = ffp->ntv_vod_buffer_start_us;
        ffp_toggle_buffering(ffp, 1);
        ffp->ntv_vod_buffer_start_us = reserve_start_us;
    }`);
  edit(play,'        is->buffering_on = 1;\n        stream_update_pause_l(ffp);',
    '        is->buffering_on = 1;\n        ffp->ntv_vod_buffer_start_us = av_gettime_relative();\n        ffp->ntv_vod_buffer_last_target_ms = 0;\n        stream_update_pause_l(ffp);');
  edit(play,'    if (buffering_on && !is->buffering_on) {',
    '    if (ffp->ntv_vod_buffer_active && buffering_on && is->seek_req)\n        is->seek_buffering = 1;\n    if (buffering_on && !is->buffering_on) {');
  const oldCall = '                                              cached_duration_in_ms, elapsed_ms);';
  const durationCall = '                                              cached_duration_in_ms, elapsed_ms,\n'
    + '                                              FFMAX(0, is->ic->duration / 1000 - FFMAX(0, ffp_get_current_position_l(ffp))));';
  if (fs.readFileSync(path.join(root, play), 'utf8').includes(oldCall)) edit(play, oldCall, durationCall);
  const oldElapsed = '                int64_t elapsed_ms = (av_gettime_relative() - ffp->ntv_vod_buffer_start_us) / 1000;';
  const elapsedLine = '                int64_t elapsed_ms = FFMAX(0, (av_gettime_relative() - ffp->ntv_vod_buffer_start_us) / 1000\n'
    + '                    - __atomic_load_n(&ffp->ntv_vod_proxy_wait_ms, __ATOMIC_RELAXED));';
  const bufferingBlock = `            if (ffp->ntv_vod_buffer_active && is->buffering_on && !is->seek_buffering) {
${elapsedLine}
                hwm_in_ms = ntv_vod_reserve_ms(ffp->ntv_vod_buffer_mode,
${durationCall}
                if (abs(hwm_in_ms - ffp->ntv_vod_buffer_last_target_ms) >= 2000) {
                    ffp->ntv_vod_buffer_last_target_ms = hwm_in_ms;
                    av_log(ffp, AV_LOG_INFO, "NTV VOD reserve target=%d cached=%d elapsed=%lld\\n",
                           hwm_in_ms, cached_duration_in_ms, (long long)elapsed_ms);
                }
            }`;
  const legacyBlock = bufferingBlock.replace(elapsedLine, oldElapsed);
  let text = fs.readFileSync(path.join(root, play), 'utf8').replace(/\r\n/g, '\n');
  if (text.includes(bufferingBlock) && text.includes(legacyBlock)) {
    if (text.split(legacyBlock).length !== 2) throw new Error('Unexpected duplicate VOD overlay');
    fs.writeFileSync(path.join(root, play), text.replace(legacyBlock + '\n', ''));
  } else if (text.includes(legacyBlock)) edit(play, legacyBlock, bufferingBlock);
  edit(play, '        if (cached_duration_in_ms >= 0) {',
    '        if (cached_duration_in_ms >= 0) {\n' + bufferingBlock);
  edit(play, '            buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;',
    '            // The disk reserve already covers the download deficit. Only prime\n'
    + '            // the decoder here; genuine later rebuffering restores adaptive reserve.\n'
    + '            if (ffp->ntv_vod_buffer_active && is->buffering_on\n'
    + '                && __atomic_load_n(&ffp->ntv_vod_proxy_ready, __ATOMIC_RELAXED))\n'
    + '                hwm_in_ms = FFMIN(hwm_in_ms, 1000);\n'
    + '            hwm_in_ms = FFMAX(1, hwm_in_ms);\n'
    + '            buf_time_position = ffp_get_current_position_l(ffp) + cached_duration_in_ms;');
  const oldFull = '    if (ffp->ntv_vod_buffer_active && is->buffering_on && cached_duration_in_ms >= 1000';
  const queueFull = '    if (ffp->ntv_vod_buffer_active && is->buffering_on\n'
    + '        && buf_time_position - ffp_get_current_position_l(ffp) >= 1000';
  if (fs.readFileSync(path.join(root, play), 'utf8').includes(oldFull)) edit(play, oldFull, queueFull);
  edit(play, '    int buf_percent = -1;',
    '    // An in-flight read can block just below the byte ceiling. Do not wait\n'
    + '    // for a time reserve that cannot fit in a small-device packet queue.\n'
    + queueFull + '\n'
    + '        && cached_size >= (int64_t)ffp->dcc.max_buffer_size * 9 / 10)\n'
    + '        need_start_buffering = 1;\n    int buf_percent = -1;');
  edit(play, '        if (ffp->packet_buffering) {\n            io_tick_counter = SDL_GetTickHR();',
    '        if (ffp->packet_buffering) {\n'
    + '            // Disk packets can arrive within one polling interval, followed\n'
    + '            // by a blocking read for the next fragment. Release a satisfied\n'
    + '            // reserve before that read rather than waiting for the timer.\n'
    + '            if (ffp->ntv_vod_buffer_active && is->buffering_on\n'
    + '                && (__atomic_load_n(&ffp->ntv_vod_proxy_ready, __ATOMIC_RELAXED)\n'
    + '                    || (int64_t)is->audioq.size + is->videoq.size >= (int64_t)ffp->dcc.max_buffer_size * 9 / 10))\n'
    + '                ffp_check_buffering_l(ffp);\n'
    + '            io_tick_counter = SDL_GetTickHR();');
};
