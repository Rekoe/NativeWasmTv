# HDR output validation — 2026-10-09

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
