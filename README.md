# CRTFace

A little desk companion: an ESP32-S3 with a 480×480 screen in a small CRT-style body,
showing a face made of glowing terminal letters (`o_o`). You control it from a web page
on your phone or computer.

## What it does

- **Face** – an emoticon in dot-matrix letters with phosphor glow and scanlines. It blinks,
  glances around, follows your pointer from the control page, gets happy when you click it,
  and dozes off when left alone. Expressions: neutral, happy, sad, angry, surprised, sleepy.
  Color themes: green, amber, ice, pink.
- **Music** – what's playing on your Spotify (optional, needs a free Spotify developer app).
- **Weather** – current conditions from [Open-Meteo](https://open-meteo.com), no account needed.
- **Desktop** – a retro gray desktop you drive with the remote mouse: icons, start menu,
  taskbar clock, and draggable Clock, Paint and System windows.

The board's **BOOT** button cycles Face → Music → Weather. The Desktop is opened from the web page.

## Hardware

- Waveshare ESP32-S3-Touch-LCD-4 (ESP32-S3 N16R8, 480×480 ST7701 RGB panel). The touch layer isn't used.

## Build and flash

1. Install VS Code with the **pioarduino** extension (not the regular PlatformIO one:
   `platformio.ini` pins a pioarduino ESP32 platform).
2. Copy `include/secrets.example.h` to `include/secrets.h` and fill in your Wi-Fi and a
   password for the device's own network. `secrets.h` is git-ignored.
3. Optionally edit `include/config.h`: hostname, time zone, when the device's own network
   is on, and the weather location (by default it's guessed from your IP address).
4. Build and upload from the pioarduino sidebar.

## Using it

Until someone opens the control page, the screen shows where to find it:

- On your Wi-Fi: `http://crtface.local`, or the IP shown on screen.
- On the device's own network (`CRTFace-XXXX`): `http://crt.face` or `http://192.168.4.1`.
  Handy on dorm, university and other 802.1x networks (eduroam: set `WIFI_USERNAME`),
  which often block device-to-device traffic.

The control page has no login. Anyone on the same network, or anyone who knows your
`AP_PASSWORD`, can drive the screen.

## Spotify (optional)

Follow the steps in `include/secrets.example.h`. `tools/spotify_token.py` (Python 3, no
packages needed) does the one-time sign-in and prints the refresh token for `secrets.h`.

## Credits

- Libraries (fetched by PlatformIO): [GFX Library for Arduino](https://github.com/moononournation/Arduino_GFX)
  (also the 5×7 font), [ESPAsyncWebServer and AsyncTCP](https://github.com/ESP32Async),
  [ArduinoJson](https://arduinojson.org).
- The control page uses [Mona Sans](https://github.com/github/mona-sans) by GitHub, shipped in
  `src/mona_sans.h` under the SIL Open Font License (`src/mona_sans_OFL.txt`).
- Weather by [Open-Meteo](https://open-meteo.com), IP location by [ip-api.com](https://ip-api.com).

## License

[MIT](LICENSE), except the Mona Sans font (SIL OFL 1.1).
