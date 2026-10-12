# HDR output validation — updated 2026-10-10

## Live broadcast colour correction and full playlist audit

The current LAN sources became reachable and were captured on 2026-10-09/10.
`E:/Downloads/playlist.m3u` contains 210 channels. All entries were attempted;
failed entries were retried, including a sequential pass. Final source results:
167 captured/decoded, 43 still returning HTTP 503 from the supplied server. The
original playlist was not modified. Source availability is a point-in-time result.

Import validation found a second compatibility bug: the file reuses `tvg-id`
across CDTV1..6 and large channel families. EPG-only merging collapsed 210 entries
into 150 channel cards. The importer now merges by name/source identity and retains
the EPG ID for programme lookup. The actual file imports all 210 names/URLs;
control cases also preserve distinct channel/4K variants while merging same-channel
backup URLs and deduplicating identical URLs.

All 167 available samples passed actual Activity/IJK playback on MI 6 / Android
7.1: first frame, advancing video output/position, matching dimensions, an audio
decoder and a visible video-only capture. This includes all 11 named 4K channels,
ordinary SD/HD H.264, AAC, MP2 and MP3. Initial app failures for CCTV-1, Hebei and
Guizhou were retested: invalidate the asynchronous startup request, retain a
longer raw TS sample, and wait for a video cadence sample after audio advances.
Guangdong's raw TS had the same short cadence-sampling issue. These harness/sample
corrections are retained in the evidence; no playback failure was hidden.

The Sichuan/Hunan live HEVC sources are genuinely **8bit** BT.2020/HLG. HDR does
not imply Main10. On the reported grey-colour path, the old 1000-nit HLG + global
Reinhard/desaturation mapping raised zero colour components (a red reference
became [255,95,95], blue [69,69,255]). Production shader tests now preserve colour
ratios with the 291-nit/1.03 HLG reference viewing conditions and a common Mobius
RGB shoulder; those samples become [229,0,0] and [0,0,235]. Neutral wide-gamut SDR
uses matching BT.1886 display decoding/encoding, preventing a raised grey level.
This is an adaptation inspired by BT.2446 method B, not full ICTCP method B.

CCTV-16's SPS explicitly signals HLG (18), but a prefix Alternative Transfer SEI
contains reserved value 0. Reserved/unspecified SEI is ignored; invalid software
frame transfer is repaired from the valid SPS. Shenzhen 4K is different: SPS uses
SDR-compatible BT.2020-10 (14) and **valid** SEI signals HLG (18). Both Annex B
(including four-byte delimiters) and hvcC length-prefixed packets are parsed.
A bounded first-access-unit probe retains packets in order, and preserves the
validated SEI across FFmpeg codec opening so MediaCodec/GPU really use HLG.
Shenzhen raw TS and remuxed MKV both passed the GPU-transfer assertion, with
decoder output transfer=7 and HLG tone mapping; correcting the HUD alone did not
satisfy this test. Ordinary BT.2020 without valid HDR transfer remains unknown/SDR.

Shenzhen's additional continuous check passed after the 6.5-second warmup:
3,017 ms wall time, 3,018 ms position advance, 75 GPU swaps, zero buffering
transitions and 24.71–24.95 output fps for a 25 fps source. This confirms that
the batch's roughly 12.5 fps startup samples are not its settled playback rate.
Evidence: `.codex-tmp/playlist-audit/test-shenzhen-continuous.txt`.

Colour shader checks pass 45/45 on API19 and API25, including primary colour and
neutral/ramp checks. Native tests cover real SPS, valid/invalid SEI, four-byte
Annex B, 1–4-byte hvcC samples, truncation and 10,000 malformed inputs per fixture.
Android 4.4 software playback passed Sichuan HLG, CCTV-16 invalid-SEI HLG and
ordinary SD H.264/MP3. Real original Sichuan/Hunan/CCTV-16 4K samples were also
observed for 6.5 seconds on API25 at approximately 25 fps.

Both native ABIs and APKs build. Two overlay reapplications preserve identical
content across 5,687 C/header files. Evidence: `.codex-tmp/playlist-audit/audit.json`,
`app-playback-*.json`, `log-shenzhen-final-native.txt`, `log-mkv-native.txt`,
`test-cctv16-preserve32.txt`; colour evidence under `.codex-tmp/hdr/test-gray-*`.
The per-channel Chinese report is `.codex-tmp/playlist-audit/report.md`.

These are bounded recorded-sample playback checks, not long-term live monitoring.
No Amlogic HDR TV is attached, so its physical HDR/HDMI quality is not established.
The 43 server failures cannot be repaired by colour/decoder changes in this app.
Earlier sections below describe earlier fixtures and access limitations.

## HEVC signalling recovery and BT.2020 without transfer metadata

The three-line source/debug display and unknown HDR/SDR card label were restored;
the channel startup-time field remains. HEVC SPS probing now supplies source depth,
profile, primaries, matrix, range and transfer for Annex B and hvcC signalling before
hardware configuration. An available SPS is preferred over incomplete container
parameters. The reader is bounded, parses no pixels, and rejects malformed/truncated
input without changing an output snapshot.

Wide-gamut SDR/unknown-transfer BT.2020 NCL is converted to BT.709 on the GPU,
independently of PQ/HLG luminance mapping. Unknown transfer uses the BT.1886 SDR
display curve as an output assumption, and remains unknown in the HUD. A decoder
reporting BT.709 SDR output bypasses this conversion. This does not establish the
missing transfer of an untagged live source.

