/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    nxipc.c
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-11-30 13:38:44
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <nng/nng.h>
#include <nng/compat/nanomsg/nn.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

#include "nxipc.h"

#define NNG_NUTTX_TOPIC_NAME_LEN 15
#define NNG_NUTTX_SEND_TIMEOUT_MS 200
#define NNG_NUTTX_RECV_TIMEOUT_MS 200

typedef enum {
    NNG_NUTTX_MODE_REQREP = 0,
    NNG_NUTTX_MODE_PIPELINE,
    NNG_NUTTX_MODE_PAIR,

    NNG_NUTTX_MODE_MAX
} nng_nuttx_mode_t;

typedef enum {
    NNG_NUTTX_TRANS_TYPE_INPROC = 0,
    NNG_NUTTX_TRANS_TYPE_IPC,
    NNG_NUTTX_TRANS_TYPE_TCP,

    NNG_NUTTX_TRANS_TYPE_MAX
} nng_nuttx_trans_type_t;

typedef enum {
    NNG_NUTTX_INIT_RECV,
    NNG_NUTTX_RECV_RET_SEND,
    NNG_NUTTX_SEND_RET_RECV,
    NNG_NUTTX_RECV_RET_RECV,
} nng_nuttx_aio_state_t;

typedef struct nng_trans_hdr {
    int seq;
    int op_code;
    size_t len;
    unsigned char* data[];
} nng_trans_hdr_t;

typedef struct nng_server_ctx {
    nng_nuttx_aio_state_t state;
    char* name;
    nng_socket fd;
    nng_aio* aio;
    nng_ctx  nng;
    on_transaction on_trans_cb;
    void* priv;
} nng_server_ctx_t;

typedef struct nng_client_ctx {
    char* name;
    nng_socket fd;
} nng_client_ctx_t;

typedef struct nng_pub_ctx {
    char* name;
    nng_socket fd;
} nng_pub_ctx_t;

typedef struct nng_sub_ctx {
    nng_nuttx_aio_state_t state;
    char* name;
    nng_socket fd;
    nng_aio* aio;
    nng_ctx  nng;
    on_topic_listener listener;
    void* priv;
} nng_sub_ctx_t;

typedef struct nng_nuttx_topic {
    uint8_t topic[NNG_NUTTX_TOPIC_NAME_LEN + 1];
    size_t content_len;
    uint8_t content[];
} nng_nuttx_topic_t;


const char* const nng_nuttx_trans_prefix_str[] = {
    "inproc://", "ipc://", "tcp://"
};

#define nxipc_log(fmt, args...)  do { fprintf(stderr, fmt, ## args); } while(0)

static inline void* nxipc_calloc(size_t size)
{
    return calloc(1, size);
}

static inline void nxipc_free(void* ptr)
{
    free(ptr);
}

