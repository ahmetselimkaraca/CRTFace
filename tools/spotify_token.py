"""One-time Spotify sign-in for the Music screen. Prints the refresh token for include/secrets.h.

Usage: python tools/spotify_token.py CLIENT_ID CLIENT_SECRET
The app's Redirect URI on developer.spotify.com must be exactly http://127.0.0.1:8888/callback
"""
import base64
import http.server
import json
import sys
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

REDIRECT = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    client_id, secret = sys.argv[1], sys.argv[2]
    url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode(
        {"client_id": client_id, "response_type": "code", "redirect_uri": REDIRECT, "scope": SCOPES})

    answer = {}

    class Callback(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            if "code" in q or "error" in q:
                answer.update({k: v[0] for k, v in q.items()})
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"Done - you can close this tab." if "code" in q else b"Waiting for Spotify...")

        def log_message(self, *args):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 8888), Callback)
    print("Opening Spotify's sign-in page. If it doesn't open, visit:\n" + url)
    webbrowser.open(url)
    while not answer:
        server.handle_request()
    if "error" in answer:
        sys.exit("Spotify said: " + answer["error"])

    request = urllib.request.Request(
        "https://accounts.spotify.com/api/token",
        data=urllib.parse.urlencode(
            {"grant_type": "authorization_code", "code": answer["code"], "redirect_uri": REDIRECT}).encode(),
        headers={"Authorization": "Basic " + base64.b64encode(f"{client_id}:{secret}".encode()).decode()})
    try:
        tokens = json.load(urllib.request.urlopen(request))
    except urllib.error.HTTPError as e:
        sys.exit(f"Spotify refused the sign-in ({e.code}): {e.read().decode(errors='replace')}")
    print("\nPaste this into include/secrets.h:\n")
    print(f'#define SPOTIFY_REFRESH_TOKEN "{tokens["refresh_token"]}"')


if __name__ == "__main__":
    main()
