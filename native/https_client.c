/*
 * HTTPS POST over lwIP sockets + mbed TLS (TLS 1.2/1.3; ALPN http/1.1).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/build_info.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"

#include <hal/debug.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#define HTTP_HDR_MAX 8192

static void ssl_dbg(void *ctx, int level, const char *file, int line, const char *str)
{
    (void)ctx;
    (void)level;
    (void)file;
    (void)line;
    (void)str;
}

static int lwip_send_cb(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int r = send(fd, buf, len, 0);
    return r;
}

static int lwip_recv_cb(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int r = recv(fd, buf, len, 0);
    return r;
}

int https_post_json(const char *hostname, const char *port, const char *path, const char *json_body,
                    char *out, size_t out_sz)
{
    int ret;
    int fd = -1;
    struct addrinfo hints, *res = NULL, *rp;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    const char *pers = "insignia_nxdk";

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        debugPrint("getaddrinfo failed for %s\n", hostname);
        ret = -1;
        goto cleanup;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
            break;
        }
        closesocket(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    res = NULL;

    if (fd < 0) {
        debugPrint("TCP connect failed\n");
        ret = -2;
        goto cleanup;
    }

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)pers, strlen(pers));
    if (ret != 0) {
        debugPrint("ctr_drbg_seed failed: -0x%04x\n", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        debugPrint("ssl_config_defaults failed: -0x%04x\n", (unsigned int)-ret);
        goto cleanup;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_dbg(&conf, ssl_dbg, NULL);

#if defined(MBEDTLS_SSL_ALPN)
    {
        const char *alpn[] = { "http/1.1", NULL };
        ret = mbedtls_ssl_conf_alpn_protocols(&conf, alpn);
        if (ret != 0) {
            debugPrint("alpn failed: -0x%04x\n", (unsigned int)-ret);
            goto cleanup;
        }
    }
#endif

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        debugPrint("ssl_setup failed: -0x%04x\n", (unsigned int)-ret);
        goto cleanup;
    }

    ret = mbedtls_ssl_set_hostname(&ssl, hostname);
    if (ret != 0) {
        debugPrint("set_hostname failed: -0x%04x\n", (unsigned int)-ret);
        goto cleanup;
    }

    mbedtls_ssl_set_bio(&ssl, &fd, lwip_send_cb, lwip_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            debugPrint("handshake failed: -0x%04x\n", (unsigned int)-ret);
            goto cleanup;
        }
    }

    {
        char req[HTTP_HDR_MAX];
        int body_len = (int)strlen(json_body);
        int n = snprintf(req, sizeof(req),
                         "POST %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %d\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s",
                         path, hostname, body_len, json_body);
        if (n < 0 || n >= (int)sizeof(req)) {
            debugPrint("request too large\n");
            ret = -1;
            goto cleanup;
        }

        size_t written = 0;
        while (written < (size_t)n) {
            ret = mbedtls_ssl_write(&ssl, (unsigned char *)req + written, (size_t)n - written);
            if (ret <= 0) {
                if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                    continue;
                }
                debugPrint("ssl_write failed: -0x%04x\n", (unsigned int)-ret);
                goto cleanup;
            }
            written += (size_t)ret;
        }
    }

    {
        size_t total = 0;
        out[0] = '\0';
        while (total < out_sz - 1) {
            ret = mbedtls_ssl_read(&ssl, (unsigned char *)out + total, out_sz - 1 - total);
            if (ret == 0) {
                break;
            }
            if (ret < 0) {
                if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
                    continue;
                }
                if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                    break;
                }
                debugPrint("ssl_read failed: -0x%04x\n", (unsigned int)-ret);
                goto cleanup;
            }
            total += (size_t)ret;
        }
        out[total] = '\0';
    }

    ret = 0;

cleanup:
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (fd >= 0) {
        closesocket(fd);
    }
    return ret;
}
