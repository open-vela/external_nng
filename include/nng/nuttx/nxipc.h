/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    nxipc.h
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-11-30 13:39:10
 */

#ifndef __NXIPC_H__
#define __NXIPC_H__

#include <stdint.h>
#include "nxparcel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*on_transaction)(const void* cookie, const int code, \
        const nxparcel* in, nxparcel** out);
typedef int (*on_topic_listener)(const void* cookie, const void* topic, \
        const size_t topic_len, const nxparcel* parcel);

/**
 * @brief:nxipc_server_create
 *
 * @param name
 *
 * @return
 */
void* nxipc_server_create(const char* name);

/**
 * @brief:nxipc_server_set_transaction_cb
 *
 * @param nxipc_server_ctx
 * @param cb
 * @param cb_priv
 */
void nxipc_server_set_transaction_cb(void* nxipc_server_ctx, \
        on_transaction cb, void* cb_priv);

/**
 * @brief:nxipc_server_release
 *
 * @param nxipc_server_ctx
 *
 * @return
 */
int nxipc_server_release(void* nxipc_server_ctx);

/**
 * @brief:nxipc_client_connect
 *
 * @param server_name
 *
 * @return
 */
void* nxipc_client_connect(const char* server_name);

/**
 * @brief:nxipc_client_disconnect
 *
 * @param nxipc_client_ctx
 *
 * @return
 */
int nxipc_client_disconnect(void* nxipc_client_ctx);

/**
 * @brief:nxipc_client_transaction
 *
 * @param nxipc_client_ctx
 * @param op_code
 * @param in
 * @param out
 *
 * @return
 */
int nxipc_client_transaction(const void* nxipc_client_ctx, int op_code, \
        const nxparcel* in, nxparcel* out);

/**
 * @brief:nxipc_pub_create
 *
 * @param name
 *
 * @return
 */
void* nxipc_pub_create(const char* name);

/**
 * @brief:nxipc_pub_release
 *
 * @param nxipc_pub_ctx
 *
 * @return
 */
int nxipc_pub_release(void* nxipc_pub_ctx);

/**
 * @brief:nxipc_pub_topic_msg
 *
 * @param nxipc_pub_ctx
 * @param topic
 * @param topic_len
 * @param content
 * @param content_len
 *
 * @return
 */
int nxipc_pub_topic_msg(void* nxipc_pub_ctx, const void* topic, size_t topic_len, \
        const nxparcel* parcel);

/**
 * @brief:nxipc_sub_connect
 *
 * @param name
 * @param listener
 * @param listener_priv
 *
 * @return
 */
void* nxipc_sub_connect(const char* name, on_topic_listener listener, void* listener_priv);

/**
 * @brief:nxipc_sub_disconnect
 *
 * @param nxipc_sub_ctx
 */
void nxipc_sub_disconnect(void* nxipc_sub_ctx);

/**
 * @brief:nxipc_sub_register_topic
 *
 * @param nxipc_sub_ctx
 * @param topic
 * @param topic_len
 *
 * @return
 */
int nxipc_sub_register_topic(void* nxipc_sub_ctx, const void* topic, size_t topic_len);

/**
 * @brief:nxipc_sub_unregister_topic
 *
 * @param nxipc_sub_ctx
 * @param topic
 * @param topic_len
 *
 * @return
 */
int nxipc_sub_unregister_topic(void* nxipc_sub_ctx, const void* topic, size_t topic_len);

#ifdef __cplusplus
}
#endif

#endif /*__NXIPC_H__*/