Verified with production playback and visible captures:

| Device / fixture | Path | Result |
| --- | --- | --- |
| Android 7.1 MI 6 / 480×270 8bit HEVC BT.2020, unspecified transfer | hardware SDR output | GPU conversion active, about 23.9fps; HUD keeps unknown transfer/HDR status |
| Android 4.4 SM-G3608 / same fixture | software mapping | visible GPU YUV output, about 23.7fps; same source metadata |
| Android 7.1 MI 6 / 640×360 HLG HEVC, container transfer omitted | hardware mode on SDR display | 10bit / BT.2020 / HLG, mapped GPU output about 50.2fps |
| Android 4.4 SM-G3608 / 640×360 HLG HEVC | software mapping | 10bit / BT.2020 / HLG, visible output about 50fps |
| Android 7.1 MI 6 / BT.709 SDR H.264 | hardware mode | visible about 23.7fps, no GPU bridge allocated |

The original local CCTV 4K sample's SPS reports 10bit / BT.2020 / HLG. Native tests
also verify PQ, SDR, unspecified transfer, hvcC/Annex B agreement, truncation and
10,000 malformed inputs per fixture. The ARM32/ARM64 native libraries and debug
APKs build. Overlay reapplication was checked for identical C/header content.
Evidence is in `.codex-tmp/hdr/test-bt2020-*` and `bt2020-{unknown,hlg}-{4.4,7.1}.png`.
The first Android 7.1 attempt failed at local fixture HTTP serving; the repeated
host-served fixture and final HLG build tests returned `INSTRUMENTATION_CODE: -1`.

At that earlier test stage, `10.1.1.1:1111` was unreachable from this machine. The
lower-resolution fixture tests do not verify its current metadata, the photographed
Amlogic TV's output, or 4K GPU playback throughput.