static void nng_server_worker(void* arg)
{
    int ret = 0;
    nng_server_ctx_t* ctx = (nng_server_ctx_t*)arg;

    if (NULL == ctx) {
        return;
    }

    switch (ctx->state) {
    case NNG_NUTTX_INIT_RECV:
        ctx->state = NNG_NUTTX_RECV_RET_SEND;
        nng_ctx_recv(ctx->nng, ctx->aio);
        break;

    case NNG_NUTTX_RECV_RET_SEND: {
        nng_msg* msg;
        nxparcel* parcel = NULL;
        nng_trans_hdr_t* hdr;
        uint32_t seq, op_code, len;

        if ((ret = nng_aio_result(ctx->aio)) != 0) {
            if (ret == NNG_ETIMEDOUT) {
                nng_ctx_recv(ctx->nng, ctx->aio);
            } else {
                nxipc_log("%s: nng_aio_result.error=%d(%s)\n", __func__, ret, nng_strerror(ret));
            }

            break;
        }

        msg = nng_aio_get_msg(ctx->aio);

        if (msg == NULL || nng_msg_body(msg) == NULL) {
            nng_ctx_recv(ctx->nng, ctx->aio);
            break;
        }

        // parse & execute
        hdr = (nng_trans_hdr_t*)nng_msg_body(msg);
        ret = nng_msg_trim(msg, sizeof(nng_trans_hdr_t));

        if (ret != 0 || nng_msg_len(msg) != hdr->len) {
            hdr->op_code = -EINVAL;
        } else {
            if (ctx->on_trans_cb != NULL) {
                hdr->op_code = ctx->on_trans_cb(ctx->priv, hdr->op_code, msg, &parcel);
            } else {
                hdr->op_code = -EINVAL;
            }
        }

        // send response
        nng_msg_clear(msg);
        ret = nng_msg_append(msg, hdr, sizeof(nng_trans_hdr_t));

        if (ret == 0) {
            hdr = (nng_trans_hdr_t*)nng_msg_body(msg);

            if (parcel != NULL && (hdr->len = nng_msg_len(parcel)) > 0) {
                nng_msg_append(msg, nng_msg_body(parcel), hdr->len);
                nng_msg_free(parcel);
            } else {
                hdr->len = 0;
            }
        }

        ctx->state = NNG_NUTTX_SEND_RET_RECV;
        nng_aio_set_msg(ctx->aio, msg);
        nng_ctx_send(ctx->nng, ctx->aio);
    }
    break;

    case NNG_NUTTX_SEND_RET_RECV:
        if ((ret = nng_aio_result(ctx->aio)) != 0) {
            nng_msg_free(nng_aio_get_msg(ctx->aio));
            nxipc_log("%s: nng_aio_result=%d", __func__, ret);
        }

        ctx->state = NNG_NUTTX_RECV_RET_SEND;
        nng_ctx_recv(ctx->nng, ctx->aio);
        break;

    default:
        nxipc_log("%s: bad state %d", __func__, ctx->state);
        break;
    }

    return;
}

void* nxipc_server_create(const char* name)
{
    nng_server_ctx_t* ctx = NULL;
    const nng_nuttx_trans_type_t trans_type = NNG_NUTTX_TRANS_TYPE_INPROC;
    int name_len = strlen(name);
    int ret = 0;

    if (name_len <= 0) {
        ret = -EINVAL;
        goto err;
    }

    ctx = nxipc_calloc(sizeof(nng_server_ctx_t));

    if (ctx == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    memset(ctx, 0, sizeof(nng_server_ctx_t));

    ctx->name = nxipc_calloc(strlen(nng_nuttx_trans_prefix_str[trans_type]) + name_len + 1);

    if (ctx->name == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    strcpy(ctx->name, nng_nuttx_trans_prefix_str[trans_type]);
    strcat(ctx->name, name);

    if ((ret = nng_rep0_open(&ctx->fd)) != 0) {
        goto err;
    }

    if ((ret = nng_listen(ctx->fd, ctx->name, NULL, 0)) != 0) {
        goto err;
    }

    nng_setopt_ms(ctx->fd, NNG_OPT_RECVTIMEO, NNG_NUTTX_RECV_TIMEOUT_MS);
    nng_setopt_ms(ctx->fd, NNG_OPT_SENDTIMEO, NNG_NUTTX_SEND_TIMEOUT_MS);

    if ((ret = nng_aio_alloc(&ctx->aio, nng_server_worker, ctx)) != 0) {
        goto err;
    }

    if ((ret = nng_ctx_open(&ctx->nng, ctx->fd)) != 0) {
        goto err;
    }

    ctx->state = NNG_NUTTX_INIT_RECV;
    nng_server_worker(ctx);

    return ctx;

err:
    nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));

    if (ctx != NULL) {
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
        }

        nxipc_free(ctx);
    }

    return NULL;
}

void nxipc_server_set_transaction_cb(void* nng_server_ctx, on_transaction cb, void* cb_priv)
{
    if (nng_server_ctx != NULL && cb != NULL) {
        nng_server_ctx_t* ctx = (nng_server_ctx_t*)nng_server_ctx;
        ctx->on_trans_cb = cb;
        ctx->priv = cb_priv;
    }
}

int nxipc_server_release(void* nng_server_ctx)
{
    if (nng_server_ctx != NULL) {
        nng_server_ctx_t* ctx = (nng_server_ctx_t*)nng_server_ctx;
        nng_aio_stop(ctx->aio);
        nng_aio_wait(ctx->aio);
        nng_ctx_close(ctx->nng);
        nng_aio_free(ctx->aio);
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
            ctx->name = NULL;
        }

        nxipc_free(ctx);
    }
}


