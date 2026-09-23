#!/usr/bin/env bash
#
# Regenerate the demo JPEG frames embedded in the firmware image.
#
# Writes main/frames/frame_0.jpg .. frame_<FRAME_COUNT-1>.jpg, each at most
# MAX_BYTES bytes. The outputs are checked into git on purpose, so building
# the firmware never requires GStreamer -- run this by hand only when the
# pattern, the geometry or the quality changes.
#
# Requires gst-launch-1.0 with videotestsrc, jpegenc and multifilesink.

set -euo pipefail

readonly FRAME_COUNT=4
readonly WIDTH=800
readonly HEIGHT=600
readonly MAX_BYTES=30720            # 30 kB per frame, hard ceiling

# Static SMPTE colour bars. The frames still differ from each other because
# the pattern's noise block is regenerated per frame.
readonly PATTERN=smpte

# smpte is not a cheap pattern to encode -- it carries a noise block and fine
# chroma bars -- so it needs a quality well below jpegenc's default of 85.
# Measured at 800x600 I420: 85 gives ~37.7 kB per frame, 75 ~32.2 kB and
# 70 ~30.6 kB, all of which blow the 30 kB budget. 65 lands at ~29.0 kB,
# keeping ~5% headroom for the noise block's run-to-run variation. Switch
# PATTERN to smpte75 (~17 kB) if you ever want considerably smaller frames.
readonly QUALITY=65

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
readonly OUT_DIR="${script_dir}/../main/frames"

if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
    echo "error: gst-launch-1.0 not found in PATH" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

frame_path() {
    printf '%s/frame_%d.jpg' "$OUT_DIR" "$1"
}

# wc -c rather than stat, which takes -f%z on macOS and -c%s on Linux.
file_size() {
    wc -c < "$1" | tr -d ' '
}

file_digest() {
    if command -v md5 >/dev/null 2>&1; then
        md5 -q "$1"
    else
        md5sum "$1" | cut -d ' ' -f 1
    fi
}

rm -f "$OUT_DIR"/frame_*.jpg

# format=I420 is pinned on purpose. Left to negotiate, videotestsrc and
# jpegenc both prefer Y444 (4:4:4) and every file comes out ~65% larger for no
# visible gain at this size. videotestsrc emits I420 natively, so no
# videoconvert is needed.
gst-launch-1.0 -q \
    videotestsrc pattern="$PATTERN" num-buffers="$FRAME_COUNT" \
    ! "video/x-raw,format=I420,width=${WIDTH},height=${HEIGHT},framerate=5/1" \
    ! jpegenc quality="$QUALITY" \
    ! multifilesink location="${OUT_DIR}/frame_%d.jpg" index=0

# Enforce the budget rather than trusting the quality setting to hold after a
# change to the pattern or the geometry.
over_budget=0
echo "pattern=${PATTERN} quality=${QUALITY} budget=${MAX_BYTES} B"
i=0
while [ "$i" -lt "$FRAME_COUNT" ]; do
    f=$(frame_path "$i")
    if [ ! -f "$f" ]; then
        echo "error: ${f} was not produced" >&2
        exit 1
    fi
    size=$(file_size "$f")
    if [ "$size" -gt "$MAX_BYTES" ]; then
        over_budget=$((over_budget + 1))
        printf '  %-12s %6s B  %s  OVER BUDGET\n' \
            "$(basename "$f")" "$size" "$(file_digest "$f")"
    else
        printf '  %-12s %6s B  %s\n' \
            "$(basename "$f")" "$size" "$(file_digest "$f")"
    fi
    i=$((i + 1))
done

if [ "$over_budget" -gt 0 ]; then
    echo "error: ${over_budget} frame(s) exceed ${MAX_BYTES} B -- lower QUALITY," >&2
    echo "       pick a flatter pattern, or shrink the frame." >&2
    exit 1
fi
