# Flashing the bot

For the **Heltec WiFi LoRa 32 V4** only (ESP32-S3, 16 MB flash). The easiest way is the install page,
https://codemonkeybr.github.io/meshcore-heltec-bot/ : pick *New install* or *Update*, pick a version, and it gives
you the right file and steps. This page describes what is behind it.

## Which file for what

| You want to | File | How | Settings |
|---|---|---|---|
| Install on a new board, or start clean | `firmware-merged.bin` | flash at `0x0` | **erased** (WiFi, web password, bot settings) |
| Update over USB | `firmware-update.bin` | flash at `0xe000` | kept |
| Update over WiFi | `firmware.bin` | upload on the bot's `/update` page | kept |

Why not the merged image for an update: it covers the settings area, so flashing it wipes WiFi and settings. The
update image holds only the app and the boot-slot marker, so settings and the node cache stay, and it also works
after earlier WiFi updates (which move the bot between its two app slots).

## New install, browser

1. Open the install page in Chrome, Edge or Opera on a computer. Choose *New install*.
2. Plug the board in with a USB **data** cable and press **Install**; pick the board's serial port.
3. If the port is not listed or flashing cannot start: hold **BOOT**, tap **RST**, release **BOOT**, then try again.

## esptool

Download the files from the release, check them against `SHA256SUMS`, then:

```
pip install esptool
# new install (erases settings)
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0x0 firmware-merged.bin
# update over USB (keeps settings)
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 921600 write_flash 0xe000 firmware-update.bin
```

(On Windows the port is `COMx`, on macOS `/dev/cu.usbmodem*`.) Add `erase_flash` as a separate first step for a
completely clean start with a new identity. Back up first if the board holds a firmware you want to restore:
`esptool.py --chip esp32s3 --port /dev/ttyACM0 read_flash 0 0x1000000 backup.bin`.

## After a new install

Power the board from a charger or power bank (some PCs hold it in the bootloader). On the first boot it opens the
`MeshBot-XXXXXX` setup network; see the README's *First setup*.

## Updating over WiFi

Download `firmware.bin`, open `http://<board address>/update` in a browser, log in, and upload it. The board
restarts into the new version; check the version on the status page.

From a shell: `curl -H "Expect:" -u admin:<password> -F "MD5=$(md5sum firmware.bin | cut -d' ' -f1)" -F "firmware=@firmware.bin" http://<address>/update`
(`curl` exiting with 56 is normal: the board restarts before it answers.)

## Making a release (maintainers)

Bump `BOT_VERSION` in `BotConfig.h`, commit, then push a tag `v<version>`. GitHub Actions builds the firmware, creates the
release with the files above and redeploys the install page, which also lists the three previous versions.
Locally: `./examples/companion_radio_bot/release.sh` builds with `platformio.local.ini` hidden (so no personal WiFi or
location can leak) into `out/release/<version>/`, and `site.sh <dir>` assembles the install site from it.
