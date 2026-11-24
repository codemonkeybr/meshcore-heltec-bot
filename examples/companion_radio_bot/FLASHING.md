# Flashing the bot

For the **Heltec WiFi LoRa 32 V4** only (ESP32-S3, 16 MB flash). Installing replaces whatever firmware is on the board.

## Option 1: browser installer (easiest)

1. Open the installer page from the release in Chrome, Edge or Opera on a computer (not a phone).
2. Plug the board in with a USB **data** cable and press **Install**; pick the board's serial port.
3. If the port is not listed or flashing cannot start, put the board in download mode: hold **BOOT**, tap **RST**, release **BOOT**, then try again.

## Option 2: esptool

Download `firmware-merged.bin` from the release, check it against `SHA256SUMS`, then:

```
pip install esptool
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0x0 firmware-merged.bin
```

(On Windows the port is `COMx`, on macOS `/dev/cu.usbmodem*`.) Add `erase_flash` as a separate first step if you want a
completely clean start with a new identity and no saved settings. Without it, an upgrade keeps the board's identity,
settings and node cache.

Back up first if the board holds a firmware you want to restore:
`esptool.py --chip esp32s3 --port /dev/ttyACM0 read_flash 0 0x1000000 backup.bin`.

## After flashing

Power the board from a charger or power bank (some PCs hold it in the bootloader). On the first boot it opens the
`MeshBot-XXXXXX` setup network; see the README's *First setup*.

## Updating later (no cable)

Download `firmware.bin` (the app image, not the merged one) and open `http://<board address>/update` in a browser,
log in, and upload it. The board reboots into the new version; check the version on the status page.

From a shell: `curl -H "Expect:" -u admin:<password> -F "MD5=$(md5sum firmware.bin | cut -d' ' -f1)" -F "firmware=@firmware.bin" http://<address>/update`
(`curl` exiting with 56 is normal: the board restarts before it answers.)

## Making a release (maintainers)

`./examples/companion_radio_bot/release.sh` builds with `platformio.local.ini` hidden, so no personal WiFi or
location can leak, and writes the binaries, `manifest.json`, `index.html` and `SHA256SUMS` to `out/release/<version>/`.
The installer page works only over HTTPS with the files next to it (GitHub Pages, or any static host); attach the
same files to the GitHub release for people who prefer esptool.
