#include "qemu/osdep.h"
#include "hw/misc/apb-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t apb_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    ApbStub *s = APB_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    if (len != (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "apb-stub: access size %u != data-width %u bytes\n",
                      len, s->data_width / 8);
        return 0;
    }

    struct apb_req req = {
        .op    = APB_READ,
        .pstrb = 0,
        .addr  = (uint32_t)addr,
        .wdata = 0,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct apb_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.pslverr) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "apb-stub: PSLVERR on read at 0x%"HWADDR_PRIx"\n",
                      addr);
    }

    return resp.rdata;
}

static void apb_stub_write(void *opaque, hwaddr addr, uint64_t val,
                           unsigned len)
{
    ApbStub *s = APB_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    if (len != (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "apb-stub: access size %u != data-width %u bytes\n",
                      len, s->data_width / 8);
        return;
    }

    uint8_t pstrb = (1U << len) - 1;

    struct apb_req req = {
        .op    = APB_WRITE,
        .pstrb = pstrb,
        .addr  = (uint32_t)addr,
        .wdata = (uint32_t)val,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct apb_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.pslverr) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "apb-stub: PSLVERR on write at 0x%"HWADDR_PRIx"\n",
                      addr);
    }
}

static const MemoryRegionOps apb_stub_ops = {
    .read  = apb_stub_read,
    .write = apb_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static const MemoryRegionOps *apb_stub_get_region_ops(ChardevStub *s)
{
    return &apb_stub_ops;
}

static const Property apb_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", ApbStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", ApbStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", ApbStub, data_width, 32),
};

static void apb_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = apb_stub_get_region_ops;
    device_class_set_props(dc, apb_stub_properties);
}

static const TypeInfo apb_stub_info = {
    .name          = TYPE_APB_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(ApbStub),
    .class_init    = apb_stub_class_init,
};

static void apb_stub_register_types(void)
{
    type_register_static(&apb_stub_info);
}

type_init(apb_stub_register_types)
