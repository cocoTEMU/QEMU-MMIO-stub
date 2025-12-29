#ifndef HW_APB_STUB_H
#define HW_APB_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_APB_STUB "apb-stub"
OBJECT_DECLARE_SIMPLE_TYPE(ApbStub, APB_STUB)

enum apb_op {
    APB_READ  = 0x00,
    APB_WRITE = 0x01,
};

/* Request: models a PSEL+PENABLE+PWRITE transaction */
struct apb_req {
    uint8_t  op;        /* apb_op (PWRITE) */
    uint8_t  pstrb;     /* PSTRB: byte-lane strobes (APB4) */
    uint16_t _pad;
    uint32_t addr;      /* PADDR */
    uint32_t wdata;     /* PWDATA (writes only) */
} QEMU_PACKED;

/* Response */
struct apb_resp {
    uint8_t  pslverr;   /* PSLVERR: 0=OK, 1=error */
    uint8_t  _pad[3];
    uint32_t rdata;     /* PRDATA (reads only) */
} QEMU_PACKED;

struct ApbStub {
    ChardevStub parent_obj;

    uint32_t data_width; /* 8, 16, or 32 bits */
};

#endif /* HW_APB_STUB_H */
