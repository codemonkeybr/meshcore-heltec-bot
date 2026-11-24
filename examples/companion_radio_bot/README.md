# companion_radio_bot

MeshCore **companion radio firmware with an on-device command engine** for the
Heltec V4 (ESP32-S3). It answers mesh commands (`ping`, `path`, …) directly on
the device — no host computer — while keeping the standard companion protocol
available over TCP/BLE/USB for a phone app.

This is a downstream project of [MeshCore](https://github.com/meshcore-dev/MeshCore).
It reuses `examples/companion_radio/*` **unchanged** and substitutes a `MyMesh`
subclass (`BotMesh`) for the global `the_mesh` via two build flags, so the delta
against upstream is tiny and rebases cleanly.

## Upstream delta

| File | Change |
|---|---|
| `examples/companion_radio/MyMesh.h` | `extern MyMesh the_mesh;` → guarded `MESH_CLASS` macro (defaults to `MyMesh`) |
| `examples/companion_radio/main.cpp` | optional `#include MESH_CLASS_HEADER`; `MyMesh the_mesh` → `MESH_CLASS the_mesh` |

Both are upstream-friendly and could be PR'd.

## Layout

```
BotMesh.{h,cpp}       MyMesh subclass: hooks onChannelMessageRecv / onMessageRecv,
                      calls base first, splits "<sender>: <body>" for channels,
                      loop guard (ignores own name), gate() = channel allow-list
                      + rate limiter, then runs the matched command. botReply()
                      sends over the mesh (channel or DM).
CommandRouter.{h,cpp} Compile-time command table + match() (no dynamic plugins).
RateLimiter.{h,cpp}   Token buckets: global / per-channel / per-sender. A reply
                      needs a token in all three; keyFromName() hashes channel
                      senders (DMs key on pubkey prefix).
BotConfig.h           All compile-time knobs (rates, allow-list, prefix, version).
BotMsg.h              Normalised inbound message (carries routing path).
BotDebug.h            `BOT_DEBUG_PRINTLN`, gated on -D BOT_DEBUG=1 (dev env only).
commands/
  Command.{h,cpp}     Base class: matchesKeyword() + args() parsing.
  PingCommand.h       ping -> pong
  VersionCommand.h    version -> bot/firmware version + uptime
  TestCommand.h       test / t -> SNR + hop count
  HelloCommand.h      hello / hi / hey
  DiceCommand.h       dice [NdM(+K)]
  RollCommand.h       roll [N]
  Magic8Command.h     magic8 / 8ball
  CmdCommand.h        cmd -> keyword list
  HelpCommand.h       help [cmd]
```

## Install

Heltec WiFi LoRa 32 **V4** only. Use the browser installer from the release page (Chrome, Edge or
Opera), or flash `firmware-merged.bin` at `0x0` with esptool. Step by step, including the BOOT-button
fallback and updates over WiFi: [FLASHING.md](FLASHING.md).

## First setup

1. Power the board from a charger or power bank. With no WiFi saved it opens a network called
   `MeshBot-XXXXXX`; the password is on the board's screen. Join it and the setup page opens.
2. Pick your WiFi network. The board reboots, joins it, and shows its address on the screen.
3. Open that address in a browser (user `admin`, and the web password you chose during setup).
   The **Settings** page has the bot's name, location, UTC offset, channel allow-list,
   per-command on/off, wx units and rate limits.
4. Set the LoRa region and the channel the bot answers on from the MeshCore app, as for any
   companion radio.

Holding BOOT for 10 s wipes the saved WiFi (keeps the web password).

## MQTT observer uplink (off by default)

The bot can also publish the packets it hears to a MeshCore analyzer over MQTT. It does nothing until you
set **both** a broker (a preset or a custom one) and your **IATA location code** on the Settings page,
then reboot. The community brokers are shared, volunteer-run infrastructure: use your own region's
code and the operator's rules.

## Credits and licence

MIT. Built on MeshCore and other people's work, listed with their licences in
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).

## Building from source

```
pio run -e heltec_v4_companion_radio_bot               # release flags
pio run -e heltec_v4_companion_radio_bot_dev           # + diagnostics and serial debug prints
./examples/companion_radio_bot/release.sh              # clean, secret-free release into out/release/<version>/
```

`platformio.local.ini` (gitignored) holds dev-only overrides such as a compile-time WiFi fallback; the
release script builds with it hidden. See [DEV.md](DEV.md) for flashing and OTA notes.
