#ifndef HW_CHARDEV_STUB_H
#define HW_CHARDEV_STUB_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define TYPE_CHARDEV_STUB "chardev-stub"

OBJECT_DECLARE_TYPE(ChardevStub, ChardevStubClass, CHARDEV_STUB)

#define CHARDEV_STUB_DEFAULT_SIZE 0x1000

/* Instance state — inherited by every protocol subclass */
struct ChardevStub {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    CharBackend  chr;
    qemu_irq     irq;
    uint64_t     size;
};

/* Class struct with virtual methods for protocol subclasses */
struct ChardevStubClass {
    SysBusDeviceClass parent_class;

    /*
     * get_region_ops — REQUIRED (unless protocol is address-less).
     * Returns a pointer to the subclass's static MemoryRegionOps.
     * Called during realize to initialize the memory region.
     * Return NULL for address-less protocols (e.g. AXI-Stream)
     * that handle region setup in post_realize instead.
     */
    const MemoryRegionOps *(*get_region_ops)(ChardevStub *s);

    /*
     * get_region_name — OPTIONAL.
     * Human-readable name for the MemoryRegion.
     * Defaults to the QOM type name if not overridden.
     */
    const char *(*get_region_name)(ChardevStub *s);

    /*
     * post_realize — OPTIONAL.
     * Called after the base class realize completes.
     * Subclass can do additional setup (extra regions, validation, etc.).
     * Return false and set errp on failure.
     */
    bool (*post_realize)(ChardevStub *s, Error **errp);
};

/*
 * chardev_stub_send — send len bytes from buf over the chardev.
 * Returns true on success. Logs error on failure.
 */
bool chardev_stub_send(ChardevStub *s, const void *buf, size_t len);

/*
 * chardev_stub_recv — receive exactly len bytes into buf.
 * Returns true on success. Logs error on failure.
 */
bool chardev_stub_recv(ChardevStub *s, void *buf, size_t len);

/*
 * chardev_stub_is_connected — check if chardev backend is connected.
 * Logs LOG_GUEST_ERROR with op_name context if not connected.
 */
bool chardev_stub_is_connected(ChardevStub *s, const char *op_name,
                               hwaddr addr);

/*
 * chardev_stub_check_bounds — check addr < s->size.
 * Logs LOG_GUEST_ERROR if out of bounds.
 */
bool chardev_stub_check_bounds(ChardevStub *s, hwaddr addr,
                               const char *op_name);

#endif /* HW_CHARDEV_STUB_H */
