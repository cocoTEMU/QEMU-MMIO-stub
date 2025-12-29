#include "qemu/osdep.h"
#include "hw/misc/avalon-mm-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t avalon_mm_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    AvalonMmStub *s = AVALON_MM_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    uint32_t byteenable = (1U << len) - 1;

    struct avalon_mm_req req = {
        .op         = AVALON_MM_READ,
        .burstcount = 1,
        .byteenable = byteenable,
        .addr       = addr,
        .wdata      = 0,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct avalon_mm_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.response != AVALON_MM_RESP_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "avalon-mm-stub: resp=%u on read at 0x%"HWADDR_PRIx"\n",
                      resp.response, addr);
    }

    return resp.rdata;
}

static void avalon_mm_stub_write(void *opaque, hwaddr addr, uint64_t val,
                                 unsigned len)
{
    AvalonMmStub *s = AVALON_MM_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    uint32_t byteenable = (1U << len) - 1;

    struct avalon_mm_req req = {
        .op         = AVALON_MM_WRITE,
        .burstcount = 1,
        .byteenable = byteenable,
        .addr       = addr,
        .wdata      = val,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct avalon_mm_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.response != AVALON_MM_RESP_OK) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "avalon-mm-stub: resp=%u on write at "
                      "0x%"HWADDR_PRIx"\n", resp.response, addr);
    }
}

static const MemoryRegionOps avalon_mm_stub_ops = {
    .read  = avalon_mm_stub_read,
    .write = avalon_mm_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *avalon_mm_stub_get_region_ops(ChardevStub *s)
{
    return &avalon_mm_stub_ops;
}

static const Property avalon_mm_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", AvalonMmStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", AvalonMmStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", AvalonMmStub, data_width, 32),
    DEFINE_PROP_UINT32("max-burstcount", AvalonMmStub, max_burstcount, 1),
};

static void avalon_mm_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = avalon_mm_stub_get_region_ops;
    device_class_set_props(dc, avalon_mm_stub_properties);
}

static const TypeInfo avalon_mm_stub_info = {
    .name          = TYPE_AVALON_MM_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(AvalonMmStub),
    .class_init    = avalon_mm_stub_class_init,
};

static void avalon_mm_stub_register_types(void)
{
    type_register_static(&avalon_mm_stub_info);
}

type_init(avalon_mm_stub_register_types)
