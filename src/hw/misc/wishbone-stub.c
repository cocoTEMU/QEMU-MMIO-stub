#include "qemu/osdep.h"
#include "hw/misc/wishbone-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t wishbone_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    WishboneStub *s = WISHBONE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    /* SEL: byte-lane select mask based on access size */
    uint8_t sel = (1U << len) - 1;

    struct wb_req req = {
        .op    = WB_READ,
        .sel   = sel,
        .cti   = s->pipelined ? WB_CTI_END_BURST : WB_CTI_CLASSIC,
        .bte   = WB_BTE_LINEAR,
        .addr  = addr,
        .wdata = 0,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct wb_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.err) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wishbone-stub: ERR on read at 0x%"HWADDR_PRIx"\n",
                      addr);
        return 0;
    }

    if (resp.rty) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wishbone-stub: RTY on read at 0x%"HWADDR_PRIx"\n",
                      addr);
        return 0;
    }

    return resp.rdata;
}

static void wishbone_stub_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned len)
{
    WishboneStub *s = WISHBONE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    uint8_t sel = (1U << len) - 1;

    struct wb_req req = {
        .op    = WB_WRITE,
        .sel   = sel,
        .cti   = s->pipelined ? WB_CTI_END_BURST : WB_CTI_CLASSIC,
        .bte   = WB_BTE_LINEAR,
        .addr  = addr,
        .wdata = val,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct wb_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.err) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wishbone-stub: ERR on write at 0x%"HWADDR_PRIx"\n",
                      addr);
    }

    if (resp.rty) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "wishbone-stub: RTY on write at 0x%"HWADDR_PRIx"\n",
                      addr);
    }
}

static const MemoryRegionOps wishbone_stub_ops = {
    .read  = wishbone_stub_read,
    .write = wishbone_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *wishbone_stub_get_region_ops(ChardevStub *s)
{
    return &wishbone_stub_ops;
}

static const Property wishbone_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", WishboneStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", WishboneStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", WishboneStub, data_width, 32),
    DEFINE_PROP_UINT32("granularity", WishboneStub, granularity, 8),
    DEFINE_PROP_BOOL("pipelined", WishboneStub, pipelined, false),
};

static void wishbone_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = wishbone_stub_get_region_ops;
    device_class_set_props(dc, wishbone_stub_properties);
}

static const TypeInfo wishbone_stub_info = {
    .name          = TYPE_WISHBONE_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(WishboneStub),
    .class_init    = wishbone_stub_class_init,
};

static void wishbone_stub_register_types(void)
{
    type_register_static(&wishbone_stub_info);
}

type_init(wishbone_stub_register_types)
