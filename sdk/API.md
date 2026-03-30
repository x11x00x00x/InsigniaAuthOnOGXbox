# Insignia device auth — HTTP API

The native Xbox client in this repository uses this contract against the Insignia auth backend.

**Base URL (production):** `https://auth.insigniastats.live`

## Start device session

`POST /api/auth/device`

- **Body:** `{}` (JSON)
- **Response (JSON):**
  - `device_code` — opaque string for polling
  - `user_code` — short code the user types or sees on screen
  - `verification_uri` — human page (e.g. `https://…/device-link.html`)
  - `verification_uri_complete` — URL that may include `?user_code=…` for QR encoding
  - `expires_in` — seconds
  - `interval` — suggested poll interval in seconds

## Poll until complete

`POST /api/auth/device/token`

- **Body (JSON):**
  - `grant_type`: `"urn:ietf:params:oauth:grant-type:device_code"`
  - `device_code`: value from start
- **Pending:** `{ "error": "authorization_pending" }` (HTTP 200)
- **Success:** same shape as interactive login, including `sessionKey`, `username`, `email` as applicable
- **Failure:** `{ "error": "…" }` (e.g. expired)

## Complete in browser (phone / PC)

`POST /api/auth/device/complete`

- **Body (JSON):** `user_code`, `email`, `password` (Insignia account)
- Used by `device-link.html` on the auth server.

## Pages

- **`/device-link.html`** — primary device linking page (canonical).
- **`/sdk/device-link.html`** — legacy path; may still be served for old links.

---

Implementations:

| Implementation | Location |
|----------------|----------|
| Native QR + poll | `native/main.c` → `bin/default.xbe` |
| Native text + poll | `native/main_text.c` → `bin/default2.xbe` |