void* nxipc_client_connect(const char* server_name)
{
    int ret = 0;
    nng_client_ctx_t* ctx = NULL;
    int name_len = strlen(server_name);
    const nng_nuttx_trans_type_t trans_type = NNG_NUTTX_TRANS_TYPE_INPROC;

    if (name_len <= 0) {
        ret = -EINVAL;
        goto err;
    }

    ctx = nxipc_calloc(sizeof(nng_server_ctx_t));

    if (ctx == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    memset(ctx, 0, sizeof(nng_server_ctx_t));

    ctx->name = nxipc_calloc(strlen(nng_nuttx_trans_prefix_str[trans_type]) + name_len + 1);

    if (ctx->name == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    strcpy(ctx->name, nng_nuttx_trans_prefix_str[trans_type]);
    strcat(ctx->name, server_name);

    if ((ret = nng_req0_open(&ctx->fd)) != 0) {
        goto err;
    }

    if ((ret = nng_dial(ctx->fd, ctx->name, NULL, 0)) != 0) {
        goto err;
    }

    nng_setopt_ms(ctx->fd, NNG_OPT_RECVTIMEO, NNG_NUTTX_RECV_TIMEOUT_MS);
    nng_setopt_ms(ctx->fd, NNG_OPT_SENDTIMEO, NNG_NUTTX_SEND_TIMEOUT_MS);

    return ctx;

err:
    nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));

    if (ctx != NULL) {
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
        }

        nxipc_free(ctx);
    }

    return NULL;
}

int nxipc_client_disconnect(void* nng_client_ctx)
{
    if (nng_client_ctx != NULL) {
        nng_client_ctx_t* ctx = (nng_client_ctx_t*)nng_client_ctx;

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
            ctx->name = NULL;
        }

        nng_close(ctx->fd);
        nxipc_free(ctx);
    }
}

int nxipc_client_transaction(const void* nng_client_ctx, int op_code, \
                             const nxparcel* in, nxparcel* out)
{
    int ret = 0;
    nng_msg* msg;
    nng_trans_hdr_t* hdr;
    nng_client_ctx_t* ctx = (nng_client_ctx_t*)nng_client_ctx;

    if (NULL == ctx) {
        ret = -EINVAL;
        goto out;
    }

    nng_msg_alloc(&msg, sizeof(nng_trans_hdr_t));

    if (msg == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    hdr = (nng_trans_hdr_t*)nng_msg_body(msg);
    hdr->seq = 0;
    hdr->op_code = op_code;
    hdr->len = 0;

    if (in != NULL && (hdr->len = nng_msg_len(in)) > 0) {
        nng_msg_append(msg, nng_msg_body((nng_msg*)in), hdr->len);
    }

    ret = nng_sendmsg(ctx->fd, msg, 0);

    if (ret < 0) {
        nxipc_log("%s: %s\n", __func__, nng_strerror(ret));
        goto out;
    }

    ret = nng_recvmsg(ctx->fd, &msg, 0);

    if (ret < 0 || msg == NULL || nng_msg_body(msg) == NULL) {
        nxipc_log("client_recv.ret=%s, msg=%p\n", nng_strerror(ret), msg);
        goto out;
    }

    hdr = (nng_trans_hdr_t*)nng_msg_body(msg);
    ret = nng_msg_trim(msg, sizeof(nng_trans_hdr_t));

    if (ret == 0 && hdr->len == nng_msg_len(msg)) {
        if (hdr->len > 0 && out != NULL) {
            nng_msg_append(out, nng_msg_body(msg), hdr->len);
        }
    }

out:
    nng_msg_free(msg);

    return ret;
}

void* nxipc_pub_create(const char* name)
{
    int ret = 0;
    nng_pub_ctx_t* ctx = NULL;
    int name_len = strlen(name);
    const nng_nuttx_trans_type_t trans_type = NNG_NUTTX_TRANS_TYPE_INPROC;

    if (name_len <= 0) {
        ret = -EINVAL;
        goto err;
    }

    ctx = nxipc_calloc(sizeof(nng_server_ctx_t));

    if (ctx == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    memset(ctx, 0, sizeof(nng_server_ctx_t));

    ctx->name = nxipc_calloc(strlen(nng_nuttx_trans_prefix_str[trans_type]) + name_len + 1);

    if (ctx->name == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    strcpy(ctx->name, nng_nuttx_trans_prefix_str[trans_type]);
    strcat(ctx->name, name);

    if ((ret = nng_pub0_open(&ctx->fd)) != 0) {
        goto err;
    }

    if ((ret = nng_listen(ctx->fd, ctx->name, NULL, 0)) != 0) {
        goto err;
    }

    return ctx;
err:
    nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));

    if (ctx != NULL) {
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
        }

        nxipc_free(ctx);
    }

    return NULL;
}

