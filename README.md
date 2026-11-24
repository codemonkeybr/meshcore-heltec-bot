# HelBot

A mesh chat bot that runs **on the radio itself**. Flash it to a Heltec WiFi LoRa 32 V4, give it WiFi,
and it answers commands on your [MeshCore](https://github.com/meshcore-dev/MeshCore) channel: `ping`, `path`,
`wx`, `sun`, `moon`, and more. No computer or server needed, and the normal companion radio (phone app over
TCP/BLE/USB) keeps working.

**Install it from your browser: https://codemonkeybr.github.io/meshcore-heltec-bot/**
(Chrome, Edge or Opera on a computer, USB cable, about two minutes.)

## What it does

| | Commands |
|---|---|
| Basics | `ping`, `test` / `t`, `version`, `hello`, `help`, `cmd` |
| Mesh | `path` / `p` (hop names with distances), `decode`, `route`, `prefix`, `leaderboard` / `lb`, `stats`, `advert` |
| Local almanac | `sun`, `moon` |
| Internet | `wx` / `weather`, `rain`, `wind`, `aqi`, `solar` / `hf`, `joke`, `dadjoke` |
| Fun | `dice`, `roll`, `magic8` |

Also: a persistent node cache, rate limiting and a loop guard so bots don't talk to each other, a web page for
status and settings (name, location, channels, per-command on/off), WiFi setup from your phone, and updates over WiFi.

An optional **MQTT observer** can publish the packets the radio hears to a MeshCore analyzer. It is off until you
choose a broker and your IATA location code on the settings page.

## Install and update

Step by step on the install page, or in [FLASHING.md](examples/companion_radio_bot/FLASHING.md).
Releases (binaries and checksums) are on the [Releases](../../releases) page.

## Build from source

```
pip install platformio
pio run -e heltec_v4_companion_radio_bot        # firmware
./examples/companion_radio_bot/release.sh       # clean, secret-free release in out/release/<version>/
```

Project layout and developer notes: [examples/companion_radio_bot/README.md](examples/companion_radio_bot/README.md)
and [DEV.md](examples/companion_radio_bot/DEV.md). Pushing a tag like `v0.5.0` builds a release and the install page
in GitHub Actions.

## Credits and licence

MIT. This is a fork of [MeshCore](https://github.com/meshcore-dev/MeshCore) (Scott Powell / rippleradios.com); the
original README is kept at [docs/MESHCORE_README.md](docs/MESHCORE_README.md). The MQTT observer is built on
[agessaman's MeshCore fork](https://github.com/agessaman/MeshCore), and the commands follow
[meshcore-bot](https://github.com/agessaman/meshcore-bot). Everything used, with licences, is in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
