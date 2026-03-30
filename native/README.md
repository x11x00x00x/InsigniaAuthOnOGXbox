# Native (NXDK) — Xbox Original

Builds **`default.xbe`** (QR on framebuffer) and **`default2.xbe`** (text-only), same HTTP flow as [`sdk/API.md`](../sdk/API.md).

## Layout

- Sources live in this directory; **mbed TLS** stays at repo root: `../third_party/mbedtls`
- **nxdk** is expected **next to the repo folder** (sibling), e.g. `../../nxdk` from `native/`

## Dependencies

- [nxdk](https://github.com/XboxDev/nxdk) (recursive clone)
- **mbed TLS** — clone into **`third_party/mbedtls`** (see [`../third_party/README.md`](../third_party/README.md))
- LLVM toolchain (e.g. Homebrew `llvm` on macOS)

## Build

From repository root:

```bash
eval "$(/path/to/nxdk/bin/activate -s)"
export NXDK_DIR=/path/to/nxdk   # must match sibling layout or edit native/Makefile
make
```

Or only native:

```bash
cd native
export NXDK_DIR=/path/to/nxdk
make
```

Outputs: **`native/bin/default.xbe`**, **`native/bin/default2.xbe`**, **`native/insignia_device_auth.iso`**.

## Configure host

Edit `AUTH_SERVER_HOST` in `main.c` / `main_text.c` if you point at another server.

## lwIP debug

`lwip_override/lwipopts.h` disables `LWIP_DEBUG` so lwIP does not print over the QR framebuffer.