int nxipc_pub_release(void* nng_pub_ctx)
{
    if (nng_pub_ctx != NULL) {
        nng_pub_ctx_t* ctx = (nng_pub_ctx_t*)nng_pub_ctx;

        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
            ctx->name = NULL;
        }

        nxipc_free(ctx);
    }
}

int nxipc_pub_topic_msg(void* nng_pub_ctx, const void* topic, size_t topic_len, const nxparcel* parcel)
{
    int ret = 0;
    int send_len = 0;
    nng_pub_ctx_t* ctx = (nng_pub_ctx_t*)nng_pub_ctx;
    nng_nuttx_topic_t* topic_data;
    nng_msg* msg = NULL;

    if (topic == NULL || ctx == NULL /*|| ctx->proto != NN_PUB*/) {
        return -EINVAL;
    }

    nng_msg_alloc(&msg, sizeof(nng_nuttx_topic_t));
    topic_data = (nng_nuttx_topic_t*)nng_msg_body(msg);

    if (topic_data == NULL || msg == NULL) {
        nng_msg_free(msg);
        return -ENOMEM;
    }

    memcpy(topic_data->topic, topic, topic_len > NNG_NUTTX_TOPIC_NAME_LEN ? NNG_NUTTX_TOPIC_NAME_LEN : topic_len);

    if (parcel != NULL) {
        topic_data->content_len = nng_msg_len(parcel);
        nng_msg_append(msg, nng_msg_body((nng_msg*)parcel), topic_data->content_len);
    } else {
        topic_data->content_len = 0;
    }

    ret = nng_sendmsg(ctx->fd, msg, 0);

    if (ret != 0) {
        nng_msg_free(msg);
        nxipc_log("%s.ret=%d, send_len=%d\n", __func__, ret, send_len);
    }

    return ret;
}

static void nng_sub_worker(void* arg)
{
    int ret;
    nng_msg* msg = NULL;
    nng_nuttx_topic_t* topic;
    nng_sub_ctx_t* ctx = (nng_sub_ctx_t*)arg;

    if (NULL == ctx) {
        return;
    }

    switch (ctx->state) {
    case NNG_NUTTX_INIT_RECV:
        ctx->state = NNG_NUTTX_RECV_RET_RECV;
        nng_ctx_recv(ctx->nng, ctx->aio);
        break;

    case NNG_NUTTX_RECV_RET_RECV:
        if ((ret = nng_aio_result(ctx->aio)) != 0) {
            if (ret == NNG_ETIMEDOUT) {
                nng_ctx_recv(ctx->nng, ctx->aio);
            } else {
                nxipc_log("%s: nng_aio_result.error=%d(%s)\n", __func__, ret, nng_strerror(ret));
            }

            break;
        }

        msg = nng_aio_get_msg(ctx->aio);

        if (msg == NULL || nng_msg_body(msg) == NULL) {
            nng_ctx_recv(ctx->nng, ctx->aio);
            break;
        }

        topic = (nng_nuttx_topic_t*)nng_msg_body(msg);
        nng_msg_trim(msg, sizeof(nng_nuttx_topic_t));
        ret = nng_msg_len(msg);

        if (ret != topic->content_len) {
            nxipc_log("%s.content_len=%lu, rc=%d\n", __func__, topic->content_len, ret);
        } else if (ctx->listener != NULL) {
            ctx->listener(ctx->priv, topic->topic, NNG_NUTTX_TOPIC_NAME_LEN, msg);
        }

        nng_msg_free(msg);
        nng_ctx_recv(ctx->nng, ctx->aio);
        break;

    default:
        nxipc_log("%s: bad state %d", __func__, ctx->state);
        break;
    }

    return;
}

