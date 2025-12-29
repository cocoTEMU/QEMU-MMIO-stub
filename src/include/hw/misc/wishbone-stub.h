#ifndef HW_WISHBONE_STUB_H
#define HW_WISHBONE_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_WISHBONE_STUB "wishbone-stub"
OBJECT_DECLARE_SIMPLE_TYPE(WishboneStub, WISHBONE_STUB)

enum wb_op {
    WB_READ  = 0x00,
    WB_WRITE = 0x01,
};

/* Cycle Type Identifier (CTI) */
enum wb_cti {
    WB_CTI_CLASSIC    = 0, /* classic cycle */
    WB_CTI_CONST_ADDR = 1, /* constant address burst */
    WB_CTI_INCR_ADDR  = 2, /* incrementing address burst */
    WB_CTI_END_BURST  = 7, /* end of burst */
};

/* Burst Type Extension (BTE) */
enum wb_bte {
    WB_BTE_LINEAR  = 0,
    WB_BTE_WRAP4   = 1,
    WB_BTE_WRAP8   = 2,
    WB_BTE_WRAP16  = 3,
};

/* Request (one per cycle beat) */
struct wb_req {
    uint8_t  op;        /* wb_op: WE_O */
    uint8_t  sel;       /* SEL_O: byte-lane select (1 bit per byte) */
    uint8_t  cti;       /* wb_cti */
    uint8_t  bte;       /* wb_bte */
    uint32_t _pad;
    uint64_t addr;      /* ADR_O */
    uint64_t wdata;     /* DAT_O (writes only) */
} QEMU_PACKED;

/* Response (one per beat) */
struct wb_resp {
    uint8_t  ack;       /* ACK_I */
    uint8_t  err;       /* ERR_I */
    uint8_t  rty;       /* RTY_I */
    uint8_t  _pad[5];
    uint64_t rdata;     /* DAT_I (reads only) */
} QEMU_PACKED;

struct WishboneStub {
    ChardevStub parent_obj;

    uint32_t data_width;    /* 8, 16, 32, or 64 bits */
    uint32_t granularity;   /* port granularity in bits (8, 16, 32, 64) */
    bool     pipelined;     /* enable pipelined mode */
};

#endif /* HW_WISHBONE_STUB_H */
