#!/usr/bin/env bash
# Assemble the GitHub Pages install site.
#   site.sh <site_dir> [release_dir]
# release_dir (default out/release/<BOT_VERSION>) holds the build made by release.sh.
# Result:
#   <site>/index.html, versions.json
#   <site>/<version>/{firmware-merged.bin,firmware.bin,firmware-update.bin,SHA256SUMS,
#                     manifest-install.json,manifest-update.json}
# Env: REPO (owner/name shown on the page), PREVIOUS=N (also publish the N most recent
# earlier GitHub releases; needs the gh CLI and GH_TOKEN), SITE_TEMPLATE.
set -euo pipefail
cd "$(dirname "$0")/../.."

SITE=${1:?usage: site.sh <site_dir> [release_dir]}
VER=$(sed -n 's/^#define BOT_VERSION "\(.*\)"/\1/p' examples/companion_radio_bot/BotConfig.h)
REL=${2:-out/release/$VER}
REPO=${REPO:-codemonkeybr/meshcore-heltec-bot}
PREVIOUS=${PREVIOUS:-0}
TEMPLATE=${SITE_TEMPLATE:-examples/companion_radio_bot/flasher/index.html}
[ -f "$REL/firmware-merged.bin" ] || { echo "no build in $REL (run release.sh first)"; exit 1; }

mkdir -p "$SITE/$VER"
cp "$REL"/firmware-merged.bin "$REL"/firmware.bin "$REL"/SHA256SUMS "$SITE/$VER/"
[ -f "$REL/firmware-update.bin" ] && cp "$REL/firmware-update.bin" "$SITE/$VER/"

# Earlier releases (the current version always comes from this build).
if [ "$PREVIOUS" -gt 0 ] && command -v gh >/dev/null; then
  for tag in $(gh release list --repo "$REPO" --limit $((PREVIOUS + 1)) --json tagName --jq '.[].tagName'); do
    v=${tag#v}
    [ "$v" = "$VER" ] && continue
    [ -f "$SITE/$v/firmware-merged.bin" ] && continue
    mkdir -p "$SITE/$v"
    gh release download "$tag" --repo "$REPO" --dir "$SITE/$v" \
      -p firmware-merged.bin -p firmware.bin -p firmware-update.bin -p SHA256SUMS || { rm -rf "$SITE/$v"; continue; }
  done
fi

# Manifests and the version list, newest first by semantic version.
mapfile -t VERS < <(for d in "$SITE"/*/; do basename "$d"; done | sort -t. -k1,1nr -k2,2nr -k3,3nr)
printf '[' > "$SITE/versions.json"
first=1
for v in "${VERS[@]}"; do
  [ -f "$SITE/$v/firmware-merged.bin" ] || continue
  usb=false
  cat > "$SITE/$v/manifest-install.json" <<JSON
{
  "name": "HelBot",
  "version": "$v",
  "new_install_prompt_erase": false,
  "builds": [
    { "chipFamily": "ESP32-S3", "parts": [ { "path": "firmware-merged.bin", "offset": 0 } ] }
  ]
}
JSON
  if [ -f "$SITE/$v/firmware-update.bin" ]; then
    usb=true
    cat > "$SITE/$v/manifest-update.json" <<JSON
{
  "name": "HelBot",
  "version": "$v",
  "new_install_prompt_erase": false,
  "builds": [
    { "chipFamily": "ESP32-S3", "parts": [ { "path": "firmware-update.bin", "offset": 57344 } ] }
  ]
}
JSON
  fi
  latest=false; [ $first -eq 1 ] && latest=true
  [ $first -eq 1 ] || printf ',' >> "$SITE/versions.json"
  printf '{"version":"%s","latest":%s,"usb_update":%s}' "$v" "$latest" "$usb" >> "$SITE/versions.json"
  first=0
done
printf ']\n' >> "$SITE/versions.json"

sed -e "s#@REPO@#$REPO#g" "$TEMPLATE" > "$SITE/index.html"
echo "site ready in $SITE: $(cat "$SITE/versions.json")"
