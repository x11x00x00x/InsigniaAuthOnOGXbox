# Third-party (native build)

## Mbed TLS (required for `native/`)

Clone into **`third_party/mbedtls/`** (this directory):

```bash
cd third_party
git clone --depth 1 --branch mbedtls-3.6.2 https://github.com/Mbed-TLS/mbedtls.git mbedtls
```

The NXDK Makefile expects **`../third_party/mbedtls`** relative to **`native/`**.

Do **not** commit the full `mbedtls` tree to git unless you intend to vendor it (large). Add `third_party/mbedtls/` to `.gitignore` if you clone locally.
