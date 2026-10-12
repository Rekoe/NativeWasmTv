"""Capture and decode every M3U entry with bounded concurrency/timeouts.

This tests source availability/bitstreams, not Android decoder/display support.
Retained original packets can then be replayed by the Android test runner.
"""
import argparse
import concurrent.futures
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import time


def entries(path):
    info = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if line.startswith("#EXTINF:"):
            info = line
        elif line and not line.startswith("#") and info:
            group = re.search(r'group-title="([^"]*)"', info)
            yield {"name": info.rsplit(",", 1)[-1], "group": group[1] if group else "", "url": line}
            info = None


def command(args, timeout):
    result = subprocess.run(args, capture_output=True, timeout=timeout)
    return result.returncode, result.stdout.decode("utf-8", "replace"), result.stderr.decode("utf-8", "replace")


def audit(row, folder, seconds, timeout, reuse=False):
    row = dict(row)
    history = list(row.get("attempts", []))
    if not history and row.get("status"):
        history.append({"status": row["status"], "error": row.get("error", "")})
    start = time.monotonic()
    clip = Path(row["clip"]) if reuse and row.get("clip") else folder / f"{row['index']:04d}.ts"
    try:
        if not (reuse and row.get("clip") and clip.exists()):
            code, _, errors = command([
                "ffmpeg", "-hide_banner", "-loglevel", "warning", "-rw_timeout", "8000000",
                "-analyzeduration", "3000000", "-probesize", "4000000", "-i", row["url"],
                "-t", str(seconds), "-map", "0:v:0?", "-map", "0:a:0?", "-c", "copy", "-y", str(clip),
            ], timeout)
            (folder / f"{row['index']:04d}-capture.log").write_text(errors, encoding="utf-8")
            if code or not clip.exists() or clip.stat().st_size < 1024:
                raise RuntimeError("Capture failed: " + errors[-600:])
        code, metadata, errors = command([
            "ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(clip),
        ], 12)
        if code:
            raise RuntimeError("Probe failed: " + errors[-600:])
        data = json.loads(metadata)
        fields = ("index", "codec_type", "codec_name", "profile", "width", "height", "pix_fmt", "field_order",
                  "r_frame_rate", "color_range", "color_space", "color_transfer", "color_primaries",
                  "sample_rate", "channels", "channel_layout")
        row["streams"] = [{key: stream[key] for key in fields if key in stream} for stream in data["streams"]]
        row["duration"] = data.get("format", {}).get("duration")
        row["clip"] = str(clip.resolve())
        code, progress, errors = command([
            "ffmpeg", "-hide_banner", "-loglevel", "warning", "-threads", "2", "-i", str(clip),
            "-map", "0:v:0?", "-map", "0:a:0?", "-progress", "pipe:1", "-f", "null", "-",
        ], 25)
        (folder / f"{row['index']:04d}-decode.log").write_text(errors, encoding="utf-8")
        frames = re.findall(r"^frame=(\d+)\r?$", progress, re.M)
        row["decodedFrames"] = int(frames[-1]) if frames else 0
        row["decodeWarnings"] = errors[-800:]
        if code or not row["decodedFrames"]:
            raise RuntimeError("Decode failed: " + errors[-600:])
        # Retain VUI/SEI evidence when the decoder reports an unknown transfer.
        video = next((s for s in row["streams"] if s.get("codec_type") == "video"), {})
        if video.get("codec_name") == "hevc" and video.get("color_transfer", "unknown") in ("reserved", "unknown"):
            code, _, trace = command([
                "ffmpeg", "-hide_banner", "-loglevel", "info", "-i", str(clip), "-t", "0.1",
                "-map", "0:v:0", "-c", "copy", "-bsf:v", "trace_headers", "-f", "null", "-",
            ], 12)
            lines = [line for line in trace.splitlines() if any(key in line for key in
                     ("transfer_characteristics", "colour_primaries", "matrix_coeff", "bit_depth_luma"))]
            row["vuiSei"] = lines[:20]
        row["status"] = "decoded"
        row.pop("error", None)
    except (subprocess.TimeoutExpired, RuntimeError, ValueError, OSError) as error:
        row["status"] = "failed"
        row["error"] = str(error)
    row["elapsedSeconds"] = round(time.monotonic() - start, 2)
    history.append({"checkedAtUtc": datetime.now(timezone.utc).isoformat(), "status": row["status"],
                    "error": row.get("error", ""), "elapsedSeconds": row["elapsedSeconds"]})
    row["attempts"] = history
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("playlist", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--timeout", type=float, default=35)
    parser.add_argument("--retry-failed", action="store_true")
    parser.add_argument("--reuse-captured", action="store_true")
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    rows = [dict(row, index=i+1) for i, row in enumerate(entries(options.playlist))]
    report = options.output / "audit.json"
    results = json.loads(report.read_text(encoding="utf-8")) if options.retry_failed else []
    pending = [row for row in results if row["status"] == "failed"] if options.retry_failed else rows
    with concurrent.futures.ThreadPoolExecutor(max_workers=options.workers) as executor:
        futures = [executor.submit(audit, row, options.output, options.seconds, options.timeout,
                                   options.reuse_captured) for row in pending]
        for future in concurrent.futures.as_completed(futures):
            row = future.result()
            results = [old for old in results if old["index"] != row["index"]] + [row]
            report.write_text(json.dumps(sorted(results, key=lambda r: r["index"]), ensure_ascii=False, indent=2), encoding="utf-8")
            print(f"{len(results)}/{len(rows)} {row['index']:04d} {row['name']}: {row['status']}", flush=True)
    print(f"Finished: {sum(r['status']=='decoded' for r in results)}/{len(rows)} decoded", flush=True)


if __name__ == "__main__":
    main()
