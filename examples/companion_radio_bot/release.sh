#!/usr/bin/env bash
# Build a distributable release into out/release/<version>/ :
#   firmware-merged.bin  full image for first install (flash at 0x0)
#   firmware.bin         app image for OTA updates (web UI /update)
#   manifest.json + index.html   browser installer (serve over HTTPS)
#   SHA256SUMS
# Env: PIO (pio binary), OUT_DIR (override output folder), REPO (owner/name shown
# on the page, default codemonkeybr/meshcore-heltec-bot).
# Builds with platformio.local.ini hidden so no WiFi password, location or other
# local settings can end up in the binary.
set -euo pipefail
cd "$(dirname "$0")/../.."

ENV=heltec_v4_companion_radio_bot
VER=$(sed -n 's/^#define BOT_VERSION "\(.*\)"/\1/p' examples/companion_radio_bot/BotConfig.h)
[ -n "$VER" ] || { echo "cannot read BOT_VERSION"; exit 1; }
OUT=${OUT_DIR:-out/release/$VER}
REPO=${REPO:-codemonkeybr/meshcore-heltec-bot}

# A release tag must match the firmware version.
TAG=$(git describe --tags --exact-match 2>/dev/null || true)
if [ -n "$TAG" ] && [ "$TAG" != "v$VER" ]; then
  echo "tag $TAG does not match BOT_VERSION $VER" >&2; exit 1
fi
LOCAL=platformio.local.ini
HOLD=$LOCAL.release-hold

if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
  echo "warning: working tree has uncommitted changes; the release will not match any commit" >&2
fi

restore() { [ -f "$HOLD" ] && mv "$HOLD" "$LOCAL" || true; }
trap restore EXIT
[ -f "$LOCAL" ] && mv "$LOCAL" "$HOLD"

PIO=${PIO:-pio}
$PIO run -e $ENV -t clean >/dev/null
$PIO run -e $ENV -t mergebin

B=.pio/build/$ENV
mkdir -p "$OUT"
cp $B/firmware-merged.bin "$OUT/firmware-merged.bin"
cp $B/firmware.bin "$OUT/firmware.bin"
sed -e "s#@VERSION@#$VER#g" -e "s#@REPO@#$REPO#g" examples/companion_radio_bot/flasher/index.html > "$OUT/index.html"
cat > "$OUT/manifest.json" <<JSON
{
  "name": "HelBot",
  "version": "$VER",
  "new_install_prompt_erase": false,
  "builds": [
    { "chipFamily": "ESP32-S3", "parts": [ { "path": "firmware-merged.bin", "offset": 0 } ] }
  ]
}
JSON
(cd "$OUT" && sha256sum firmware-merged.bin firmware.bin > SHA256SUMS)

# Guard: nothing from a local config may be in the image.
if [ -f "$HOLD" ]; then
  while IFS= read -r secret; do
    [ -n "$secret" ] && grep -qF -- "$secret" "$OUT/firmware-merged.bin" && { echo "LEAK: '$secret' found in image"; exit 1; }
  done < <(sed -n 's/^\(wifi_ssid\|wifi_pwd\)[[:space:]]*=[[:space:]]*//p' "$HOLD")
fi
echo "release $VER ready in $OUT"
ls -la "$OUT"
