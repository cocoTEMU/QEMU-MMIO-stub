#ifndef HW_AHB_LITE_STUB_H
#define HW_AHB_LITE_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_AHB_LITE_STUB "ahb-lite-stub"
OBJECT_DECLARE_SIMPLE_TYPE(AhbLiteStub, AHB_LITE_STUB)

enum ahb_lite_op {
    AHB_LITE_READ  = 0x00,
    AHB_LITE_WRITE = 0x01,
};

enum ahb_htrans {
    AHB_HTRANS_IDLE   = 0,
    AHB_HTRANS_BUSY   = 1,
    AHB_HTRANS_NONSEQ = 2,
    AHB_HTRANS_SEQ    = 3,
};

enum ahb_hburst {
    AHB_HBURST_SINGLE = 0,
    AHB_HBURST_INCR   = 1,
    AHB_HBURST_WRAP4  = 2,
    AHB_HBURST_INCR4  = 3,
    AHB_HBURST_WRAP8  = 4,
    AHB_HBURST_INCR8  = 5,
    AHB_HBURST_WRAP16 = 6,
    AHB_HBURST_INCR16 = 7,
};

/* Request */
struct ahb_lite_req {
    uint8_t  op;        /* ahb_lite_op (HWRITE) */
    uint8_t  hsize;     /* HSIZE: log2(bytes), 0..7 */
    uint8_t  htrans;    /* ahb_htrans */
    uint8_t  hburst;    /* ahb_hburst */
    uint32_t _pad;
    uint64_t addr;      /* HADDR */
    uint64_t wdata;     /* HWDATA (writes only) */
} QEMU_PACKED;

/* Response */
struct ahb_lite_resp {
    uint8_t  hresp;     /* 0=OKAY, 1=ERROR */
    uint8_t  _pad[7];
    uint64_t rdata;     /* HRDATA (reads only) */
} QEMU_PACKED;

struct AhbLiteStub {
    ChardevStub parent_obj;

    uint32_t data_width; /* 8, 16, 32, 64, 128, 256, 512, 1024 bits */
};

#endif /* HW_AHB_LITE_STUB_H */
