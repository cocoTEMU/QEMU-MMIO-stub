#ifndef HW_AXI4_STUB_H
#define HW_AXI4_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_AXI4_STUB "axi4-stub"
OBJECT_DECLARE_SIMPLE_TYPE(Axi4Stub, AXI4_STUB)

/* Max data width: 1024 bits = 128 bytes per beat */
#define AXI4_MAX_DATA_BYTES 128

enum axi4_op {
    AXI4_READ  = 0x00,
    AXI4_WRITE = 0x01,
};

enum axi4_burst {
    AXI4_BURST_FIXED = 0,
    AXI4_BURST_INCR  = 1,
    AXI4_BURST_WRAP  = 2,
};

enum axi4_resp_code {
    AXI4_RESP_OKAY   = 0x00,
    AXI4_RESP_EXOKAY = 0x01,
    AXI4_RESP_SLVERR = 0x02,
    AXI4_RESP_DECERR = 0x03,
};

/*
 * Request header — sent once per transaction.
 * For single-beat (standard MMIO access): len=0, burst=INCR.
 * For multi-beat burst (via burst engine): len=N-1, burst=type.
 */
struct axi4_req_hdr {
    uint8_t  op;        /* axi4_op */
    uint8_t  size;      /* AxSIZE: log2(bytes per beat), 0..7 => 1..128 */
    uint8_t  burst;     /* axi4_burst */
    uint8_t  len;       /* AxLEN: beats minus 1, 0..255 */
    uint32_t _pad;
    uint64_t addr;      /* AxADDR */
} QEMU_PACKED;

/*
 * Write beat — sent (len+1) times after the header for writes.
 * Variable size: wdata is (1 << size) bytes, wstrb is ceil((1 << size) / 8).
 * Declared as a max-size struct; actual bytes sent = beat_bytes + strb_bytes + 1.
 */
struct axi4_write_beat {
    uint8_t wdata[AXI4_MAX_DATA_BYTES]; /* up to 128 bytes */
    uint8_t wstrb[AXI4_MAX_DATA_BYTES / 8]; /* up to 16 bytes */
    uint8_t wlast;                      /* 1 on final beat */
} QEMU_PACKED;

/* Response — one per transaction */
struct axi4_resp {
    uint8_t  resp;      /* axi4_resp_code */
    uint8_t  _pad[7];
} QEMU_PACKED;

/*
 * Burst engine control registers (offsets from burst_base).
 * Guest writes these, then writes GO_REG to trigger the burst.
 */
#define AXI4_BURST_ADDR_LO  0x00  /* bits [31:0] of burst address */
#define AXI4_BURST_ADDR_HI  0x04  /* bits [63:32] of burst address */
#define AXI4_BURST_LEN      0x08  /* AxLEN (0..255) */
#define AXI4_BURST_SIZE      0x0C  /* AxSIZE (0..7) */
#define AXI4_BURST_TYPE      0x10  /* burst type (0=FIXED, 1=INCR, 2=WRAP) */
#define AXI4_BURST_GO        0x14  /* write 1=read burst, 2=write burst */
#define AXI4_BURST_STATUS    0x18  /* read: 0=idle, 1=busy, 2=done, 3=error */
#define AXI4_BURST_DATA      0x20  /* data FIFO window for burst read/write */
#define AXI4_BURST_REGION_SIZE 0x100

struct Axi4Stub {
    ChardevStub parent_obj;

    uint32_t data_width;     /* bus data width in bits (8..1024) */
    uint32_t max_burst_len;  /* max AxLEN+1 (default 256) */

    /* Burst engine state */
    MemoryRegion burst_iomem;
    uint64_t burst_addr;
    uint8_t  burst_len;
    uint8_t  burst_size;
    uint8_t  burst_type;
    uint8_t  burst_status;
};

#endif /* HW_AXI4_STUB_H */
