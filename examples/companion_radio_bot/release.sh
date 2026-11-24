#!/usr/bin/env bash
# Build a distributable release into out/release/<version>/ :
#   firmware-merged.bin  full image for first install (flash at 0x0)
#   firmware.bin         app image for OTA updates (web UI /update)
#   firmware-update.bin  USB update image (flash at 0xe000): boot_app0 + app, keeps
#                        NVS settings and the filesystem, resets the OTA slot to app0
#   SHA256SUMS
# site.sh turns these into the browser installer page.
# Env: PIO (pio binary), OUT_DIR (override output folder).
# Builds with platformio.local.ini hidden so no WiFi password, location or other
# local settings can end up in the binary.
set -euo pipefail
cd "$(dirname "$0")/../.."

ENV=heltec_v4_companion_radio_bot
VER=$(sed -n 's/^#define BOT_VERSION "\(.*\)"/\1/p' examples/companion_radio_bot/BotConfig.h)
[ -n "$VER" ] || { echo "cannot read BOT_VERSION"; exit 1; }
OUT=${OUT_DIR:-out/release/$VER}

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
# boot_app0.bin is exactly the 0x2000-byte otadata slot (0xe000..0x10000) and the app
# starts at 0x10000, so the two concatenate into one contiguous image.
BOOT_APP0="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
[ "$(stat -c %s "$BOOT_APP0")" -eq 8192 ] || { echo "unexpected boot_app0.bin size"; exit 1; }
cat "$BOOT_APP0" "$OUT/firmware.bin" > "$OUT/firmware-update.bin"
(cd "$OUT" && sha256sum firmware-merged.bin firmware.bin firmware-update.bin > SHA256SUMS)

# Guard: nothing from a local config may be in the image.
if [ -f "$HOLD" ]; then
  while IFS= read -r secret; do
    [ -n "$secret" ] && grep -qF -- "$secret" "$OUT/firmware-merged.bin" && { echo "LEAK: '$secret' found in image"; exit 1; }
  done < <(sed -n 's/^\(wifi_ssid\|wifi_pwd\)[[:space:]]*=[[:space:]]*//p' "$HOLD")
fi
echo "release $VER ready in $OUT"
ls -la "$OUT"
