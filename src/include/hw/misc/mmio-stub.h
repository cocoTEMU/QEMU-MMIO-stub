#ifndef HW_MMIO_STUB_H
#define HW_MMIO_STUB_H

#include "hw/misc/chardev-stub.h"

#define TYPE_MMIO_STUB "mmio-stub"
OBJECT_DECLARE_SIMPLE_TYPE(MMIOStub, MMIO_STUB)

/* Wire protocol — kept identical for backward compatibility */
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
    ChardevStub parent_obj;
    /* No additional fields — pure compatibility wrapper */
};

#endif /* HW_MMIO_STUB_H */
