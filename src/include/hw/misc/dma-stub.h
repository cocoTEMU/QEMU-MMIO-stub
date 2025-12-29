#ifndef HW_DMA_STUB_H
#define HW_DMA_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_DMA_STUB "dma-stub"
OBJECT_DECLARE_SIMPLE_TYPE(DmaStub, DMA_STUB)

#define DMA_MAX_CHANNELS 8

enum dma_op {
    DMA_START  = 0x00, /* start a transfer */
    DMA_ABORT  = 0x01, /* abort a transfer */
    DMA_STATUS = 0x02, /* query transfer status */
};

enum dma_status {
    DMA_STATUS_IDLE  = 0,
    DMA_STATUS_BUSY  = 1,
    DMA_STATUS_DONE  = 2,
    DMA_STATUS_ERROR = 3,
};

enum dma_flags {
    DMA_FLAG_IRQ_ON_DONE = (1 << 0), /* raise IRQ on completion */
    DMA_FLAG_SRC_INCR    = (1 << 1), /* increment source address */
    DMA_FLAG_DST_INCR    = (1 << 2), /* increment destination address */
};

/* Request sent over chardev */
struct dma_req {
    uint8_t  op;            /* dma_op */
    uint8_t  channel;       /* channel index */
    uint16_t _pad;
    uint32_t flags;         /* dma_flags bitmask */
    uint64_t src_addr;
    uint64_t dst_addr;
    uint32_t length;        /* transfer length in bytes */
    uint32_t _pad2;
} QEMU_PACKED;

/* Response received from chardev */
struct dma_resp {
    uint8_t  status;        /* dma_status */
    uint8_t  channel;
    uint16_t _pad;
    uint32_t bytes_transferred;
} QEMU_PACKED;

/*
 * Per-channel control register layout (offsets relative to channel base).
 * Channel N base = N * DMA_CHAN_STRIDE.
 */
#define DMA_CHAN_SRC_LO     0x00  /* source address [31:0] */
#define DMA_CHAN_SRC_HI     0x04  /* source address [63:32] */
#define DMA_CHAN_DST_LO     0x08  /* destination address [31:0] */
#define DMA_CHAN_DST_HI     0x0C  /* destination address [63:32] */
#define DMA_CHAN_LENGTH      0x10  /* transfer length in bytes */
#define DMA_CHAN_FLAGS       0x14  /* dma_flags bitmask */
#define DMA_CHAN_CONTROL     0x18  /* write: 1=start, 2=abort */
#define DMA_CHAN_STATUS      0x1C  /* read: dma_status */
#define DMA_CHAN_XFERRED     0x20  /* read: bytes transferred so far */
#define DMA_CHAN_STRIDE      0x40  /* spacing between channel register blocks */

/* Per-channel shadow state */
struct dma_channel_state {
    uint64_t src_addr;
    uint64_t dst_addr;
    uint32_t length;
    uint32_t flags;
    uint8_t  status;
    uint32_t bytes_transferred;
};

struct DmaStub {
    ChardevStub parent_obj;

    uint32_t num_channels;      /* 1..DMA_MAX_CHANNELS */
    uint32_t max_transfer_size; /* max length per transfer */

    struct dma_channel_state channels[DMA_MAX_CHANNELS];
};

#endif /* HW_DMA_STUB_H */
