/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    media.c
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-11-30 23:32:40
 */

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <pthread.h>

#include "../../src/nuttx/nxipc.h"

#define SERVER_WORKERS_MAX 1
#define CLIENT_WORKERS_MAX 1
#define CLIENT_SEND_COUND_MAX 50

bool verbose = false;

#define media_log(fmt, args...)  do { if(verbose) fprintf(stderr, fmt, ## args); } while(0)

typedef struct {
    int cmd;
    int arg1;
    int arg2;
} player_info_t;

typedef struct media_player_ctx {
    void* player;
    char* name;
    struct media_player_ctx* next;
}media_player_ctx_t;

typedef struct {
    void* server;
    void* notifier;
    pthread_t tid;
    media_player_ctx_t* player_list;
    bool interrupted;
    int interval;
} media_server_ctx_t;

typedef struct {
    const char* name;
    pthread_t tid;
    void* server_proxy;
    void* cb_proxy;
    void* handle;
    bool interrupted;
    int interval;
} player_t;

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


static void media_server_player_notify_info(media_server_ctx_t* ctx, media_player_ctx_t* player) {
    nxparcel* parcel;
    player_info_t info = {
        .cmd = 1,
        .arg1 = 2,
        .arg2 = 3
    };
    nxparcel_alloc(&parcel);
    if (parcel != NULL) {
        nxparcel_append(parcel, &info, sizeof(player_info_t));
    }
    nxipc_pub_topic_msg(ctx->notifier, player->name, strlen(player->name), parcel);
    nxparcel_free(parcel);
}

static void* media_server_worker(void* arg)
{
    int cnt = 0;
    media_server_ctx_t* ctx = (media_server_ctx_t*)arg;

    prctl(PR_SET_NAME, "pub_worker", NULL, NULL, NULL);

    if (ctx == NULL) {
        return NULL;
    }

    sleep(1);

    while (!ctx->interrupted) {
        media_player_ctx_t* player = ctx->player_list;
        while(player != NULL) {
            media_server_player_notify_info(ctx, player);
            player = player->next;
        }

        usleep(ctx->interval);
        cnt++;
    }

    media_log("%s.cnt=%d\n", __func__, cnt);
    return NULL;
}

static void* media_server_player_create(media_server_ctx_t* ctx, const char* name) {
    media_player_ctx_t* player = calloc(1, sizeof(media_player_ctx_t));
    media_player_ctx_t** head = &ctx->player_list;
    if (player == NULL) {
        return NULL;
    }
    player->player = calloc(1, sizeof(void*));
    player->name = strdup(name);

    while(*head != NULL) {
        head = &((*head)->next);
    }

    *head = player;

    return player;
}

static int media_server_on_transaction(const void* cookie, const int code, const nxparcel* parcel, nxparcel** out)
{
    int ret = 0;

    switch (code) {
    case CREATE:
        if (parcel != NULL) {
            char* name = (char*)nxparcel_data(parcel);
            void* player = media_server_player_create((media_server_ctx_t*)cookie, name);
            media_log("media_server.create.name=%s, player=%p\n", name, player);
            if (player != NULL) {
                nxparcel_alloc(out);
                if (*out != NULL) {
                    nxparcel_append(*out, &player, sizeof(void*));
                }
            }
        }

        break;

    case SET_DATA_SOURCE:
        if (parcel != NULL && nxparcel_size(parcel) > 0) {
            media_log("media_server.set_data_source.url=%s\n", (char*)nxparcel_data(parcel));
        }

        break;

    default:
        break;
    }

    return ret;
}

static int media_player_listener(const void* thiz, const void* topic, const size_t topic_len, const nxparcel* parcel) {
    player_t* player = (player_t*)thiz;
    player_info_t* info = (player_info_t*)nxparcel_data(parcel);
    if (strncmp(player->name, (char*)topic, sizeof(player->name) != 0)) {
        media_log("%s.player=%s, topic=%s\n", __func__, player->name, (char*)topic);
    } else if (info != NULL){
        media_log("%s.%s.topic=%s, .cmd=%d, .arg1=%d, .arg2=%d\n", __func__, player->name, (char*)topic, info->cmd, info->arg1, info->arg2);
    }
}

static inline void* get_media_server_proxy(void) {
    return nxipc_client_connect("mediaserver");
}

static inline void* get_media_server_cb_proxy(player_t* player) {
    return nxipc_sub_connect("mediapub", media_player_listener, player);
}

static void media_player_create(player_t* player) {
    void* handle = NULL;
    nxparcel* parcel = NULL;

    player->server_proxy = get_media_server_proxy();
    player->cb_proxy = get_media_server_cb_proxy(player);
    nxipc_sub_register_topic(player->cb_proxy, player->name, strlen(player->name));

    nxparcel_alloc(&parcel);
    nxparcel_append(parcel, player->name, strlen(player->name) + 1);

    nxipc_client_transaction(player->server_proxy, CREATE, parcel, parcel);

    nxparcel_skip(parcel, strlen(player->name) + 1);
    player->handle = nxparcel_data(parcel);

    media_log("%s.name=%s, player=%p\n", __func__, player->name, player->handle);
    nxparcel_free(parcel);
    return;
}

