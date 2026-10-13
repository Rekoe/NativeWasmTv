const fs = require('fs');
const path = require('path');

// Later overlays changed the text following this hook and invalidated the old
// substring idempotency check. Replace the entire bounded instrumentation prefix.
function apply(root) {
    const file = path.join(root, 'ijkmedia/ijkplayer/ff_ffplay.c');
    const source = fs.readFileSync(file, 'utf8').replace(/\r\n/g, '\n');
    const branch = '        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range';
    const begin = source.indexOf(branch);
    const body = source.indexOf('\n', source.indexOf('AV_DISPOSITION_ATTACHED_PIC))) {', begin)) + 1;
    const queue = '            packet_queue_put(&is->videoq, pkt);';
    const end = source.indexOf(queue, body);
    if (begin < 0 || body <= begin || end < body || end - body > 20000)
        throw Error('Unexpected video packet queue hook');
    // Only migrate known instrumentation, never erase arbitrary upstream code.
    const prefix = source.slice(body, end);
    const latency = /            if \(ffp->ntv_live_video && pkt->pts != AV_NOPTS_VALUE\) \{\n(?:                .*\n)+?            \}\n/g;
    const remaining = prefix.replace(latency, '')
        .replace(/^            ntv_measure_source\(ffp, is->video_st, pkt, is->videoq.serial\);\n/gm, '')
        .replace(/^            if \(ffp->ntv_trace_latency\) av_log\(.*\n/gm, '');
    if (remaining.trim()) throw Error('Unknown code in video packet instrumentation prefix');
    const canonical = `            if (ffp->ntv_trace_latency) av_log(ffp, AV_LOG_INFO, "NTV_LAT READ pts=%.6f us=%lld\\n", pkt->pts * av_q2d(is->video_st->time_base), (long long)av_gettime_relative());
            ntv_measure_source(ffp, is->video_st, pkt, is->videoq.serial);
            if (ffp->ntv_live_video && pkt->pts != AV_NOPTS_VALUE) {
                SDL_LockMutex(is->videoq.mutex);
                int slot = ffp->ntv_latency_cursor;
                ffp->ntv_latency_cursor = (slot + 1) & 255;
                ffp->ntv_latency_pts[slot] = pkt->pts * av_q2d(is->video_st->time_base);
                ffp->ntv_latency_read_us[slot] = av_gettime_relative();
                ffp->ntv_latency_serial[slot] = is->videoq.serial;
                SDL_UnlockMutex(is->videoq.mutex);
            }
`;
    const updated = source.slice(0, body) + canonical + source.slice(end);
    if (updated !== source) fs.writeFileSync(file, updated);
}
module.exports = apply;
if (require.main === module) apply(path.resolve(process.argv[2] || '.codex-tmp/ijkplayer-0.8.8'));
