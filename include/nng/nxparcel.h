/*
 * Copyright (c) 2020 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    nxparcel.h
 * @brief:
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.0
 * @date:    2020-11-30 19:52:35
 */

#ifndef __NXPARCEL_H__
#define __NXPARCEL_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <nng/nng.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nng_msg nxparcel;

/**
 * @brief:nxparcel_alloc
 *
 * @param msg
 *
 * @return
 */
int nxparcel_alloc(nxparcel** msg);

/**
 * @brief:nxparcel_free
 *
 * @param msg
 */
void nxparcel_free(nxparcel* msg);

/**
 * @brief:nxparcel_append_u16
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_append_u16(nxparcel* msg, uint16_t val);

/**
 * @brief:nxparcel_append_u32
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_append_u32(nxparcel* msg, uint32_t val);

/**
 * @brief:nxparcel_append_u64
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_append_u64(nxparcel* msg, uint64_t val);

/**
 * @brief:nxparcel_append
 *
 * @param msg
 * @param data
 * @param size
 *
 * @return
 */
int nxparcel_append(nxparcel* msg, const void* data, size_t size);

/**
 * @brief:nxparcel_read
 *
 * @param msg
 *
 * @return
 */
void* nxparcel_data(const nxparcel* msg);

/**
 * @brief:nxparcel_skip
 *
 * @param msg
 * @param size
 *
 * @return
 */
int nxparcel_skip(nxparcel* msg, size_t size);
/**
 * @brief:nxparcel_read_u16
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_read_u16(nxparcel* msg, uint16_t* val);

/**
 * @brief:nxparcel_read_u32
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_read_u32(nxparcel* msg, uint32_t* val);

/**
 * @brief:nxparcel_read_u64
 *
 * @param msg
 * @param val
 *
 * @return
 */
int nxparcel_read_u64(nxparcel* msg, uint64_t* val);

/**
 * @brief:nxparcel_size
 *
 * @param msg
 *
 * @return
 */
int nxparcel_size(const nxparcel* msg);

/**
 * @brief:nxparcel_clear
 *
 * @param msg
 */
void nxparcel_clear(nxparcel* msg);

#ifdef __cplusplus
}
#endif

#endif /*__NXPARCEL_H__*/
