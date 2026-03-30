/*
 * Insignia device login demo for NXDK (Original Xbox).
 * Polls auth.insigniastats.live device flow over HTTPS; draws QR on the framebuffer.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hal/debug.h>
#include <hal/video.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <nxdk/net.h>

#include "https_client.h"
#include "qrcodegen.h"

#define AUTH_SERVER_HOST "auth.insigniastats.live"
#define AUTH_SERVER_PORT_HTTPS "443"

#define JSON_BUF_SIZE 16384

extern struct netif *g_pnetif;

static void delay_rough_ms(unsigned ms)
{
    for (unsigned j = 0; j < ms; j++) {
        for (volatile unsigned i = 0; i < 80000u; i++) {
        }
    }
}

static const char *http_body_json(const char *resp)
{
    const char *p = strstr(resp, "\r\n\r\n");
    if (!p) {
        return resp;
    }
    return p + 4;
}

static int json_extract_string(const char *json, const char *key, char *out, size_t outsz)
{
    char prefix[80];
    snprintf(prefix, sizeof(prefix), "\"%s\":\"", key);
    const char *p = strstr(json, prefix);
    if (!p) {
        return -1;
    }
    p += strlen(prefix);
    size_t i = 0;
    while (*p && *p != '"' && i < outsz - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

static int json_extract_int(const char *json, const char *key, int *out)
{
    char prefix[80];
    snprintf(prefix, sizeof(prefix), "\"%s\":", key);
    const char *p = strstr(json, prefix);
    if (!p) {
        return -1;
    }
    p += strlen(prefix);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    *out = atoi(p);
    return 0;
}

/* Xbox framebuffer: 32-bit X8R8G8B8 (see nxdk samples/gamma). */
#define FB_WHITE 0x00FFFFFFu
#define FB_BLACK 0x00000000u
#define FB_BG    0x00F5F5F5u

static void fb_fill(uint32_t color)
{
    VIDEO_MODE vm = XVideoGetMode();
    volatile uint32_t *fb = (volatile uint32_t *)XVideoGetFB();
    unsigned int W = (unsigned int)vm.width;
    unsigned int H = (unsigned int)vm.height;
    for (unsigned int y = 0; y < H; y++) {
        for (unsigned int x = 0; x < W; x++) {
            fb[y * W + x] = color;
        }
    }
}

/** Draw QR as large square pixels on the main framebuffer (scannable with a phone). */
static void draw_qr_to_framebuffer(const uint8_t qrcode[], int size)
{
    /* Clear any prior debug text (same buffer as hal/debug). */
    debugClearScreen();

    VIDEO_MODE vm = XVideoGetMode();
    volatile uint32_t *fb = (volatile uint32_t *)XVideoGetFB();
    int W = vm.width;
    int H = vm.height;

    /* Quiet zone is included via qrcodegen_getModule at -1 .. size. */
    int modules = size + 2;
    int margin = 32;
    int max_side = (W < H ? W : H) - margin * 2;
    if (max_side < 160) {
        max_side = (W < H ? W : H) - 16;
    }

    int cell = max_side / modules;
    if (cell < 5) {
        cell = 5;
    }
    if (cell > 18) {
        cell = 18;
    }

    int qr_px = modules * cell;
    int ox = (W - qr_px) / 2;
    int oy = (H - qr_px) / 2 - 24;

    fb_fill(FB_BG);

    for (int qy = -1; qy <= size; qy++) {
        for (int qx = -1; qx <= size; qx++) {
            bool on = qrcodegen_getModule(qrcode, qx, qy);
            uint32_t pixel = on ? FB_BLACK : FB_WHITE;
            int x0 = ox + (qx + 1) * cell;
            int y0 = oy + (qy + 1) * cell;
            for (int dy = 0; dy < cell; dy++) {
                for (int dx = 0; dx < cell; dx++) {
                    int px = x0 + dx;
                    int py = y0 + dy;
                    if (px >= 0 && px < W && py >= 0 && py < H) {
                        fb[(unsigned)py * (unsigned)W + (unsigned)px] = pixel;
                    }
                }
            }
        }
    }

    /* Thick black border around the symbol (helps cameras and composite). */
    for (int t = 0; t < 3; t++) {
        int L = ox - 1 - t;
        int R = ox + qr_px - 1 + 1 + t;
        int T = oy - 1 - t;
        int B = oy + qr_px - 1 + 1 + t;
        for (int x = L; x <= R; x++) {
            if (x >= 0 && x < W) {
                if (T >= 0 && T < H) {
                    fb[(unsigned)T * (unsigned)W + (unsigned)x] = FB_BLACK;
                }
                if (B >= 0 && B < H) {
                    fb[(unsigned)B * (unsigned)W + (unsigned)x] = FB_BLACK;
                }
            }
        }
        for (int y = T; y <= B; y++) {
            if (y >= 0 && y < H) {
                if (L >= 0 && L < W) {
                    fb[(unsigned)y * (unsigned)W + (unsigned)L] = FB_BLACK;
                }
                if (R >= 0 && R < W) {
                    fb[(unsigned)y * (unsigned)W + (unsigned)R] = FB_BLACK;
                }
            }
        }
    }

    XVideoFlushFB();
}

