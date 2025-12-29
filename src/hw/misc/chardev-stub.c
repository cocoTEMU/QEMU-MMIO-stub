#include "qemu/osdep.h"
#include "hw/misc/chardev-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"

/* Helper functions */

bool chardev_stub_is_connected(ChardevStub *s, const char *op_name,
                               hwaddr addr)
{
    if (!qemu_chr_fe_backend_connected(&s->chr)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: no chardev connected for %s at 0x%"HWADDR_PRIx"\n",
                      object_get_typename(OBJECT(s)), op_name, addr);
        return false;
    }
    return true;
}

bool chardev_stub_check_bounds(ChardevStub *s, hwaddr addr,
                               const char *op_name)
{
    if (addr >= s->size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: out-of-bounds %s at 0x%"HWADDR_PRIx
                      " (region size 0x%"PRIx64")\n",
                      object_get_typename(OBJECT(s)), op_name, addr, s->size);
        return false;
    }
    return true;
}

bool chardev_stub_send(ChardevStub *s, const void *buf, size_t len)
{
    if (!qemu_chr_fe_backend_connected(&s->chr)) {
        return false;
    }
    int ret = qemu_chr_fe_write_all(&s->chr, (const uint8_t *)buf, len);
    if (ret != (int)len) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: chardev send failed (%d/%zu bytes)\n",
                      object_get_typename(OBJECT(s)), ret, len);
        return false;
    }
    return true;
}

bool chardev_stub_recv(ChardevStub *s, void *buf, size_t len)
{
    int ret = qemu_chr_fe_read_all(&s->chr, (uint8_t *)buf, len);
    if (ret != (int)len) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: chardev recv failed (%d/%zu bytes)\n",
                      object_get_typename(OBJECT(s)), ret, len);
        return false;
    }
    return true;
}

/* Default virtual methods */

static const char *chardev_stub_default_region_name(ChardevStub *s)
{
    return object_get_typename(OBJECT(s));
}

/* Realize */

static void chardev_stub_realize(DeviceState *dev, Error **errp)
{
    ChardevStub *s = CHARDEV_STUB(dev);
    ChardevStubClass *csc = CHARDEV_STUB_GET_CLASS(s);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    uint64_t region_size = s->size ? s->size : CHARDEV_STUB_DEFAULT_SIZE;

    /* Get subclass memory region ops */
    const MemoryRegionOps *ops = NULL;
    if (csc->get_region_ops) {
        ops = csc->get_region_ops(s);
    }

    if (ops) {
        const char *name = csc->get_region_name
                           ? csc->get_region_name(s)
                           : chardev_stub_default_region_name(s);
        memory_region_init_io(&s->iomem, OBJECT(s), ops, s,
                              name, region_size);
        sysbus_init_mmio(sbd, &s->iomem);
    }

    /* Set up chardev handlers (synchronous — no async read handler) */
    qemu_chr_fe_set_handlers(&s->chr, NULL, NULL, NULL, NULL,
                             s, NULL, true);

    /* Call subclass post_realize if provided */
    if (csc->post_realize) {
        if (!csc->post_realize(s, errp)) {
            return;
        }
    }
}

/* Instance init */

static void chardev_stub_init(Object *obj)
{
    ChardevStub *s = CHARDEV_STUB(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);
}

/* Properties */

static const Property chardev_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", ChardevStub, chr),
    DEFINE_PROP_UINT64("size", ChardevStub, size, CHARDEV_STUB_DEFAULT_SIZE),
};

/* Class init */

static void chardev_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    dc->realize = chardev_stub_realize;
    device_class_set_props(dc, chardev_stub_properties);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    /* Default virtual methods */
    csc->get_region_name = chardev_stub_default_region_name;
}

/* Type registration */

static const TypeInfo chardev_stub_info = {
    .name          = TYPE_CHARDEV_STUB,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ChardevStub),
    .instance_init = chardev_stub_init,
    .class_size    = sizeof(ChardevStubClass),
    .class_init    = chardev_stub_class_init,
    .abstract      = true,
};

static void chardev_stub_register_types(void)
{
    type_register_static(&chardev_stub_info);
}

type_init(chardev_stub_register_types)
