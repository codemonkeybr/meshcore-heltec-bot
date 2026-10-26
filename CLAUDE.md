# meshcore-heltec-bot

MeshCore fork (branch `heltec-bot`) with an on-device bot in `examples/companion_radio_bot/`.

## Versioning

The bot version is `BOT_VERSION` in `examples/companion_radio_bot/BotConfig.h`. It is shown by the
`version` command, `/api/status`, and the MQTT observer firmware version. It follows
[Semantic Versioning](https://semver.org) (`MAJOR.MINOR.PATCH`).

**Bump `BOT_VERSION` in the same commit as any change to the bot that will be flashed or pushed.**
Pick the level by what the change does to users:

- **PATCH** (`0.3.0` -> `0.3.1`): bug fixes and wording tweaks that add nothing new. Example: reply text
  reworded to stop bot-to-bot loops.
- **MINOR** (`0.3.1` -> `0.4.0`): new backwards-compatible features. Example: a new command or alias,
  a new setting, a new page. Reset PATCH to 0.
- **MAJOR** (`0.x` -> `1.0.0`): changes that break existing users. Example: renamed or removed commands,
  settings or NVS formats that need a reset, changed OTA or web login behaviour. Reset MINOR and PATCH
  to 0. Stay on `0.x` until the bot is declared stable for other people to use; breaking changes while
  on `0.x` bump MINOR.

If a push has several kinds of change, bump for the highest level only, once per push, not once per
commit.

Do not touch `FIRMWARE_VERSION`; that is the upstream MeshCore version.

**After bumping, verify on the device.** The `/api/status` `version` field is
`<BOT_VERSION> <build date> <build time>`. The build date/time part only updates when
`BotWebUI.cpp` is recompiled, so a changed version number in `/api/status` is the reliable sign that
an OTA took, together with a fresh `uptime_s`. If the version still shows the old value after a
build, a header change was not picked up: run `pio run -e heltec_v4_companion_radio_bot -t clean`
and rebuild.
