#pragma once

// Control page address on your Wi-Fi: http://HOSTNAME.local (or the IP shown on screen)
#define HOSTNAME "crtface"

// POSIX time zone for the desktop clock. Central Europe: UTC+1, UTC+2 from the last Sunday
// of March to the last Sunday of October.
// Examples: "<+03>-3" (Turkey), "GMT0BST,M3.5.0/1,M10.5.0" (UK).
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// The device's own Wi-Fi network, so you can reach it even when the main network
// blocks device-to-device traffic (common on dorm / 802.1x networks).
//   0 = never, 1 = only if the main Wi-Fi can't connect, 2 = always on
#define AP_MODE     2
// Its password is AP_PASSWORD in secrets.h.
// On that network every name resolves to the device, so this is just the one shown on screen.
// (192.168.4.1 always works too.)
#define AP_DOMAIN   "crt.face"

// Weather screen location. Leave WEATHER_LAT and WEATHER_LON at 0 to find it from your IP address
// (city-level; on a VPN or some university networks it may pick the wrong city).
#define WEATHER_LAT  0.0
#define WEATHER_LON  0.0
#define WEATHER_CITY ""   // name shown on screen when you set the coordinates yourself
