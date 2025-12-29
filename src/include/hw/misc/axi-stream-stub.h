#ifndef HW_AXI_STREAM_STUB_H
#define HW_AXI_STREAM_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_AXI_STREAM_STUB "axi-stream-stub"
OBJECT_DECLARE_SIMPLE_TYPE(AxiStreamStub, AXI_STREAM_STUB)

/* Max TDATA width: 1024 bits = 128 bytes */
#define AXI_STREAM_MAX_TDATA_BYTES 128

enum axi_stream_op {
    AXI_STREAM_PUSH = 0x00, /* host -> sim: push data into stream */
    AXI_STREAM_PULL = 0x01, /* host -> sim: pull data from stream */
};

/* Push request header (followed by tdata + tkeep payload) */
struct axi_stream_push_req {
    uint8_t  op;            /* AXI_STREAM_PUSH */
    uint8_t  tlast;         /* TLAST: 1 = end of packet */
    uint16_t tkeep_len;     /* number of TKEEP bytes following TDATA */
    uint32_t tdata_len;     /* number of TDATA bytes following header */
} QEMU_PACKED;

/* Push response */
struct axi_stream_push_resp {
    uint8_t  ok;            /* 0 = accepted, 1 = backpressure/error */
    uint8_t  _pad[3];
} QEMU_PACKED;

/* Pull request */
struct axi_stream_pull_req {
    uint8_t  op;            /* AXI_STREAM_PULL */
    uint8_t  _pad[3];
    uint32_t max_len;       /* max bytes the host can accept */
} QEMU_PACKED;

/* Pull response header (followed by tdata + tkeep payload) */
struct axi_stream_pull_resp {
    uint8_t  tlast;
    uint8_t  _pad;
    uint16_t tkeep_len;
    uint32_t tdata_len;
} QEMU_PACKED;

struct AxiStreamStub {
    ChardevStub parent_obj;

    uint32_t tdata_width;   /* TDATA width in bits (default 32) */
};

#endif /* HW_AXI_STREAM_STUB_H */
