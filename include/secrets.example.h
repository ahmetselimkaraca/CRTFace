// Copy this file to secrets.h (same folder) and fill in your Wi-Fi.
// secrets.h is git-ignored so your password never gets committed.
#pragma once

#define WIFI_SSID     "your-network-name"
#define WIFI_PASSWORD "your-password"

// Only for networks that ask for a username too (eduroam, university/work Wi-Fi).
// Leave empty for normal home Wi-Fi. For eduroam use your full address, e.g. "you@uni.edu".
#define WIFI_USERNAME ""

// Password for the device's own Wi-Fi network (AP_MODE in config.h). 8+ characters, pick your own:
// the control page has no login, so anyone who joins that network can drive the screen.
#define AP_PASSWORD "change-me-please"

// Optional: the song you're playing on Spotify, on the Music screen. One-time setup:
// 1. Sign in at https://developer.spotify.com/dashboard and click "Create app".
//    Redirect URI: http://127.0.0.1:8888/callback   API: Web API
// 2. Copy the app's Client ID and Client Secret into the two lines below.
// 3. On your computer, run:  python tools/spotify_token.py <CLIENT_ID> <CLIENT_SECRET>
//    It opens Spotify's sign-in page and prints the refresh token to paste below.
#define SPOTIFY_CLIENT_ID     ""
#define SPOTIFY_CLIENT_SECRET ""
#define SPOTIFY_REFRESH_TOKEN ""
