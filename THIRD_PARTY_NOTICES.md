# Third-party notices

The meshcore-heltec-bot firmware (`examples/companion_radio_bot/`) is MIT licensed
(see `license.txt`). It is built on, and ships with, the work of others. This file lists
that work and its licences. Each licence's text is in the linked upstream repository or in
the vendored directory noted below.

## Code in this repository

| Component | Licence | Notes |
|---|---|---|
| [MeshCore](https://github.com/meshcore-dev/MeshCore) | MIT, Copyright (c) 2025 Scott Powell / rippleradios.com | The firmware this project forks. `src/`, `examples/companion_radio/`, `variants/`. |
| [agessaman/MeshCore](https://github.com/agessaman/MeshCore), branch `mqtt-bridge-implementation-flex`, commit `c2c4cb59e34c9d305a23bfc5d30f16c6fc3e30b2` | MIT, Copyright (c) 2025 Scott Powell / rippleradios.com | The MQTT observer uplink: `src/helpers/bridges/MQTTBridge.*` and the `src/helpers/MQTT*.h/.cpp` files. Vendored with small local changes (publish skip threshold, loss counters, custom-broker TLS via the ESP-IDF root bundle). |
| [PsychicMqttClient](https://github.com/theelims/PsychicMqttClient) (`lib/PsychicMqttClient/`) | MIT, Copyright (c) 2024 elims | MQTT client used by the bridge, with the MeshCore memory-leak fix. Its `LICENSE` is in that directory. |
| [meshcore-bot](https://github.com/agessaman/meshcore-bot) | MIT, Copyright (c) 2025-2026 Adam Gessaman and contributors | Not linked. The on-device commands reimplement its commands, and the reply formats follow it so both bots answer alike. |
| `lib/TimezoneStub/` | MIT | A small stand-in for the type definitions of the `Timezone` library, which is GPL-3.0 and is therefore not used. |

## Libraries linked into the firmware

| Library | Licence |
|---|---|
| [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) and [AsyncTCP](https://github.com/ESP32Async/AsyncTCP) (ESP32Async) | LGPL-3.0 |
| [AsyncElegantOTA](https://github.com/ayushsharma82/ElegantOTA) | MIT, Copyright (c) 2019 Ayush Sharma |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | MIT, Benoit Blanchon |
| [base64](https://github.com/Densaugeo/base64_arduino) | MIT, Copyright (c) 2016 Densaugeo |
| [NTPClient](https://github.com/arduino-libraries/NTPClient) | See the upstream repository |
| [RadioLib](https://github.com/jgromes/RadioLib) | MIT, Copyright (c) 2018 Jan Gromes |
| [Crypto](https://github.com/rweather/arduinolibs) (Rhys Weatherley) and the other libraries MeshCore already depends on (Adafruit sensor/display libraries, RTClib, and others) | Their own licences, as in upstream MeshCore |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) / [ESP-IDF](https://github.com/espressif/esp-idf) | LGPL-2.1 / Apache-2.0 |
| Mozilla root certificate bundle (inside ESP-IDF) | MPL-2.0 |

### LGPL libraries

ESPAsyncWebServer and AsyncTCP are LGPL-3.0 and are linked statically. The complete
corresponding source of this firmware is this repository, and `release.sh` plus
`platformio.ini` rebuild the exact image. You can rebuild the firmware with a modified copy
of those libraries and flash it to your own board, which is what the LGPL asks for.

## Data sources used by commands

- **Weather, rain and air quality**: [Open-Meteo](https://open-meteo.com) (CC BY 4.0). Free for
  non-commercial use with attribution; do not run this as a commercial service on the free API.
- **Solar and HF conditions**: [NOAA Space Weather Prediction Center](https://www.swpc.noaa.gov), US government data.
- **Jokes**: [icanhazdadjoke](https://icanhazdadjoke.com) and [JokeAPI](https://jokeapi.dev), under their own terms.
- **MQTT uplink**: when enabled, packets are published to the broker you choose (for example
  the community brokers listed in the settings page). Those are shared infrastructure run by
  volunteers; use your own region's IATA code and the operators' rules.
