# XboxQRCodeLogin

Insignia **device authorization** for the **Original Xbox**: show a **user code** and **QR (or link)** on the console, complete sign-in in a browser, receive a **`sessionKey`**. Same OAuth2-style **device flow** as common “TV/device login” patterns.

**Production API host:** `https://auth.insigniastats.live`


---NOTE THIS IS AN UNOFFICIAL LOGIN MOD NOT CREATED BY THE INSIGNIA TEAM.

## What’s in this repo

| Area | Purpose |
|------|---------|
| **[`sdk/API.md`](sdk/API.md)** | **Canonical HTTP contract** — the native client implements these endpoints. |
| **[`native/`](native/)** | **NXDK** homebrew: builds **`default.xbe`** (framebuffer QR) and **`default2.xbe`** (text-only). |
| **`third_party/mbedtls/`** | TLS for **native** (clone per [`third_party/README.md`](third_party/README.md); not vendored in git by default). |

---

## How the flow works

1. **Start session** — `POST /api/auth/device` → `device_code`, `user_code`, `verification_uri_complete`, etc.  
2. **Show** the user code + QR (or URL) from this SDK.  
3. User opens **`/device-link.html`** on a phone/PC and signs in with Insignia credentials.  
4. **Poll** — `POST /api/auth/device/token` until `sessionKey` (and profile fields) are returned.

Full field list and errors: **[`sdk/API.md`](sdk/API.md)**.

---

## Native SDK (NXDK / Original Xbox)

- **Sources:** [`native/`](native/) — `main.c` (QR), `main_text.c` (text), `https_client.c`, Nayuki `qrcodegen`, mbed TLS config.  
- **Build doc:** [`native/README.md`](native/README.md)  
- **Outputs:** `native/bin/default.xbe`, `native/bin/default2.xbe`, optional `.iso`.

### Build (outline)

```bash
# Mbed TLS (once)
git clone --depth 1 --branch mbedtls-3.6.2 https://github.com/Mbed-TLS/mbedtls.git third_party/mbedtls

git clone --recursive https://github.com/XboxDev/nxdk.git
# Place this repo as a sibling of nxdk, or set NXDK_DIR
export NXDK_DIR=/path/to/nxdk
eval "$(/path/to/nxdk/bin/activate -s)"
make
```

Details: [`native/README.md`](native/README.md), [`third_party/README.md`](third_party/README.md).

---

## Configuration

| Setting | Where |
|--------|--------|
| Auth host | `AUTH_SERVER_HOST` in `main.c` / `main_text.c`. |
| TLS verify | See `https_client.c`. |

---

## Third-party

- **Native:** [nxdk](https://github.com/XboxDev/nxdk), [Mbed TLS](https://github.com/Mbed-TLS/mbedtls), [Nayuki QR Code generator](https://github.com/nayuki/QR-Code-generator) (C).

---

## License

This project’s **original code** is provided as-is for integration with Insignia. Third-party libraries retain their own licenses (see `third_party/`).
