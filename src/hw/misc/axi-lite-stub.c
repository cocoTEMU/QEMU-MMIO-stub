#include "qemu/osdep.h"
#include "hw/misc/axi-lite-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t axi_lite_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    AxiLiteStub *s = AXI_LITE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);
    uint32_t word_size = s->data_width / 8;

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    /* AXI-Lite: access must match data width */
    if (len != word_size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: access size %u != data-width %u bytes\n",
                      len, word_size);
        return 0;
    }

    /* AXI-Lite: address must be naturally aligned */
    if (addr & (word_size - 1)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: unaligned read at 0x%"HWADDR_PRIx
                      " (alignment %u)\n", addr, word_size);
        return 0;
    }

    struct axi_lite_req req = {
        .op    = AXI_LITE_READ,
        .wstrb = 0,
        .addr  = addr,
        .wdata = 0,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct axi_lite_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.resp != AXI_LITE_RESP_OKAY) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: read resp=%u at 0x%"HWADDR_PRIx"\n",
                      resp.resp, addr);
    }

    return resp.rdata;
}

static void axi_lite_stub_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned len)
{
    AxiLiteStub *s = AXI_LITE_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);
    uint32_t word_size = s->data_width / 8;

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    if (len != word_size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: access size %u != data-width %u bytes\n",
                      len, word_size);
        return;
    }

    if (addr & (word_size - 1)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: unaligned write at 0x%"HWADDR_PRIx
                      " (alignment %u)\n", addr, word_size);
        return;
    }

    struct axi_lite_req req = {
        .op    = AXI_LITE_WRITE,
        .wstrb = (1U << len) - 1,
        .addr  = addr,
        .wdata = val,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    struct axi_lite_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.resp != AXI_LITE_RESP_OKAY) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-lite-stub: write resp=%u at 0x%"HWADDR_PRIx"\n",
                      resp.resp, addr);
    }
}

static const MemoryRegionOps axi_lite_stub_ops = {
    .read  = axi_lite_stub_read,
    .write = axi_lite_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *axi_lite_stub_get_region_ops(ChardevStub *s)
{
    return &axi_lite_stub_ops;
}

static const Property axi_lite_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", AxiLiteStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", AxiLiteStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", AxiLiteStub, data_width, 32),
};

static void axi_lite_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = axi_lite_stub_get_region_ops;
    device_class_set_props(dc, axi_lite_stub_properties);
}

static const TypeInfo axi_lite_stub_info = {
    .name          = TYPE_AXI_LITE_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(AxiLiteStub),
    .class_init    = axi_lite_stub_class_init,
};

static void axi_lite_stub_register_types(void)
{
    type_register_static(&axi_lite_stub_info);
}

type_init(axi_lite_stub_register_types)