static int media_player_set_data_source(player_t *player, const char* url) {
    nxparcel* parcel = NULL;
    nxparcel_alloc(&parcel);
    if (parcel != NULL) {
        nxparcel_append(parcel, url, strlen(url) + 1);
    }
    int rc = nxipc_client_transaction(player->server_proxy, SET_DATA_SOURCE, parcel, NULL);
    nxparcel_free(parcel);
    return rc;
}

static void* player_worker(void* arg) {
    int cnt = 0;
    player_t *player = (player_t*)arg;

    prctl(PR_SET_NAME, player->name, NULL, NULL, NULL);

    media_player_create(player);


    while(!player->interrupted) {
        uint64_t start = milliseconds();
        int ret = media_player_set_data_source(player, "http://xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx/yyyyyyyyyyyyyyyyyyyyyyyyyyyy/zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz/253.mp3");
        if (ret != 0) {
            media_log("%s.%s.latency=%lu\n", __func__, player->name, milliseconds() - start);
        }
        usleep(player->interval);
        cnt++;
    }

    media_log("%s.%s.cnt=%d\n", __func__, player->name, cnt);
    return NULL;
}

static int media_client_start(player_t client[], const int cnt)
{
    int ret = 0;
    for(int i = 0; i < cnt; i++) {
        ret = pthread_create(&client[i].tid, NULL, player_worker, &client[i]);
        if (ret != 0) {
            media_log("%s.player.%s.failed\n", __func__, client[i].name);
            break;
        }
    }

    return ret;
}

static void inline media_player_destroy(player_t *player) {
    player->interrupted = true;
    media_log("%s.%s.enter\n", __func__, player->name);
    pthread_join(player->tid, NULL);
    nxipc_client_disconnect(player->server_proxy);
    nxipc_sub_unregister_topic(player->cb_proxy, player->name, strlen(player->name));
    nxipc_sub_disconnect(player->cb_proxy);
    media_log("%s.%s.exit\n", __func__, player->name);
}

static int media_client_stop(player_t client[], const int cnt) {
    media_log("%s.enter\n", __func__);
    for (int i = 0; i < cnt; i++) {
        media_player_destroy(&client[i]);
    }
    media_log("%s.exit\n", __func__);
}

int media_server_start(media_server_ctx_t* ctx)
{
    ctx->server = nxipc_server_create("mediaserver");
    if (ctx->server == NULL) {
        return -1;
    }

    ctx->notifier = nxipc_pub_create("mediapub");
    if (ctx->notifier == NULL) {
        nxipc_server_release(ctx->server);
        ctx->server = NULL;
        return -2;
    }

    nxipc_server_set_transaction_cb(ctx->server, media_server_on_transaction, ctx);
    pthread_create(&ctx->tid, NULL, media_server_worker, ctx);
}

int media_server_stop(media_server_ctx_t *ctx) {
    media_log("%s.enter\n", __func__);
    ctx->interrupted = true;
    pthread_join(ctx->tid, NULL);
    nxipc_server_release(ctx->server);
    nxipc_pub_release(ctx->notifier);
    media_log("%s.exit\n", __func__);
}

static void inline usage(const char* progname) {
    printf("Usage: %s -t seconds -i interval_ms -r reqrep_mode -p subpub_mode -v log_verbose -h help\n", progname);
    printf("Usage: %s -t 10 -i 1000000 -r -p -v\n", progname);
}

int main(int argc, char* const argv[])
{
    int ch = 0;
    player_t player[3];
    media_server_ctx_t media_server;
    bool reqrep = false;
    bool pubsub = false;
    int interval = 10000; //ms
    int duration = 30; //seconds

    while ((ch = getopt(argc, argv, "t:i:vrph")) != -1) {
        switch (ch) {
        case 'v':
            verbose = true;
            break;
        case 't':
            duration = atoi(optarg);
            media_log("duration=%d\n", duration);
            break;

        case 'i':
            interval = atoi(optarg);;
            media_log("interval=%d\n", interval);
            break;

        case 'r':
            reqrep = true;
            media_log("reqrep=%d\n", reqrep);
            break;
        case 'p':
            pubsub = true;
            media_log("pubsub=%d\n", pubsub);
            break;
        case 'h':
        case '?':
        default:
            usage(argv[0]);
            return 0;
        }
    }

    if (!reqrep && !pubsub) {
        usage(argv[0]);
        return 0;
    }

    // player init
    memset(player, 0, sizeof(player));
    player[2].name = "wakeup";
    player[0].name = "tts";
    player[1].name = "music";
    player[0].interval = interval;
    player[1].interval = interval;
    player[2].interval = interval;
    if (!reqrep) {
        player[0].interrupted = true;
        player[1].interrupted = true;
        player[2].interrupted = true;
    }

    //server init
    memset(&media_server, 0, sizeof(media_server));
    media_server.interval = interval;
    if (!pubsub) {
        media_server.interrupted = true;
    }

    if (!pubsub && !reqrep) {
        duration = 0;
    }

    media_server_start(&media_server);
    media_client_start(player, 3);
    while(duration > 0) {
        if (duration > 100) {
            sleep(100);
        } else {
            sleep(duration);
        }
        duration -= 100;
    }
    media_server_stop(&media_server);
    media_client_stop(player, 3);
    exit(EXIT_SUCCESS);
}
