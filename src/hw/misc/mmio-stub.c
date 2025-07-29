#include "qemu/osdep.h"
#include "hw/misc/mmio-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "chardev/char-fe.h"
#include "qapi/error.h"

static uint64_t mmio_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    MMIOStub *dev = MMIO_STUB(opaque);
    struct mmio_stub_msg_hdr hdr = {
        .op = MMIO_STUB_READ,
        .addr = addr,
        .size = len,
        .val = 0
    };
    uint64_t buf = 0;

    if (addr >= dev->size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: out-of-bounds read at 0x%"HWADDR_PRIx
                     " (size 0x%"PRIx64")\n", addr, dev->size);
        return 0;
    }

    if (!qemu_chr_fe_backend_connected(&dev->chr)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: no chardev connected for read at 0x%"
                     HWADDR_PRIx "\n", addr);
        return 0;
    }

    // Send read request
    qemu_chr_fe_write_all(&dev->chr, (uint8_t *)&hdr, sizeof(hdr));

    // Read response
    if (qemu_chr_fe_read_all(&dev->chr, (uint8_t *)&buf, len) != len) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: failed to read response for addr 0x%"
                     HWADDR_PRIx "\n", addr);
        return 0;
    }

    return buf;
}

static void mmio_stub_write(void *opaque, hwaddr addr, uint64_t val, unsigned len)
{
    MMIOStub *dev = MMIO_STUB(opaque);
    struct mmio_stub_msg_hdr hdr = {
        .op = MMIO_STUB_WRITE,
        .addr = addr,
        .size = len,
        .val = val
    };
    uint8_t ok = 0;

    if (addr >= dev->size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: out-of-bounds write at 0x%"HWADDR_PRIx
                     " (size 0x%"PRIx64")\n", addr, dev->size);
        return;
    }

    if (!qemu_chr_fe_backend_connected(&dev->chr)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: no chardev connected for write at 0x%"
                     HWADDR_PRIx "\n", addr);
        return;
    }

    // Send write request
    qemu_chr_fe_write_all(&dev->chr, (uint8_t *)&hdr, sizeof(hdr));

    // Read acknowledgment
    if (qemu_chr_fe_read_all(&dev->chr, &ok, sizeof(ok)) != sizeof(ok)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                     "mmio-stub: failed to read ack for write at 0x%"
                     HWADDR_PRIx "\n", addr);
    }
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

// Realizes the MMIO stub device (Initializes its memory region and chardev handlers).
static void mmio_stub_realize(DeviceState *dev, Error **errp)
{
    MMIOStub *s = MMIO_STUB(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    uint64_t region_size = s->size ? s->size : MMIO_STUB_DEFAULT_SIZE;

    memory_region_init_io(&s->iomem, OBJECT(s), &mmio_stub_ops, s,
                         "mmio-stub", region_size);
    sysbus_init_mmio(sbd, &s->iomem);

    qemu_chr_fe_set_handlers(&s->chr, NULL, NULL, NULL, NULL, s, NULL, true);
}

static void mmio_stub_init(Object *obj)
{
    MMIOStub *s = MMIO_STUB(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbd, &s->irq);
}

static const Property mmio_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", MMIOStub, chr),
    DEFINE_PROP_UINT64("size", MMIOStub, size, MMIO_STUB_DEFAULT_SIZE),
};

static void mmio_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = mmio_stub_realize;
    device_class_set_props(dc, mmio_stub_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo mmio_stub_info = {
    .name          = TYPE_MMIO_STUB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MMIOStub),
    .instance_init = mmio_stub_init,
    .class_init    = mmio_stub_class_init,
};

static void mmio_stub_register_types(void)
{
    type_register_static(&mmio_stub_info);
}

type_init(mmio_stub_register_types)

