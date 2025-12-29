#include "qemu/osdep.h"
#include "hw/misc/ahb-lite-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

/* Compute HSIZE (log2 of access size) from byte count */
static uint8_t ahb_hsize_from_len(unsigned len)
{
    uint8_t hsize = 0;
    unsigned v = len;
    while (v > 1) {
        v >>= 1;
        hsize++;
    }
    return hsize;
}

static uint64_t ahb_lite_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    AhbLiteStub *s = AHB_LITE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    /* Validate access size is power of 2 and within bus width */
    if ((len & (len - 1)) != 0 || len > (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ahb-lite-stub: invalid access size %u "
                      "(bus width %u bits)\n", len, s->data_width);
        return 0;
    }

    struct ahb_lite_req req = {
        .op     = AHB_LITE_READ,
        .hsize  = ahb_hsize_from_len(len),
        .htrans = AHB_HTRANS_NONSEQ,
        .hburst = AHB_HBURST_SINGLE,
        .addr   = addr,
        .wdata  = 0,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct ahb_lite_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.hresp) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ahb-lite-stub: ERROR response on read at "
                      "0x%"HWADDR_PRIx"\n", addr);
    }

    return resp.rdata;
}

static void ahb_lite_stub_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned len)
{
    AhbLiteStub *s = AHB_LITE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    if ((len & (len - 1)) != 0 || len > (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ahb-lite-stub: invalid access size %u "
                      "(bus width %u bits)\n", len, s->data_width);
        return;
    }

    struct ahb_lite_req req = {
        .op     = AHB_LITE_WRITE,
        .hsize  = ahb_hsize_from_len(len),
        .htrans = AHB_HTRANS_NONSEQ,
        .hburst = AHB_HBURST_SINGLE,
        .addr   = addr,
        .wdata  = val,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct ahb_lite_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.hresp) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "ahb-lite-stub: ERROR response on write at "
                      "0x%"HWADDR_PRIx"\n", addr);
    }
}

static const MemoryRegionOps ahb_lite_stub_ops = {
    .read  = ahb_lite_stub_read,
    .write = ahb_lite_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *ahb_lite_stub_get_region_ops(ChardevStub *s)
{
    return &ahb_lite_stub_ops;
}

static const Property ahb_lite_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", AhbLiteStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", AhbLiteStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", AhbLiteStub, data_width, 32),
};

static void ahb_lite_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = ahb_lite_stub_get_region_ops;
    device_class_set_props(dc, ahb_lite_stub_properties);
}

static const TypeInfo ahb_lite_stub_info = {
    .name          = TYPE_AHB_LITE_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(AhbLiteStub),
    .class_init    = ahb_lite_stub_class_init,
};

static void ahb_lite_stub_register_types(void)
{
    type_register_static(&ahb_lite_stub_info);
}

type_init(ahb_lite_stub_register_types)
