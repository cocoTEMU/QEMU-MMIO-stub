#ifndef HW_AVALON_MM_STUB_H
#define HW_AVALON_MM_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_AVALON_MM_STUB "avalon-mm-stub"
OBJECT_DECLARE_SIMPLE_TYPE(AvalonMmStub, AVALON_MM_STUB)

enum avalon_mm_op {
    AVALON_MM_READ  = 0x00,
    AVALON_MM_WRITE = 0x01,
};

enum avalon_mm_resp_code {
    AVALON_MM_RESP_OK     = 0x00,
    AVALON_MM_RESP_SLVERR = 0x02,
    AVALON_MM_RESP_DECERR = 0x03,
};

/* Request */
struct avalon_mm_req {
    uint8_t  op;            /* avalon_mm_op */
    uint8_t  _pad;
    uint16_t burstcount;    /* number of words (1..max) */
    uint32_t byteenable;    /* byte-enable mask */
    uint64_t addr;          /* byte address */
    uint64_t wdata;         /* writedata (first word; burst data follows) */
} QEMU_PACKED;

/* Response */
struct avalon_mm_resp {
    uint8_t  response;      /* avalon_mm_resp_code */
    uint8_t  _pad[7];
    uint64_t rdata;         /* readdata (first word; burst data follows) */
} QEMU_PACKED;

struct AvalonMmStub {
    ChardevStub parent_obj;

    uint32_t data_width;       /* data bus width in bits */
    uint32_t max_burstcount;   /* max burst length (default 1) */
};

#endif /* HW_AVALON_MM_STUB_H */
