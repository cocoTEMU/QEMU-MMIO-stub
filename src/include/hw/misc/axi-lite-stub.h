#ifndef HW_AXI_LITE_STUB_H
#define HW_AXI_LITE_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_AXI_LITE_STUB "axi-lite-stub"
OBJECT_DECLARE_SIMPLE_TYPE(AxiLiteStub, AXI_LITE_STUB)

enum axi_lite_op {
    AXI_LITE_READ  = 0x00,
    AXI_LITE_WRITE = 0x01,
};

enum axi_lite_resp_code {
    AXI_LITE_RESP_OKAY   = 0x00,
    AXI_LITE_RESP_SLVERR = 0x02,
    AXI_LITE_RESP_DECERR = 0x03,
};

/* Request */
struct axi_lite_req {
    uint8_t  op;        /* axi_lite_op */
    uint8_t  wstrb;     /* write strobes (1 bit per byte, max 8) */
    uint16_t _pad;
    uint32_t _pad2;
    uint64_t addr;      /* ARADDR / AWADDR */
    uint64_t wdata;     /* WDATA (writes only) */
} QEMU_PACKED;

/* Response */
struct axi_lite_resp {
    uint8_t  resp;      /* axi_lite_resp_code (BRESP/RRESP) */
    uint8_t  _pad[7];
    uint64_t rdata;     /* RDATA (reads only) */
} QEMU_PACKED;

struct AxiLiteStub {
    ChardevStub parent_obj;

    uint32_t data_width; /* 32 or 64 bits */
};

#endif /* HW_AXI_LITE_STUB_H */
