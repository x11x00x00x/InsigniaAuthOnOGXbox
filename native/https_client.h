#ifndef HTTPS_CLIENT_H
#define HTTPS_CLIENT_H

#include <stddef.h>

int https_post_json(const char *hostname, const char *port, const char *path, const char *json_body,
                    char *out, size_t out_sz);

#endif
