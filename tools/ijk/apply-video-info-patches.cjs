// LGPL-2.1-or-later. Cheap source colour snapshot for the nTv debug/card UI.
const fs = require('fs');
const path = require('path');
module.exports = function applyVideoInfo(root) {
    function edit(file, before, after) {
        const target = path.join(root,file);
        const text = fs.readFileSync(target,'utf8').replace(/\r\n/g,'\n');
        if (text.includes(after)) return;
        if (text.split(before).length !== 2) throw new Error('Unexpected video info upstream: '+file+' / '+before);
        fs.writeFileSync(target,text.replace(before,after));
    }
    fs.copyFileSync(path.join(__dirname,'ntv_video_description.h'),
            path.join(root,'ijkmedia/ijkplayer/ntv_video_description.h'));
    fs.copyFileSync(path.join(__dirname,'ntv_hevc_color.h'),
            path.join(root,'ijkmedia/ijkplayer/ntv_hevc_color.h'));
    fs.copyFileSync(path.join(__dirname,'ntv_h264_color.h'),
            path.join(root,'ijkmedia/ijkplayer/ntv_h264_color.h'));
    // Keep this field outside the playback overlay's contiguous timing block.
    // Migrate earlier checkouts that inserted it into that block.
    const defTarget = path.join(root,'ijkmedia/ijkplayer/ff_ffplay_def.h');
    const oldDef = fs.readFileSync(defTarget,'utf8').replace(/\r\n/g,'\n');
    let timingBlocks = 0;
    const migratedDef = oldDef.replace(
        /^    int ntv_source_(?:preferred_transfer|avc_header);[^\n]*\n/gm,'').replace(
        /    int ntv_frame_sync;\n    NtvStreamTiming ntv_timing;\n    AVCodecParserContext \*ntv_source_parser;\n    AVCodecContext \*ntv_source_context;\n    int ntv_source_parser_serial;\n/g,
        block => ++timingBlocks === 1 ? block : '');
    if (oldDef !== migratedDef) fs.writeFileSync(defTarget,migratedDef);
    edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    int ntv_hdr_mode;',
            '    uint32_t ntv_video_description; /* atomic source snapshot, property 22010 */\n    int ntv_hdr_mode;');
    edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    uint32_t ntv_video_description;',
            '    int ntv_source_preferred_transfer; /* valid Alternative Transfer SEI, per parser serial */\n    int ntv_source_avc_header; /* observed in-band SPS, per parser serial */\n    uint32_t ntv_video_description;');
    edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    ffp->ntv_hdr_mode = 1;',
            '    __atomic_store_n(&ffp->ntv_video_description, 0, __ATOMIC_RELAXED);\n    ffp->ntv_hdr_mode = 1;');
    const play = 'ijkmedia/ijkplayer/ff_ffplay.c';
    const playTarget = path.join(root,play);
    const oldPlay = fs.readFileSync(playTarget,'utf8');
    const migratedPlay = oldPlay.replace('ntv_hevc_probe_initial(ic,st_index[AVMEDIA_TYPE_VIDEO],',
            'ntv_hevc_probe_initial(ic,st_index,').replace(
            '    ntv_hevc_probe_initial(ic,st_index,&ntv_hevc_probe);',
            '    ffp->ntv_source_preferred_transfer = ntv_hevc_probe_initial(ic,st_index,&ntv_hevc_probe);');
    if (oldPlay !== migratedPlay) fs.writeFileSync(playTarget,migratedPlay);
    edit(play,'    int st_index[AVMEDIA_TYPE_NB];\n    AVPacket pkt1, *pkt = &pkt1;',
            '    int st_index[AVMEDIA_TYPE_NB];\n    AVPacket pkt1, *pkt = &pkt1;\n    NtvHevcProbePackets ntv_hevc_probe = {0};');
    edit(play,'    /* open the streams */',
            '    ffp->ntv_source_preferred_transfer = ntv_hevc_probe_initial(ic,st_index,&ntv_hevc_probe);\n    /* open the streams */');
    edit(play,'        ret = av_read_frame(ic, pkt);',
            '        ret = ntv_hevc_probe_read(ic,pkt,&ntv_hevc_probe);');
    edit(play,'        if (is->seek_req) {\n            int64_t seek_target',
            '        if (is->seek_req) {\n            ntv_hevc_probe_clear(&ntv_hevc_probe);\n            int64_t seek_target');
    edit(play,' fail:\n    if (ic && !is->ic)',
            ' fail:\n    ntv_hevc_probe_clear(&ntv_hevc_probe);\n    if (ic && !is->ic)');
    edit(play,'        is->video_st = NULL;',
            '        __atomic_store_n(&ffp->ntv_video_description, 0, __ATOMIC_RELAXED);\n        is->video_st = NULL;');
    edit(play,'        is->video_st = ic->streams[stream_index];',
            '        is->video_st = ic->streams[stream_index];\n        __atomic_store_n(&ffp->ntv_video_description, ntv_describe_video(is->video_st, avctx, NULL), __ATOMIC_RELAXED);');
    if (!fs.readFileSync(playTarget,'utf8').includes('/* Software source metadata is published before deinterlacing. */'))
    edit(play,'    int64_t video_seek_pos = 0;\n    int64_t now = 0;\n    int64_t deviation = 0;\n\n    int64_t deviation2 = 0;\n    int64_t deviation3 = 0;',
            '    int64_t video_seek_pos = 0;\n    int64_t now = 0;\n    int64_t deviation = 0;\n\n    int64_t deviation2 = 0;\n    int64_t deviation3 = 0;\n    if (ffp->stat.vdec_type == FFP_PROPV_DECODER_AVCODEC && is->video_st)\n        __atomic_store_n(&ffp->ntv_video_description, ntv_describe_video(is->video_st, is->viddec.avctx, src_frame) | NTV_DESCRIPTION_FRAME, __ATOMIC_RELAXED);');
    edit(play,'        case 22001: /* nTv: successful submissions of distinct video frames */',
            '        case 22010: /* source colour/depth, before any output conversion */\n            return ffp ? __atomic_load_n(&ffp->ntv_video_description, __ATOMIC_RELAXED) : default_value;\n        case 22001: /* nTv: successful submissions of distinct video frames */');
};
