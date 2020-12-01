#include "nxparcel.h"

int nxparcel_alloc(nxparcel** msg) {
    return nng_msg_alloc(msg, 0);
}

void nxparcel_free(nxparcel* msg) {
    nng_msg_free(msg);
}

int nxparcel_append_u16(nxparcel* msg, uint16_t val) {
    return nng_msg_append_u16(msg, val);
}

int nxparcel_append_u32(nxparcel* msg, uint32_t val) {
    return nng_msg_append_u32(msg, val);
}

int nxparcel_append_u64(nxparcel* msg, uint64_t val){
    return nng_msg_append_u64(msg, val);
}

int nxparcel_append(nxparcel* msg, const void* data, size_t size) {
    return nng_msg_append(msg, data, size);
}

void* nxparcel_data(const nxparcel* msg) {
    return nng_msg_body((nxparcel*)msg);
}

int nxparcel_skip(nxparcel* msg, size_t size) {
    return nng_msg_trim(msg, size);
}

int nxparcel_read_u16(nxparcel* msg, uint16_t* val) {
    return nng_msg_trim_u16(msg, val);
}

int nxparcel_read_u32(nxparcel* msg, uint32_t* val) {
    return nng_msg_trim_u32(msg, val);
}

int nxparcel_read_u64(nxparcel* msg, uint64_t* val) {
    return nng_msg_trim_u64(msg, val);
}

int nxparcel_size(const nxparcel* msg) {
    return nng_msg_len(msg);
}

void nxparcel_clear(nxparcel* msg) {
    nng_msg_clear(msg);
}
