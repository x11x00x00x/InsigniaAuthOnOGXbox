/*
 * Insignia device login — text-only NXDK build (no QR framebuffer).
 * Shows device user code + website URL; same HTTPS device flow as main.c.
 */

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

int main(void)
{
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    debugPrint("\n\n=== Insignia device login (text) ===\n");
    debugPrint("https://%s/api\n\n", AUTH_SERVER_HOST);

    nxNetInit(NULL);

    for (int wait = 0; wait < 90; wait++) {
        if (g_pnetif && netif_ip4_addr(g_pnetif)->addr != 0) {
            break;
        }
        delay_rough_ms(1000);
        debugPrint("Waiting for DHCP...\n");
    }
    if (!g_pnetif || netif_ip4_addr(g_pnetif)->addr == 0) {
        debugPrint("No IP — check Ethernet. Halting.\n");
        for (;;) {
        }
    }
    debugPrint("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(g_pnetif)));

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
    char verification_uri[512];
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
    if (json_extract_string(json, "verification_uri", verification_uri, sizeof(verification_uri)) != 0) {
        snprintf(verification_uri, sizeof(verification_uri), "https://%s/device-link.html", AUTH_SERVER_HOST);
    }
    json_extract_int(json, "interval", &interval_sec);
    if (interval_sec < 1) {
        interval_sec = 5;
    }

    debugPrint("\n");
    debugPrint("-------- ENTER THIS CODE --------\n");
    debugPrint("  %s\n", user_code);
    debugPrint("---------------------------------\n\n");
    debugPrint("Open this website on your phone or PC:\n");
    debugPrint("  %s\n\n", verification_uri);
    debugPrint("Or open this link (code may be filled in):\n");
    debugPrint("  %s\n\n", verification_uri_complete);

    debugPrint("Polling every %d s...\n", interval_sec);

    for (;;) {
        delay_rough_ms((unsigned)interval_sec * 1000u);

        char body[512];
        snprintf(body, sizeof(body), "{\"grant_type\":\"urn:ietf:params:oauth:grant-type:device_code\",\"device_code\":\"%s\"}",
                 device_code);

        memset(resp, 0, sizeof(resp));
        if (https_post_json(AUTH_SERVER_HOST, AUTH_SERVER_PORT_HTTPS, "/api/auth/device/token", body, resp,
                            sizeof(resp)) != 0) {
            debugPrint("token poll HTTPS error\n");
            continue;
        }

        json = http_body_json(resp);
        char err[64];
        if (json_extract_string(json, "error", err, sizeof(err)) == 0) {
            if (strcmp(err, "authorization_pending") == 0) {
                debugPrint("...\n");
                continue;
            }
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
            debugPrint("\n*** LOGGED IN ***\n");
            debugPrint("username: %s\n", username);
            debugPrint("sessionKey: %s\n", session_key);
            debugPrint("Use X-Session-Key header on API calls.\n");
            break;
        }

        debugPrint("Unexpected: %s\n", json);
    }

    for (;;) {
    }
    return 0;
}
