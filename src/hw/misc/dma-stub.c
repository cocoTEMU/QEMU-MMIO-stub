#include "qemu/osdep.h"
#include "hw/misc/dma-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"

/* DMA transfer execution over chardev */

static void dma_execute_transfer(DmaStub *s, uint8_t ch)
{
    ChardevStub *base = CHARDEV_STUB(s);
    struct dma_channel_state *chan = &s->channels[ch];

    if (!chardev_stub_is_connected(base, "dma-start", 0)) {
        chan->status = DMA_STATUS_ERROR;
        return;
    }

    struct dma_req req = {
        .op       = DMA_START,
        .channel  = ch,
        .flags    = chan->flags,
        .src_addr = chan->src_addr,
        .dst_addr = chan->dst_addr,
        .length   = chan->length,
    };

    chan->status = DMA_STATUS_BUSY;

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        chan->status = DMA_STATUS_ERROR;
        return;
    }

    struct dma_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        chan->status = DMA_STATUS_ERROR;
        return;
    }

    chan->status = resp.status;
    chan->bytes_transferred = resp.bytes_transferred;

    /* Raise IRQ if transfer completed and IRQ flag is set */
    if (chan->status == DMA_STATUS_DONE &&
        (chan->flags & DMA_FLAG_IRQ_ON_DONE)) {
        qemu_irq_raise(base->irq);
    }
}

static void dma_abort_transfer(DmaStub *s, uint8_t ch)
{
    ChardevStub *base = CHARDEV_STUB(s);
    struct dma_channel_state *chan = &s->channels[ch];

    if (!chardev_stub_is_connected(base, "dma-abort", 0)) {
        return;
    }

    struct dma_req req = {
        .op      = DMA_ABORT,
        .channel = ch,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct dma_resp resp = {};
    if (chardev_stub_recv(base, &resp, sizeof(resp))) {
        chan->status = resp.status;
        chan->bytes_transferred = resp.bytes_transferred;
    }
}

/* MMIO register access */

static uint64_t dma_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    DmaStub *s = DMA_STUB(opaque);
    uint8_t ch = addr / DMA_CHAN_STRIDE;
    hwaddr off = addr % DMA_CHAN_STRIDE;

    if (ch >= s->num_channels) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dma-stub: read from invalid channel %u\n", ch);
        return 0;
    }

    struct dma_channel_state *chan = &s->channels[ch];

    switch (off) {
    case DMA_CHAN_SRC_LO:
        return (uint32_t)(chan->src_addr);
    case DMA_CHAN_SRC_HI:
        return (uint32_t)(chan->src_addr >> 32);
    case DMA_CHAN_DST_LO:
        return (uint32_t)(chan->dst_addr);
    case DMA_CHAN_DST_HI:
        return (uint32_t)(chan->dst_addr >> 32);
    case DMA_CHAN_LENGTH:
        return chan->length;
    case DMA_CHAN_FLAGS:
        return chan->flags;
    case DMA_CHAN_STATUS:
        return chan->status;
    case DMA_CHAN_XFERRED:
        return chan->bytes_transferred;
    default:
        return 0;
    }
}

static void dma_stub_write(void *opaque, hwaddr addr, uint64_t val,
                           unsigned len)
{
    DmaStub *s = DMA_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);
    uint8_t ch = addr / DMA_CHAN_STRIDE;
    hwaddr off = addr % DMA_CHAN_STRIDE;

    if (ch >= s->num_channels) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "dma-stub: write to invalid channel %u\n", ch);
        return;
    }

    struct dma_channel_state *chan = &s->channels[ch];

    switch (off) {
    case DMA_CHAN_SRC_LO:
        chan->src_addr = (chan->src_addr & 0xFFFFFFFF00000000ULL) |
                          (uint32_t)val;
        break;
    case DMA_CHAN_SRC_HI:
        chan->src_addr = (chan->src_addr & 0x00000000FFFFFFFFULL) |
                          ((uint64_t)(uint32_t)val << 32);
        break;
    case DMA_CHAN_DST_LO:
        chan->dst_addr = (chan->dst_addr & 0xFFFFFFFF00000000ULL) |
                          (uint32_t)val;
        break;
    case DMA_CHAN_DST_HI:
        chan->dst_addr = (chan->dst_addr & 0x00000000FFFFFFFFULL) |
                          ((uint64_t)(uint32_t)val << 32);
        break;
    case DMA_CHAN_LENGTH:
        chan->length = (uint32_t)val;
        break;
    case DMA_CHAN_FLAGS:
        chan->flags = (uint32_t)val;
        break;
    case DMA_CHAN_CONTROL:
        if (val == 1) {
            dma_execute_transfer(s, ch);
        } else if (val == 2) {
            dma_abort_transfer(s, ch);
        }
        break;
    case DMA_CHAN_STATUS:
        /* Write clears status + lowers IRQ */
        chan->status = DMA_STATUS_IDLE;
        chan->bytes_transferred = 0;
        qemu_irq_lower(base->irq);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps dma_stub_ops = {
    .read  = dma_stub_read,
    .write = dma_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps *dma_stub_get_region_ops(ChardevStub *s)
{
    return &dma_stub_ops;
}

static bool dma_stub_post_realize(ChardevStub *s, Error **errp)
{
    DmaStub *dev = DMA_STUB(s);

    if (dev->num_channels < 1 || dev->num_channels > DMA_MAX_CHANNELS) {
        error_setg(errp, "dma-stub: num-channels must be 1..%u (got %u)",
                   DMA_MAX_CHANNELS, dev->num_channels);
        return false;
    }

    /*
     * Override the default MMIO region size to fit all channels.
     * The base class already created the region with s->size, but we
     * validate it's large enough here.
     */
    uint64_t needed = (uint64_t)dev->num_channels * DMA_CHAN_STRIDE;
    if (s->size < needed) {
        error_setg(errp, "dma-stub: region size 0x%"PRIx64" too small for "
                   "%u channels (need 0x%"PRIx64")",
                   s->size, dev->num_channels, needed);
        return false;
    }

    return true;
}

static const Property dma_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", DmaStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", DmaStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("num-channels", DmaStub, num_channels, 1),
    DEFINE_PROP_UINT32("max-transfer-size", DmaStub, max_transfer_size,
                       0x100000),
};

static void dma_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = dma_stub_get_region_ops;
    csc->post_realize   = dma_stub_post_realize;
    device_class_set_props(dc, dma_stub_properties);
}

static const TypeInfo dma_stub_info = {
    .name          = TYPE_DMA_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(DmaStub),
    .class_init    = dma_stub_class_init,
};

static void dma_stub_register_types(void)
{
    type_register_static(&dma_stub_info);
}

type_init(dma_stub_register_types)
