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
    edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    int ntv_hdr_mode;',
            '    uint32_t ntv_video_description; /* atomic source snapshot, property 22010 */\n    int ntv_hdr_mode;');
    edit('ijkmedia/ijkplayer/ff_ffplay_def.h','    ffp->ntv_hdr_mode = 1;',
            '    __atomic_store_n(&ffp->ntv_video_description, 0, __ATOMIC_RELAXED);\n    ffp->ntv_hdr_mode = 1;');
    const play = 'ijkmedia/ijkplayer/ff_ffplay.c';
    edit(play,'        is->video_st = NULL;',
            '        __atomic_store_n(&ffp->ntv_video_description, 0, __ATOMIC_RELAXED);\n        is->video_st = NULL;');
    edit(play,'        is->video_st = ic->streams[stream_index];',
            '        is->video_st = ic->streams[stream_index];\n        __atomic_store_n(&ffp->ntv_video_description, ntv_describe_video(is->video_st, avctx, NULL), __ATOMIC_RELAXED);');
    edit(play,'    int64_t video_seek_pos = 0;\n    int64_t now = 0;\n    int64_t deviation = 0;\n\n    int64_t deviation2 = 0;\n    int64_t deviation3 = 0;',
            '    int64_t video_seek_pos = 0;\n    int64_t now = 0;\n    int64_t deviation = 0;\n\n    int64_t deviation2 = 0;\n    int64_t deviation3 = 0;\n    if (ffp->stat.vdec_type == FFP_PROPV_DECODER_AVCODEC && is->video_st)\n        __atomic_store_n(&ffp->ntv_video_description, ntv_describe_video(is->video_st, is->viddec.avctx, src_frame) | NTV_DESCRIPTION_FRAME, __ATOMIC_RELAXED);');
    edit(play,'        case 22001: /* nTv: successful submissions of distinct video frames */',
            '        case 22010: /* source colour/depth, before any output conversion */\n            return ffp ? __atomic_load_n(&ffp->ntv_video_description, __ATOMIC_RELAXED) : default_value;\n        case 22001: /* nTv: successful submissions of distinct video frames */');
};