static int encode_and_show_qr(const char *text)
{
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    if (!qrcodegen_encodeText(text, temp, qrcode, qrcodegen_Ecc_LOW, 1, 40, qrcodegen_Mask_AUTO,
                              true)) {
        debugPrint("QR encode failed\n");
        return -1;
    }
    int size = qrcodegen_getSize(qrcode);
    draw_qr_to_framebuffer(qrcode, size);
    return 0;
}

int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    nxNetInit(NULL);

    for (int wait = 0; wait < 90; wait++) {
        if (g_pnetif && netif_ip4_addr(g_pnetif)->addr != 0) {
            break;
        }
        delay_rough_ms(1000);
    }
    if (!g_pnetif || netif_ip4_addr(g_pnetif)->addr == 0) {
        debugPrint("No IP — check Ethernet. Halting.\n");
        for (;;) {
        }
    }

    static char resp[JSON_BUF_SIZE];
    memset(resp, 0, sizeof(resp));
    const char *start_body = "{}";
    if (https_post_json(AUTH_SERVER_HOST, AUTH_SERVER_PORT_HTTPS, "/api/auth/device", start_body, resp,
                        sizeof(resp)) != 0) {
        debugPrint("Device start HTTPS failed.\n");
        for (;;) {
        }
    }

    const char *json = http_body_json(resp);
    char device_code[128];
    char user_code[32];
    char verification_uri_complete[512];
    int interval_sec = 5;

    if (json_extract_string(json, "device_code", device_code, sizeof(device_code)) != 0 ||
        json_extract_string(json, "user_code", user_code, sizeof(user_code)) != 0) {
        debugPrint("Bad JSON from /api/auth/device:\n%s\n", json);
        for (;;) {
        }
    }
    if (json_extract_string(json, "verification_uri_complete", verification_uri_complete,
                            sizeof(verification_uri_complete)) != 0) {
        debugPrint("missing verification_uri_complete\n");
        for (;;) {
        }
    }
    json_extract_int(json, "interval", &interval_sec);
    if (interval_sec < 1) {
        interval_sec = 5;
    }

    if (encode_and_show_qr(verification_uri_complete) != 0) {
        for (;;) {
        }
    }
    /* Do not debugPrint while the QR is shown: hal/debug draws into the same framebuffer and would cover the code. */

    for (;;) {
        delay_rough_ms((unsigned)interval_sec * 1000u);

        char body[512];
        snprintf(body, sizeof(body), "{\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\",\"device_code\":\"%s\"}",
                 device_code);

        memset(resp, 0, sizeof(resp));
        if (https_post_json(AUTH_SERVER_HOST, AUTH_SERVER_PORT_HTTPS, "/api/auth/device/token", body, resp,
                            sizeof(resp)) != 0) {
            continue;
        }

        json = http_body_json(resp);
        char err[64];
        if (json_extract_string(json, "error", err, sizeof(err)) == 0) {
            if (strcmp(err, "authorization_pending") == 0) {
                continue;
            }
            debugClearScreen();
            debugPrint("Error: %s\n", err);
            if (strstr(json, "expired")) {
                break;
            }
            continue;
        }

        char session_key[128];
        char username[256];
        if (json_extract_string(json, "sessionKey", session_key, sizeof(session_key)) == 0) {
            json_extract_string(json, "username", username, sizeof(username));
            debugClearScreen();
            debugPrint("\n*** LOGGED IN ***\n");
            debugPrint("username: %s\n", username);
            debugPrint("sessionKey: %s\n", session_key);
            debugPrint("Use X-Session-Key header on API calls.\n");
            break;
        }

        debugClearScreen();
        debugPrint("Unexpected: %s\n", json);
    }

    for (;;) {
    }
    return 0;
}