The source is [Apple's advanced HLS example](https://developer.apple.com/streaming/examples/advanced-stream-dv-atmos.html). The mixed master is `https://devstreaming-cdn.apple.com/videos/streaming/examples/adv_dv_atmos/main.m3u8`. [The pinned HDR + AAC playlist](fixtures/hdr-1080p.m3u8) selects the 1080p PQ HEVC variant, with no adaptive SDR/Dolby Vision selection.

`ffprobe` on actual media frames confirmed HEVC Main10, 1920×1080, 24000/1001 fps, `yuv420p10le`, limited range, BT.2020 primaries/non-constant-luminance matrix, SMPTE ST2084 transfer. Frames carry mastering metadata (1000-nit mastering peak), MaxCLL 1375 / MaxFALL 188 and HDR10+ dynamic metadata. The implementation renders the HDR10 base/static metadata; dynamic HDR10+ scene grading is not implemented.

## Device results

All runs use the production `MainActivity.startIjkPlayer` path. `HdrPlaybackInstrumentation` waits for advancing playback, captures the actual SurfaceView composite at approximately 6.5 seconds, rejects an all-black output, and reports the decoder and frame rate. Screenshots were visually inspected after pulling them from the devices. The short remuxed samples isolate colour/output behaviour from CDN throughput. These are short playback checks, not long-running live-stream soak tests.

| Device | Fixture / mode | Actual output FPS | Result |
| --- | --- | ---: | --- |
| MI 6, Android 7.1.1 | Original 1080p HDR, Qualcomm HEVC -> GPU SDR | 23.72 | Visible tone-mapped image; 158 distinct GPU swaps |
| MI 6, Android 7.1.1 | Same HDR, FFmpeg software -> YUV GLES | 24.21 | Visible tone-mapped image |
| MI 6, Android 7.1.1 | Return to hardware after software | 24.03 | New output connects; 158 distinct GPU swaps |
| SM-G3608, Android 4.4.4 | Same scene resized to 480×270, HEVC 10-bit PQ, software | 24.21 | Visible tone-mapped image |
| MI 6, Android 7.1.1 | Original 1080p HDR + AAC, hardware | 23.62 | Playback advances with the supported audio track; 156 GPU swaps |
| MI 6, Android 7.1.1 | 480×270 SDR H.264 + AAC, hardware / software | 23.68 / 23.68 | SDR remains visible; no HDR bridge allocated |
| SM-G3608, Android 4.4.4 | Same SDR + AAC, software | 23.68 | SDR remains visible |

An initial software test exposed a real Surface ownership issue: the first RGB frame connected the CPU producer, while later HDR frames attempted an EGL connection. The first-frame overlay policy and bounded release handoff fix this; the final tests above include screenshot validation rather than treating a decoded-frame callback as proof of visible video. A release instrumentation build also confirmed that the JNI bridge methods survive shrinking.

Native ARM32/ARM64 IJK and SDL libraries compiled and linked with NDK r17c against the unchanged FFmpeg binaries. Both release APKs and corresponding instrumentation APKs built successfully. Applying the native overlay again produced identical hashes for all 244 IJK media C/header files. `git diff --check` passed.

Local evidence is under `.codex-tmp/hdr/`: `apple-probe.json`, `test-7.1.txt`, `test-4.4.txt`, `test-7.1-aac.txt`, `test-7.1-sdr.txt`, `test-4.4-sdr.txt`, `7.1-hardware.png`, `7.1-software.png`, `4.4-software.png`. The HDR snapshots were saved before the later SDR runs overwrote the device-side fixture image names.

Release packages: `.codex-tmp/hdr/nTv-hdr-fix.apk` (ARM32, SHA256 `433cad5ce0d1551d3a62cacab53daf436778e49decc71bedd0860bf1f57feab4`), `.codex-tmp/hdr/nTv64-hdr-fix.apk` (ARM64, SHA256 `62b82ee191e3ec3188e95da86eb3aa426c029eb7fbdc7d0c96bde69be868632b`). Temporary fixture servers and ADB reverse mappings were removed after testing.

Not device-verified: HDR-capable HDMI/display passthrough, HLG material, the API 31+ acknowledged codec tone-mapping path, API 14–16, and HDR 4K high-frame-rate playback. The low-resolution 4.4 result does not establish 4K software decoding capability. See [the output policy](README.md#hdr-colour-output) and Android's [HDR playback guidance](https://developer.android.com/media/grow/hdr-playback).

## Advanced HDR modes — subsequent validation

Advanced settings now offer **HDR 映射** (default), **HDR 硬解**, and **HDR BT.709**. `hdrMode` is validated, saved, returned in the scoped advanced state, and applied to each new IJK player. Every test below POSTs the real settings API while the fixture is playing, waits for a replacement player and visible advancing playback, and checks the stored preference and status response. Unsupported values are rejected without changing HDR mode. Test preferences and the temporary channel catalog are restored afterwards.

Testing found and fixed two additional problems:

1. A new HDR EGL producer could connect before the preceding direct codec had finished releasing, fail with `EGL_BAD_ALLOC`, and fall back to software. Replacement preparation now waits asynchronously for the preceding release, bounded to two seconds. The final HDR cycles keep Qualcomm hardware decoding when mapping is selected again.
2. On MI 6, setting MediaCodec's BT.709/SDR keys produced matching output-format values but still yielded a black system screenshot for a stream carrying PQ tags. BT.709 now uses a hardware-decoded GPU pass for streams originally tagged HDR or another colour standard, producing SDR RGB without HDR luminance mapping. This makes its visible output verifiable even on that vendor's old compositor. Correctly tagged BT.709 SDR remains direct. Software output explicitly uses the BT.709 matrix with mapping disabled.

| Device / fixture | Selected HDR modes | Actual FPS / evidence |
| --- | --- | --- |
| MI 6 / original 1080p PQ HEVC + AAC | mapping → hardware → BT.709 → mapping, hardware decoder | 24.03 / 24.21 / 24.00 / 23.86; 158 / 0 / 159 / 156 GPU swaps |
| MI 6 / same HDR | same cycle, software decoder | 23.68–24.21; all captures visible |
| Android 4.4.4 / 480×270 PQ HEVC | same cycle, software decoder | 23.68–24.21; all captures visible |
| MI 6 / normal 480×270 BT.709 H.264 + AAC | all three modes, hardware decoder | 23.68–24.21; all captures visible, no HDR bridge |
| Android 4.4.4 / same SDR | all three modes, software decoder | 23.68–24.21; all captures visible |
| MI 6 / SDR H.264 pixels deliberately mistagged BT.2020/PQ | mapping → BT.709 → mapping, hardware decoder | 23.99 / 23.95 / 23.90; BT.709 visible with 158 GPU swaps, output standard=1 / transfer=3 |
| Android 4.4.4 / same mistagged SDR | mapping → BT.709 → mapping, software decoder | approximately 24; visible BT.709 output |

The mistagged fixture changes signalling only, preserving the original SDR pixel samples and audio:

```text
ffmpeg -i sdr-regression.mp4 -c copy -bsf:v h264_metadata=colour_primaries=9:transfer_characteristics=16:matrix_coefficients=9 -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc -movflags +faststart bt709-mistagged.mp4
```

BT.709 and the ordinary SDR reference captures were visually compared. True HDR in BT.709 mode intentionally lacks tone mapping; that mode is for mislabelled SDR. Selecting the hardware HDR mode while using software decoding keeps the SDR mapping fallback, as stated in the settings help.

**HDR passthrough limitation:** the MI 6 decoder advances at 24.21 fps with original BT.2020/PQ metadata and no GPU bridge, but its native HDR Surface screenshot is black. This cannot distinguish an unsupported display/composition path from a capture limitation, and is not proof of correct physical HDR output. Only this passthrough capture check was explicitly waived with `allowNativeHdrCaptureBlack=true`; GPU and software captures still reject black output. There is no attached HDR-capable TV for physical passthrough validation. The other unverified API/content limits above still apply.

Final checks: both native ABIs and release APKs build; instrumentation APKs build; native overlays remain idempotent across 244 C/header files; advanced JavaScript syntax and `git diff --check` pass. Evidence under `.codex-tmp/hdr/` includes `test-modes-7.1.txt`, `test-modes-4.4.txt`, `test-modes-sdr-{7.1,4.4}.txt`, `test-modes-mistag-{7.1,4.4}.txt`, matching logs, `7.1-mode-mapping.png`, `7.1-mode-hdr-bt709.png`, `7.1-mode-mistag-bt709.png`, and `7.1-mode-sdr-bt709.png`.

Mode-enabled release packages:

* `.codex-tmp/hdr/nTv-hdr-modes.apk` — ARM32 (Android 4.4 device tested), SHA256 `d8c2b5a56049fd7b59c978e3886dbf16632fd6abfeecff0cc678ef8926b20e59`.
* `.codex-tmp/hdr/nTv64-hdr-modes.apk` — ARM64 (Android 7.1 device tested), SHA256 `7d3fc49c48bd2359d7221aa002849aea505644a240ee19d08b08c34d0ca20475`.

Temporary fixture servers and the test's TCP 8876 ADB reverse mapping were removed after validation. The existing scrcpy mapping was preserved.

## Default changed to hardware

At the user's request, HDR 硬解 is now the default for new installs and absent/invalid preferences. Existing valid mapping/hardware/BT.709 selections remain unchanged. The Java preference fallback, web selector/fallback, MediaFormat bridge fallback and native option/reset default were updated together. Both native ABIs and ARM32/ARM64 release APKs rebuilt successfully; native overlay idempotency, advanced JavaScript syntax and diff whitespace checks passed. Playback mode implementations are unchanged; the device results above remain the evidence for their behaviour.

Updated packages: `.codex-tmp/hdr/nTv-hdr-default-hardware.apk` and `.codex-tmp/hdr/nTv64-hdr-default-hardware.apk`.

## Source colour and decoder information

Debug information now contains source HDR/SDR, source component depth, colour
primaries/matrix, transfer function and the actual decoder implementation name.
The channel card adds the same HDR/SDR classification. Native property 22010 is
a single atomic snapshot; no frame extraction, readback or extra pixel decoding is
used for the HUD. Software frames update the snapshot before colour conversion.
Output mode changes cannot rewrite the source information, and missing metadata
is explicitly unknown. The debug overlay has three lines with matching card and
network-overlay spacing; Android 4.4's font padding is retained for complete glyphs.

Device checks exercise production playback and assert `/api/media` metadata,
rendered debug text, the channel-card label and IJK's actual decoder name. Captures
were inspected on the 800×480 Android 4.4 device and the 1920×1080 Android 7.1 UI.

| Device / stream | Decoder path and output modes | Verified source information |
| --- | --- | --- |
| MI 6 / original 1080p HDR10 HEVC + AAC | hardware + software; mapping and BT.709 | HDR / 10bit / BT.2020 / PQ; OMX.qcom.video.decoder.hevc or hevc |
| Android 4.4.4 / 480p HDR10 HEVC | software; mapping and BT.709 | HDR / 10bit / BT.2020 / PQ; hevc |
| Android 4.4.4 / same HEVC, hardware decoding requested | native fallback to software | H265(软解) and hevc; source remains HDR / 10bit / BT.2020 / PQ |
| MI 6 / BT.709 H.264 SDR | hardware + software; hardware and BT.709 | SDR / 8bit / BT.709; OMX.qcom.video.decoder.avc or h264 |
| Android 4.4.4 / same SDR | software; hardware and BT.709 | SDR / 8bit / BT.709; h264 |
| MI 6 / deliberately PQ/BT.2020-tagged 8bit H.264 | hardware + software; BT.709 | HDR signalling / **8bit** / BT.2020 / PQ; depth is independent of HDR signalling |

All runs above returned `INSTRUMENTATION_CODE: -1`. Test evidence is under
`.codex-tmp/hdr/test-info-*`; captures include `info-4.4-hdr.png`,
`info-4.4-sdr.png`, `info-7.1-hdr.png` and `info-7.1-mistag.png`. Both native ABIs,
both release APKs and instrumentation APKs build; native overlays are idempotent
across 245 C/header files, and diff whitespace checks pass.

Packages: `.codex-tmp/hdr/nTv-hdr-info.apk` (ARM32) and
`.codex-tmp/hdr/nTv64-hdr-info.apk` (ARM64). The physical HDR passthrough limitation
described above is unchanged; these checks verify source information and the HUD.

## Android 7.1 physical black-screen diagnosis

The user confirmed that the MI 6 phone's physical screen is black in hardware HDR
passthrough, not just the screenshot or remote preview. Runtime display probing
returns `getHdrCapabilities().getSupportedHdrTypes() = []` and maximum luminance
`-1`; dumpsys reports only colour mode 0. The HEVC decoder runs, but the display
driver cannot prepare native HDR layers. At 09:46:27 on 2026-10-09, immediately
after passthrough configured BT.2020/PQ and the decoder supplied a
`0x7fa30c09` (Qualcomm TP10 UBWC) Surface buffer, the device logged:

```text
DisplayBase::HandleHDR: Setting HDR color mode = hal_hdr
DisplayBase::SetColorModeInternal: Failed: Unknown Mode : hal_hdr
DisplayBase::Prepare: HandleHDR failed
HWCDisplay::PrepareLayerStack: Prepare failed. Error = 2
```

This establishes an unsupported physical HDR display path. Main10 decoding support
does not establish HDR display support. At diagnosis, `NativeHdrOutput.create()`
returned the direct Surface immediately for mode 1, without a display capability
check; that was the application-side compatibility gap. The existing hardware
decoder plus GPU HDR-to-SDR mapping path is the appropriate output on this phone.
The default preference was not changed by this investigation.

The Android HDR contract explains the distinction and the OEM-dependent nature of
non-tunneled Surface output on Android 7:
https://source.android.com/docs/core/display/hdr
The matching Qualcomm display code returns `kErrorNotSupported` for a missing
colour mode:
https://android.googlesource.com/platform/hardware/qcom/display/+/android-8.1.0_r28/msm8998/sdm/libs/core/display_base.cpp

The diagnosis harness now reports display HDR types, excludes the channel card
from black-video capture measurements, invalidates pending startup catalogue
loads for fixtures and rejects lost foreground/player replacement. Fresh extended
comparison attempts were interrupted by installation/settings/file-picker UI;
they are not marked as successful playback tests. Existing successful mapping
and passthrough decoder tests above, runtime driver failure and physical-screen
confirmation are the evidence for this finding. A 10bit SDR-tagged comparison
fixture was generated but not successfully tested. Logs are under
`.codex-tmp/hdr/log-hdr-black-full.txt` and `test-hdr-black-diagnosis.txt`.

## Display capability fallback and hardware/soft display (2026-10-09)

This was subsequently tightened for API 25 and earlier; see the 2026-10-10
end-to-end decoder pixel audit below. Display capability alone does not establish
Android 7's non-tunneled HDR output.

The compatibility gap above is fixed. Default hardware mode now checks the display
owning the playback SurfaceView at codec configuration: PQ requires HDR10/HDR10+,
and HLG requires HLG. Unsupported, unknown, disconnected and pre-API-24 displays
use HDR-to-SDR mapping. Main10 decode support is not used as evidence of display
support. The new **硬解软显** option (`hardware_sdr`, native value 3) forces the
hardware/GPU mapping path even on API 31+, bypassing the decoder tone-map request.
Explicit software decoding and codec fallback remain available. Normal SDR retains
direct Surface output. The default remains hardware and existing choices persist.

| Device / fixture | Verified behaviour |
| --- | --- |
| MI 6, Android 7.1.1, original 1080p HDR10 HEVC/AAC | Hardware -> forced GPU -> BT.709 display and automatic hardware-mode fallback both visible; OMX.qcom.video.decoder.hevc remains active; 157–158 GPU swaps by 6.5 s and about 24 fps for a 24 fps source. Hardware -> hardware_sdr -> BT.709 -> mapping -> hardware switching and saved settings passed. Source HUD remains HDR/10bit/BT.2020/PQ. |
| SM-G3608, Android 4.4.4, 480×270 HDR10 HEVC | Hardware requested; unavailable HEVC hardware falls back to hevc software. hardware_sdr, BT.709 and mapping remain visible at about 24 fps, retaining original HDR/10bit source information. |
| Both devices, 480×270 H.264 BT.709/AAC SDR | hardware and hardware_sdr settings use OMX.qcom.video.decoder.avc and zero HDR GPU bridge swaps; source information remains SDR/8bit/BT.709, screenshots are visible and playback position advances. |

All four fixture runs returned `INSTRUMENTATION_CODE: -1`. Evidence:
`.codex-tmp/hdr/test-compatible-{7.1,4.4}-{hdr,sdr}.txt` and the corresponding
`log-compatible-*` files. Captures `compatible-7.1-auto.png`,
`compatible-7.1-gpu.png` and `compatible-4.4-gpu.png` were inspected. The 7.1 test
process logged the expected mode-1 display fallback and mode-3 GPU output, with
none of the previous `hal_hdr` / HandleHDR failures during its playback.

Both native player/SDL ABIs and release/test APKs build. Host policy checks cover
HDR10/HLG capability mismatch, unknown displays, explicit GPU selection on API35
and preference compatibility. Native overlays are idempotent; JavaScript syntax
and diff whitespace checks pass. The SDR fixture initially hit an incorrect test
assertion requiring GPU output; the assertion now honours an explicit expected
output path and the final runs above passed. No production change was needed for
that test failure.

No attached HDR TV establishes physical passthrough quality, and API31+ decoder
tone mapping is not device-tested here. These checks establish the SDR-screen
fallback and the forced GPU option on the connected 4.4/7.1 devices; they do not
claim every vendor display/HDMI implementation is verified.

The 4.4 SDR switching run sampled zero instantaneous output FPS once, despite
advancing position and a visible capture. A separate hardware_sdr-only repeat
(`test-compatible-4.4-sdr-repeat.txt`) returned 24.2 fps and passed. The zero
sample is retained in the evidence rather than replaced with nominal FPS.

Packages:

* `.codex-tmp/hdr/nTv-hdr-compatible.apk` (ARM32), SHA256 `c6fa2d4eda21e47d10dab4952cdfdc11f8a9065b26a5f126e0fea4f7099b0a16`.
* `.codex-tmp/hdr/nTv64-hdr-compatible.apk` (ARM64), SHA256 `14b2594fd58cabdc82b38327c26c92e681fc85cfeca339ef74cbac7a61ca5491`.

## Reported repeated buffering of the 7.1 HDR fixture (2026-10-09)

The user clarified that the reported interruptions concern the screenshot's test
fixture, not a separate live channel. That screenshot was produced by the HDR
mode-switch instrumentation: it restarts playback for every setting, pauses to
capture a frame, resumes and switches to the next mode at roughly eight-second
intervals. These deliberate interruptions are not evidence of network starvation.
The screenshot's `127.0.0.1:8876` endpoint is an ADB reverse mapping to a temporary
PC fixture server, not an Internet source; it stops working after test cleanup.

A longer fixture repeats the same 1080p HDR10/AAC sample without transcoding.
With hardware_sdr unchanged, continuous production playback via that USB/HTTP
path was monitored for 40 seconds after startup: 40,039 ms wall time, 40,064 ms
position advance, zero buffering-state transitions and 960 GPU swaps. Per-second
output was 23.72–24.34 fps for a 23.976 fps source. Video cache remained 15,056 ms;
the minimum audio cache was 14,987 ms. This excludes starvation in that observed
interval; it does not establish stability of unrelated online channels.

The instrumentation now optionally records continuous buffering/queue/FPS samples
before its screenshot pause. An absolute-path fixture uses a test-only, loopback
Range server on the phone so it exercises the same production player without
Wi-Fi, USB forwarding or changes to production URL handling. The initial attempt
to pass a file path directly failed in the production HTTP proxy and is not a
passed local-playback test. Evidence: `test-buffer-7.1-usb.txt` and
`log-buffer-file.txt` under `.codex-tmp/hdr/`.

The final phone-local file/loopback run passed (`test-buffer-7.1-file.txt`):
40,049 ms wall time, 40,072 ms position advance, zero buffering-state transitions,
960 GPU swaps and visible capture. Video cache stayed at 15,056 ms. Both continuous
tests returned `INSTRUMENTATION_CODE: -1`. This supports test-driven interruptions
as the explanation for the reported fixture, rather than insufficient network
throughput; it does not rule out intermittent faults outside these intervals.
Only test instrumentation and this report changed during this investigation;
production buffering policy and the previously delivered APKs are unchanged.

## Source metadata preparation on API19 / API25 (2026-10-10)

The screenshot's 求索记录 capture is 8-bit AVC. Its original SPS has
`video_signal_type_present_flag=0`; the stream carries no primaries/transfer/matrix
colour description. Host ffprobe agrees. A parser cannot recover absent tags.

AVC now has a bounded SPS/VUI header probe alongside HEVC, using Annex B, avcC
and 1–4 byte length-prefixed samples. The same headers supply MediaCodec format
and the native source snapshot. Parser-serial state preserves newer in-band AVC
headers and resets on a discontinuity. No extra pixel decoder/library was added.
Ordinary 8-bit AVC/MPEG-2 with all three tags absent can expose conservative
SDR/HD-BT.709 or SD-BT.601 values with an explicit `colorInferred` flag. HUD/card
labels say “推定”; HEVC, >8-bit and partially tagged sources retain absent fields.

Final signed Release APKs passed `VideoMetadataInstrumentation` on Samsung
SM-G3608 Android 4.4.4 (API19, ARM32) and MI6 Android 7.1.1 (API25, ARM64).
At `prepared`, with playback not started and position zero, all seven cases passed:

| Fixture | Source interpretation | Inferred |
| --- | --- | --- |
| Original 求索记录 TS | 8-bit SDR / BT.709 | yes |
| Tagged AVC MP4 | 8-bit SDR / BT.709 | no |
| Tagged High10 AVC MP4 | 10-bit PQ / BT.2020 | no |
| Tagged HEVC MP4 | 10-bit HLG / BT.2020 | no |
| Tagged HEVC MP4 | 10-bit PQ / BT.2020 | no |
| Untagged HEVC MP4 | 10-bit; colour/transfer unknown | no |
| Untagged PAL-size AVC MP4 | 8-bit SDR / BT.601-625 / SMPTE170M | yes |

Both final Release APKs also passed real Activity/hardware playback for 求索记录,
tagged BT.709 AVC and untagged PAL-size AVC TS, including advancing position,
video frame output, visible captures and metadata provenance. The AVC parser
unit executable passed real untagged, BT.709 and High10 PQ SPS fixtures, avcC and
sample framing, truncation, invalid lengths and 10,000 malformed headers per fixture.

The same revision fixes the NAS playlist rejection: its 32-hex ETag is not the
body's MD5. ETags now remain opaque cache validators; only explicit MD5 headers
can enforce an MD5 checksum. Final Release tests on both devices imported all
210 entries and passed conditional cache reuse, changed/corrupt cache handling,
explicit bad-MD5 rejection, and the six-second total read budget. The devices
cannot directly route to the NAS subnet; a temporary PC relay forwarded the live
original body and Content-Type/ETag/cache headers. Direct attempts and initial
fixture/runner failures remain in the evidence directory; they are not passes.

Evidence and APKs: `.codex-tmp/source-metadata/`. Release sizes are 3,595,050
(ARM32) and 3,691,682 (ARM64) bytes, 16.1% / 16.7% below the previously delivered
Debug APKs. Debug packaging also excludes authoring Markdown and device-matrix
test assets. This revision's native header reader adds only a few KiB compressed.

## Android 7 output policy and decoded-pixel audit (2026-10-10)

The user's Amlogic TV still appeared grey. It was not connected for testing;
the source HDR label and prior phone playback do not establish its output quality.
The audit requested here used the attached MI 6 on Android 7.1.1 (API 25).

`HdrDecoderColorInstrumentation` tests the complete hardware colour chain:
known encoded YUV patches -> Qualcomm MediaCodec -> the production
`NativeHdrOutput` bridge -> output Surface -> GLES pixel readback. Previously,
`HdrColorInstrumentation` bypassed MediaCodec and channel tests only required
visible/advancing playback. Seven lossless HEVC fixtures cover 8/10-bit HLG,
full/limited range, 8/10-bit PQ, BT.2020 SDR and BT.709 SDR. All 112 patches pass,
with maximum RGB error 1/255; the SDR control has no HDR GPU bridge. Generate
fixtures with `python tools/ijk/generate-hdr-color-fixtures.py` and run the
instrumentation with `-e manifest /sdcard/Android/data/xiao.bu.tv/files/hdr-output/colors.json`.

The reproducible policy defect is distinct from that successful GPU path:
hardware mode previously allowed HDR passthrough based only on display HDR
capability, even on API 24/25. IJK does not use tunneled video. Android 7
[only guarantees tunneled HDR playback](https://android.googlesource.com/platform/docs/source.android.com/+/0ae2f1ea4ee95880039e95da92af6b08eeb8c3dc/src/devices/tech/display/hdr.jd#104),
and has no standard API proving non-tunneled HDR output. The policy now excludes
API 25 and earlier from passthrough even with a matching HDR-capability array.
It keeps hardware decoding and maps HDR to SDR. Policy regression checks cover
this case, capability mismatch, modern matched displays and forced GPU mode.
This addresses an unverified direct-output route; it does not prove that it was
the particular TV's cause. API 26+ native output still depends on the vendor's
decoder/display implementation; the forced-GPU setting remains available.

Fresh CCTV-4K capture from the supplied NAS plus original Sichuan/Hunan/CCTV-16
fixtures all retain 3840×2160, hardware decoding, real HLG transfer=7, GPU swaps,
advancing audio/video and visible output. The fresh CCTV SPS explicitly signals
8-bit, limited range, BT.2020 primaries/matrix (9), HLG (18). Ordinary BT.709 SDR
retains direct output. Longer 6.5-second 4K sampling measures about 25 fps; the
initial startup sampling was about 12 fps and is retained in the report.
This is short playback validation, not a long-running performance benchmark.

The debug third line and `/api/media` now distinguish source HDR information
from `colorOutput` (`GPU→SDR`, `HDR直出`, etc.). The bridge's decoder standard,
transfer and range are also exposed as `decoderColor`. Evidence and signed
Release APKs are in `.codex-tmp/hdr-output/`. Sizes are 3,595,706 (ARM32) and
3,692,126 (ARM64) bytes, only +656/+444 bytes from the previous slim Release.
No new native library, test media or fixture code is bundled in the app.


## Generic HDR default and partial decoder metadata (2026-10-10)

The user requested a universal solution after providing the TV's Android 11
system information. No model, manufacturer, hardware or board condition is
used. Default `hardware` now supplies native mode 3, exactly like the existing
`hardware_sdr` option, retaining hardware decoding with GPU HDR-to-SDR mapping
on every supported API level. Existing default preferences get this path on
upgrade. Manual native HDR output is available only through the explicit
`passthrough` selection, subject to format/display/API checks. The earlier
API 25 restriction remains relevant to this opt-in, not the default route.
Host and device policy assertions cover API 19/25/26/30/31/35 with matching PQ
and HLG display capabilities; none permit default passthrough or codec mapping.
These assertions do not establish physical playback on every listed SDK.

A second defect is reproducible through the real decoder/GPU pixel chain:
when source HLG buffers remain unchanged but the output format has transfer
3 (SDR) or 1 (linear) and omits its colour standard, the previous GPU bridge
assumed BT.709 and bypassed mapping. Its maximum RGB error was 104/255.
The bridge now retains configured source colour data for incomplete output
formats; complete SDR metadata is still honoured. The final APK passes all
16 cases (256 patches, maximum error 1/255) on MI 6 Android 7.1.1:
8/10-bit PQ/HLG, full/limited range, wide-gamut SDR, ordinary SDR, missing or
invalid fields, and complete decoded SDR output. Partial output tags are
injected without altering the actual hardware-decoded pixels. This establishes
the code defect/fix, but does not prove a remote TV/MTK decoder reports these tags.

Fresh CCTV-16 capture has HEVC SPS bit_depth_luma_minus8=0,
bit_depth_chroma_minus8=0, primaries/matrix=9 (BT.2020), transfer=18 (HLG):
the original stream really is 8-bit HLG. Apple's separate 1080p test fixture is
Main10/yuv420p10le PQ. Both are correctly reported and play through the real
Activity on Android 7.1. This distinction is not evidence that source metadata
should be rewritten as 10-bit. Full results, captures, before/after pixel data
and the signed APKs are under `.codex-tmp/hdr-metadata-fallback/`.

The remote TV and MTK phone are not connected. Universal routing and a passing
pixel test do not certify their physical panel colour. Native HDR brightness is
not preserved by the default SDR mapping; users with a verified native HDR chain
can explicitly select HDR passthrough. No extra native dependency or media is
bundled for this revision.


## FPS labels and reordered interlaced timestamps (2026-10-10)

The user's screenshot `50.0/25.0 fps` was output/source cadence, not evidence
of a 25 fps hardware cap. A fresh original Luzhou channel capture is H.264
1920x1080, top-field-first (`field_order=tt`), 25 frames / 50 fields per second.
The HUD and control-page state now explicitly label output fps and source
frames/fields. Native source metadata carries known scan order; unknown scan
order is not inferred from resolution or a 25 fps value. `/api/media` exposes
`scanType` and `sourceFieldRate` separately from actual output cadence.

The fresh remuxed PAFF fixture uncovered a real source-statistics bug:
its B-picture timestamp reordering repeatedly reset the previous monotonic
counter, leaving source cadence 0. The new media-time bins allow bounded
reordering without using arrival speed, nominal field rate or decoder throughput.
True timestamp discontinuities and stream/seek serial changes still reset.
Host timing tests cover 25/30/50 fps, reordering, discontinuity, repaint and stall.

Final ARM64 Release on MI 6 Android 7.1.1 passes all three Activity fixtures:
Luzhou interlaced source -> 25 source frames / 50 fields, about 24.2 submitted
pictures/s on Qualcomm; true progressive 1080p50 -> 50 source/50 submitted
pictures/s (asserted minimum 45); CCTV16 HLG -> 25 source/about 25 output, retaining
8-bit HLG and the GPU SDR mapping route. Each samples about 6.5 seconds, advances
position and retains hardware decoding with a visible capture. These results
are not physical panel cadence measurements or a guarantee of every device's
interlaced deinterlacing rate. The remote Amlogic screenshot already reports
about 50 submitted pictures/s; no model-specific decoder policy was added.

Evidence, captures and signed ARM32/ARM64 Release APKs:
`.codex-tmp/fps-label/`. The initial failed 0 fps statistic remains in the report;
it is not counted as a pass. APKs are 3,596,822 / 3,692,962 bytes, respectively
+752/+500 bytes from the preceding generic HDR Release. The ARM32 native library
is rebuilt, but physical playback in this revision is tested on ARM64/API25.


## 2026-10-10: quality-first software deinterlacing

Automatic software BWDIF now sends both interlaced fields as progressive frames.
No `deinterlace=auto/1` option is sent to IJK: this build has no such option.
Hardware output keeps vendor/system handling and is explicitly unverified;
there are no manufacturer/model adaptations. Advanced settings document the
software BWDIF choice and CPU requirement.

Seven production-wrapper fixtures pass exact pixel comparison against FFmpeg
on ARM64 Android 7.1.1 and ARM32 Android 4.0.4: 8/10-bit, top/bottom first,
HD multithreading and untouched progressive video. Assertions additionally
check PTS, durations, source colour, seek/EOF and reference ownership. Comparison
exposed and corrected old FFmpeg 3.4 high-depth edge-row bounds; initial
mismatches remain in `.codex-tmp/deinterlace/pixels.json`, not counted
as successful validation. Final results are `pixels-arm64/pixels.json` and
`pixels-arm32/pixels.json` (all differences and maximum errors are zero).

The final Release Activity test on Android 7.1 passes five fixtures: original
1080i50 software BWDIF, moving 360i50 software BWDIF, original 1080i50 hardware
control, progressive 1080p50 hardware, and CCTV16 4K HLG hardware GPU-to-SDR.
The original stream retains 25 source frames/50 fields and outputs about 50 fps
with BWDIF. Hardware field-rate/quality is not certified by the software test.
The physical user's TV and a current Android 4.4 Activity playback session were
not available for this revision; ARM32 native compatibility was exercised on
API 15 instead. Source fixtures and test executables are not packaged.

Evidence and signed APKs: `.codex-tmp/deinterlace/`. ARM32 Release is 3,602,354
bytes (+5,532); ARM64 Release is 3,700,790 (+7,828), relative to the immediately
preceding FPS-label build. Native libraries and both Release variants are rebuilt,
and APK v1/v2 signatures verified.


## 2026-10-10: Android 4.0.4 committed channel-swipe fix

The committed-swipe path called `SurfaceHolder.lockCanvas()` to erase the last
video frame. On API 15 this left a CPU producer (API 2) attached to the same
Surface needed by the next MediaPlayer producer (API 3), producing `connect:
already connected (cur=2, req=3)`, -22, then HLS error -38/0. A deterministic
local HLS fixture reproduces baseline success followed by first-swipe failure.

Remove video-Surface Canvas drawing entirely. The existing independent blackout
View covers the outgoing frame and is removed by the incoming-frame ready
callback. This is a generic Surface ownership fix, without model checks. Preserve
the existing detach-before-reset/release order and asynchronous release barrier.
Moving system detach after reset was tested and failed on ICS because the window
remained connected (API 3); that intermediate change was reverted.

`LegacyChannelSwitchInstrumentation` drives actual `animateRelativeChannelSwitch`
and `startChannel`, serving local HLS fixture files through a loopback HTTP server.
Final Release passes baseline, six committed swipes and rapid latest-wins on
physical Android 4.0.4 (API 15, system AndroidMediaPlayer); final logs contain no
new Surface connection failures or system HLS errors. Android 7.1.1 passes
baseline, four committed swipes and rapid replacement on its IJK path. Tests
assert video dimensions, advancing playback clock, focus and blackout dismissal.
This is lifecycle validation, not a new HDR pixel or field-rate certification.

Evidence: `.codex-tmp/channel-switch-legacy/report.json`, final device reports and
before/after logs. Signed ARM32/ARM64 Release packages are 3,602,226 / 3,700,598
bytes, 128 / 192 bytes smaller than the preceding deinterlace build. Both v1/v2
signatures verify. Test fixture videos and instrumentation are excluded from the
release APK. Earlier failed attempts remain as separate artifacts.
