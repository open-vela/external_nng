/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    reqrep.c
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-11-30 23:33:02
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "../../src/nuttx/nxipc.h"

#define SERVER_WORKERS_MAX 1
#define CLIENT_WORKERS_MAX 1
#define CLIENT_SEND_COUND_MAX 50

typedef enum {
    CREATE = 0,
    SET_DATA_SOURCE,
    PREPARE,
    START,
    PAUSE,
    STOP,
    RELEASE,
    ISPLAYING
} media_trans_type_t;


static uint64_t milliseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((uint64_t)tv.tv_sec * 1000) + ((uint64_t)tv.tv_usec / 1000));
}


static int media_server_on_transaction(const void* cookie, const int code, const nxparcel* parcel, nxparcel** out)
{
    int ret = 0;

    switch (code) {
    case CREATE:
        if (parcel != NULL) {
            char* name = (char*)nxparcel_data(parcel);
            fprintf(stderr, "media_server.create.name=%s\n", name);
            nxparcel_alloc(out);
            if (*out != NULL) {
                nxparcel_append(*out, "created", strlen("created") + 1);
            }
        }
        break;

    case SET_DATA_SOURCE:
        if (parcel != NULL) {
            fprintf(stdout, "media_server.set_data_source.url=%s\n", (char*)nxparcel_data(parcel));
        }

        break;

    case PREPARE:
        fprintf(stdout, "media_server.prepare\n");
        break;

    case START:
        fprintf(stdout, "media_server.start\n");
        break;

    case PAUSE:
        fprintf(stdout, "media_server.pause\n");
        break;

    case STOP:
        fprintf(stdout, "media_server.stop\n");
        break;

    case RELEASE:
        fprintf(stdout, "media_server.release\n");
        break;

    case ISPLAYING:
        fprintf(stdout, "media_server.isplaying\n");
        nxparcel_alloc(out);
        if (*out != NULL) {
            nxparcel_append_u32(*out, -1);
        }

        break;

    default:
        break;
    }

    return ret;
}

int client(const char* url, const char* name)
{
    int rc = 0;
    void* client = nxipc_client_connect(url);

    if (client != NULL) {
        nxparcel* parcel;
        int  is_playing = 0;
        fprintf(stderr, "client connected.\n");
        nxparcel_alloc(&parcel);
        nxparcel_append(parcel, name, strlen(name) + 1);
        rc = nxipc_client_transaction(client, CREATE, parcel, parcel);

        if (rc != 0) {
            fprintf(stdout, "player.name=%s, create.error=%d\n", name, rc);
        } else {
            nxparcel_skip(parcel, (size_t)(strlen(name) + 1));
            fprintf(stdout, "player.name=%s, create.out=%s\n", name, (char*)nxparcel_data(parcel));
        }
        nxparcel_clear(parcel);
        nxparcel_append(parcel, "http://253.mp3", strlen("http://253.mp3") + 1);
        rc = nxipc_client_transaction(client, SET_DATA_SOURCE, parcel, NULL);

        if (rc != 0) {
            fprintf(stdout, "player.name=%s, set_data_source.error=%d\n", name, rc);
        }

        rc = nxipc_client_transaction(client, PREPARE, NULL, NULL);

        if (rc != 0) {
            fprintf(stdout, "player.name=%s, prepare.error=%d\n", name, rc);
        }

        rc = nxipc_client_transaction(client, START, NULL, NULL);

        if (rc != 0) {
            fprintf(stdout, "player.name=%s, start.error=%d\n", name, rc);
        }

        nxparcel_clear(parcel);
        rc = nxipc_client_transaction(client, ISPLAYING, NULL, parcel);

        if (rc != 0) {
            fprintf(stdout, "player.name=%s, isplaying.error=%d\n", name, rc);
        } else {
            nxparcel_read_u32(parcel, &is_playing);
            fprintf(stdout, "player.name=%s, isplaying=%d\n", name, is_playing);
        }
    }

    nxipc_client_disconnect(client);
    return rc;
}

int main(int argc, char** argv)
{
    int rc;
    bool inproc = false;
    void* server = NULL;
    const char* name = NULL;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <url> [-s|name]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    inproc = (strstr(argv[1], "inproc") != NULL);
    name = strstr(argv[1], "//") + 2;

    if (inproc) {
        server = nxipc_server_create(name);

        if (server == NULL) {
            fprintf(stderr, "server start failed\n");
        } else {
            fprintf(stderr, "server start success\n");
            nxipc_server_set_transaction_cb(server, media_server_on_transaction, server);
            rc = client(name, argv[2]);

            if (rc != 0) {
                fprintf(stderr, "client start failed, ret=%d\n", rc);
            }
        }
    } else {
        if (strcmp(argv[2], "-s") == 0) {
            server = nxipc_server_create(name);

            if (server == NULL) {
                fprintf(stderr, "server start failed\n");
            } else {
                nxipc_server_set_transaction_cb(server, media_server_on_transaction, server);
            }
        } else {
            rc = client(name, argv[2]);
        }
    }

    if (server != NULL) {
        nxipc_server_release(server);
    }

    exit(rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
