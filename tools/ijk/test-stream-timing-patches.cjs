const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const apply = require('./apply-stream-timing-patches.cjs');
const root = fs.mkdtempSync(path.join(os.tmpdir(), 'ntv-source-hook-'));
const dir = path.join(root, 'ijkmedia/ijkplayer');
fs.mkdirSync(dir, {recursive:true});
const file = path.join(dir, 'ff_ffplay.c');
const branch = `        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range
                   && !(is->video_st && (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))) {
`;
try {
    fs.writeFileSync(file, branch + '            packet_queue_put(&is->videoq, pkt);\n        }\n');
    apply(root);
    const once = fs.readFileSync(file, 'utf8');
    apply(root); apply(root);
    assert.strictEqual(fs.readFileSync(file, 'utf8'), once);
    const hook = once.slice(branch.length, once.indexOf('            packet_queue_put'));
    fs.writeFileSync(file, branch + hook.repeat(6) + '            packet_queue_put(&is->videoq, pkt);\n        }\n');
    apply(root);
    assert.strictEqual(fs.readFileSync(file, 'utf8'), once);
    fs.writeFileSync(file, branch + '            unexpected_call();\n            packet_queue_put(&is->videoq, pkt);\n        }\n');
    assert.throws(()=>apply(root), /Unknown code/);
    console.log('PASS: fresh, repeated, sixfold legacy hook migration, unknown code rejection');
} finally { fs.rmSync(root, {recursive:true, force:true}); }
