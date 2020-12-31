/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    nxtask.c
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-12-30 22:46:31
 */
#include <errno.h>
#include "core/nng_impl.h"
#include "nxtask.h"

typedef struct nxtask_ctx {
    void* scheduler;
    nxtask task;
    void* priv;
} nxtask_ctx_t;

static void nxtask_task(void* arg)
{
    nxtask_ctx_t* ctx = (nxtask_ctx_t*)arg;
    if (arg != NULL) {
        if (ctx->task != NULL) {
            ctx->task(ctx->priv);
        }

        if (ctx->scheduler != NULL) {
            free(ctx->scheduler);
        }

        free(ctx);
    }
}

int nxtask_delay(int ms, nxtask task, void* arg)
{
    nxtask_ctx_t* ctx = calloc(1, sizeof(nxtask_ctx_t));

    if (ctx == NULL) {
        return -ENOMEM;
    }

    ctx->task = task;
    ctx->priv = arg;
    ctx->scheduler = calloc(1, sizeof(nni_timer_node));

    if (ctx->scheduler == NULL) {
        free(ctx);
        return -ENOMEM;
    }

    nni_timer_init((nni_timer_node*)ctx->scheduler, nxtask_task, ctx);
    nni_timer_schedule((nni_timer_node*)ctx->scheduler, nni_clock() + ms);
    return 0;
}

int nxtask_async(nxtask task, void* arg)
{
    nxtask_ctx_t* ctx = calloc(1, sizeof(nxtask_ctx_t));

    if (ctx == NULL) {
        return -ENOMEM;
    }

    ctx->task = task;
    ctx->priv = arg;
    ctx->scheduler = calloc(1, sizeof(nni_reap_item));

    if (ctx->scheduler == NULL) {
        free(ctx);
        return -ENOMEM;
    }

    nni_reap((nni_reap_item*)ctx->scheduler, nxtask_task, ctx);
    return 0;
}