void* nxipc_sub_connect(const char* name, on_topic_listener listener, void* listener_priv)
{
    int ret = 0;
    nng_sub_ctx_t* ctx = NULL;
    int name_len = strlen(name);
    const nng_nuttx_trans_type_t trans_type = NNG_NUTTX_TRANS_TYPE_INPROC;

    if (name_len <= 0) {
        ret = -EINVAL;
        goto err;
    }

    ctx = nxipc_calloc(sizeof(nng_sub_ctx_t));

    if (ctx == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    memset(ctx, 0, sizeof(nng_sub_ctx_t));

    ctx->name = nxipc_calloc(strlen(nng_nuttx_trans_prefix_str[trans_type]) + name_len + 1);

    if (ctx->name == NULL) {
        ret = -ENOMEM;
        goto err;
    }

    strcpy(ctx->name, nng_nuttx_trans_prefix_str[trans_type]);
    strcat(ctx->name, name);

    if ((ret = nng_sub0_open(&ctx->fd)) != 0) {
        goto err;
    }

    if ((ret = nng_dial(ctx->fd, ctx->name, NULL, 0)) != 0) {
        goto err;
    }

    nng_setopt_ms(ctx->fd, NNG_OPT_RECVTIMEO, NNG_NUTTX_RECV_TIMEOUT_MS);
    ctx->listener = listener;
    ctx->priv = listener_priv;

    if ((ret = nng_aio_alloc(&ctx->aio, nng_sub_worker, ctx)) != 0) {
        goto err;
    }

    if ((ret = nng_ctx_open(&ctx->nng, ctx->fd)) != 0) {
        goto err;
    }

    ctx->state = NNG_NUTTX_INIT_RECV;
    nng_sub_worker(ctx);

    return ctx;
err:
    nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));

    if (ctx != NULL) {
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
        }

        nxipc_free(ctx);
    }

    return NULL;
}

void nxipc_sub_disconnect(void* nng_sub_ctx)
{
    if (nng_sub_ctx != NULL) {
        nng_sub_ctx_t* ctx = (nng_sub_ctx_t*)nng_sub_ctx;

        nng_aio_stop(ctx->aio);
        nng_aio_wait(ctx->aio);
        nng_ctx_close(ctx->nng);
        nng_aio_free(ctx->aio);
        nng_close(ctx->fd);

        if (ctx->name != NULL) {
            nxipc_free(ctx->name);
            ctx->name = NULL;
        }

        nxipc_free(ctx);
    }
}


int nxipc_sub_register_topic(void* nng_sub_ctx, const void* topic, size_t topic_len)
{
    int ret = 0;
    nng_sub_ctx_t* ctx = (nng_sub_ctx_t*)nng_sub_ctx;

    if (topic == NULL || ctx == NULL /* || ctx->proto != NN_SUB*/) {
        return -EINVAL;
    }

    if (topic_len > NNG_NUTTX_TOPIC_NAME_LEN) {
        nxipc_log("%s.topic len(%lu) > max(%d)\n", __func__, topic_len, NNG_NUTTX_TOPIC_NAME_LEN);
        topic_len = NNG_NUTTX_TOPIC_NAME_LEN;
    }

    ret  = nng_ctx_setopt(ctx->nng, NNG_OPT_SUB_SUBSCRIBE, topic, topic_len);

    if (ret < 0) {
        nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));
    }

    return ret;
}

int nxipc_sub_unregister_topic(void* nng_sub_ctx, const void* topic, size_t topic_len)
{
    int ret = 0;
    nng_sub_ctx_t* ctx = (nng_sub_ctx_t*)nng_sub_ctx;

    if (topic == NULL || ctx == NULL /* || ctx->proto != NN_SUB*/) {
        return -EINVAL;
    }

    if (topic_len > NNG_NUTTX_TOPIC_NAME_LEN) {
        nxipc_log("%s.topic len(%lu) > max(%d)\n", __func__, topic_len, NNG_NUTTX_TOPIC_NAME_LEN);
        topic_len = NNG_NUTTX_TOPIC_NAME_LEN;
    }

    ret  = nng_ctx_setopt(ctx->nng, NNG_OPT_SUB_UNSUBSCRIBE, topic, topic_len);

    if (ret < 0) {
        nxipc_log("%s.ret=%d(%s)\n", __func__, ret, nng_strerror(ret));
    }

    return ret;
}

