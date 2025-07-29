#ifndef HW_MMIO_STUB_H
#define HW_MMIO_STUB_H

#include "hw/sysbus.h"
#include "chardev/char-fe.h"
#include "qom/object.h"

#define TYPE_MMIO_STUB "mmio-stub"
OBJECT_DECLARE_SIMPLE_TYPE(MMIOStub, MMIO_STUB)

#define MMIO_STUB_DEFAULT_SIZE 0x1000

enum mmio_stub_op {
    MMIO_STUB_READ,
    MMIO_STUB_WRITE
};

struct mmio_stub_msg_hdr {
    uint8_t op;
    uint8_t size;
    uint64_t addr;
    uint64_t val;
} QEMU_PACKED;

struct MMIOStub {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    CharBackend chr;
    qemu_irq irq;
    uint64_t size;
};

#endif /* HW_MMIO_STUB_H */

