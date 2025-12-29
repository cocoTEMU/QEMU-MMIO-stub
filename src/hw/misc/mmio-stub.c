#include "qemu/osdep.h"
#include "hw/misc/mmio-stub.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "qemu/module.h"

static uint64_t mmio_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    ChardevStub *base = CHARDEV_STUB(opaque);
    struct mmio_stub_msg_hdr hdr = {
        .op = MMIO_STUB_READ,
        .addr = addr,
        .size = len,
        .val = 0
    };
    uint64_t buf = 0;

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    if (!chardev_stub_send(base, &hdr, sizeof(hdr))) {
        return 0;
    }

    if (!chardev_stub_recv(base, &buf, len)) {
        return 0;
    }

    return buf;
}

static void mmio_stub_write(void *opaque, hwaddr addr, uint64_t val,
                            unsigned len)
{
    ChardevStub *base = CHARDEV_STUB(opaque);
    struct mmio_stub_msg_hdr hdr = {
        .op = MMIO_STUB_WRITE,
        .addr = addr,
        .size = len,
        .val = val
    };
    uint8_t ok = 0;

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    if (!chardev_stub_send(base, &hdr, sizeof(hdr))) {
        return;
    }

    chardev_stub_recv(base, &ok, sizeof(ok));
}

static const MemoryRegionOps mmio_stub_ops = {
    .read = mmio_stub_read,
    .write = mmio_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *mmio_stub_get_region_ops(ChardevStub *s)
{
    return &mmio_stub_ops;
}

static void mmio_stub_class_init(ObjectClass *klass, const void *data)
{
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = mmio_stub_get_region_ops;
}

static const TypeInfo mmio_stub_info = {
    .name          = TYPE_MMIO_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(MMIOStub),
    .class_init    = mmio_stub_class_init,
};

static void mmio_stub_register_types(void)
{
    type_register_static(&mmio_stub_info);
}

type_init(mmio_stub_register_types)
