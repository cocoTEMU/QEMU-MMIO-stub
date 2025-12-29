#include "qemu/osdep.h"
#include "hw/misc/axi4-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"

/* Helpers */

static uint8_t axi4_size_from_len(unsigned len)
{
    uint8_t s = 0;
    unsigned v = len;
    while (v > 1) {
        v >>= 1;
        s++;
    }
    return s;
}

/* Single-beat MMIO ops (standard MemoryRegion access) */

static uint64_t axi4_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    Axi4Stub *s = AXI4_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "read") ||
        !chardev_stub_is_connected(base, "read", addr)) {
        return 0;
    }

    if ((len & (len - 1)) != 0 || len > (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi4-stub: invalid access size %u "
                      "(bus width %u bits)\n", len, s->data_width);
        return 0;
    }

    /* Single-beat read: len=0, burst=INCR */
    struct axi4_req_hdr hdr = {
        .op    = AXI4_READ,
        .size  = axi4_size_from_len(len),
        .burst = AXI4_BURST_INCR,
        .len   = 0,
        .addr  = addr,
    };

    if (!chardev_stub_send(base, &hdr, sizeof(hdr))) {
        return 0;
    }

    /* Receive one beat of data */
    uint64_t rdata = 0;
    if (!chardev_stub_recv(base, &rdata, len)) {
        return 0;
    }

    /* Receive response */
    struct axi4_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return 0;
    }

    if (resp.resp != AXI4_RESP_OKAY && resp.resp != AXI4_RESP_EXOKAY) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi4-stub: read resp=%u at 0x%"HWADDR_PRIx"\n",
                      resp.resp, addr);
    }

    return rdata;
}

static void axi4_stub_write(void *opaque, hwaddr addr, uint64_t val,
                            unsigned len)
{
    Axi4Stub *s = AXI4_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_check_bounds(base, addr, "write") ||
        !chardev_stub_is_connected(base, "write", addr)) {
        return;
    }

    if ((len & (len - 1)) != 0 || len > (s->data_width / 8)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi4-stub: invalid access size %u "
                      "(bus width %u bits)\n", len, s->data_width);
        return;
    }

    /* Single-beat write: len=0, burst=INCR */
    struct axi4_req_hdr hdr = {
        .op    = AXI4_WRITE,
        .size  = axi4_size_from_len(len),
        .burst = AXI4_BURST_INCR,
        .len   = 0,
        .addr  = addr,
    };

    if (!chardev_stub_send(base, &hdr, sizeof(hdr))) {
        return;
    }

    /* Send one write beat: data + wstrb + wlast */
    if (!chardev_stub_send(base, &val, len)) {
        return;
    }
    uint8_t wstrb_byte = (1U << len) - 1;
    if (!chardev_stub_send(base, &wstrb_byte, 1)) {
        return;
    }
    uint8_t wlast = 1;
    if (!chardev_stub_send(base, &wlast, 1)) {
        return;
    }

    /* Receive response */
    struct axi4_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.resp != AXI4_RESP_OKAY && resp.resp != AXI4_RESP_EXOKAY) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi4-stub: write resp=%u at 0x%"HWADDR_PRIx"\n",
                      resp.resp, addr);
    }
}

static const MemoryRegionOps axi4_stub_ops = {
    .read  = axi4_stub_read,
    .write = axi4_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

/* Burst engine control region */

static uint64_t axi4_burst_read(void *opaque, hwaddr addr, unsigned len)
{
    Axi4Stub *s = AXI4_STUB(opaque);

    switch (addr) {
    case AXI4_BURST_ADDR_LO:
        return (uint32_t)(s->burst_addr);
    case AXI4_BURST_ADDR_HI:
        return (uint32_t)(s->burst_addr >> 32);
    case AXI4_BURST_LEN:
        return s->burst_len;
    case AXI4_BURST_SIZE:
        return s->burst_size;
    case AXI4_BURST_TYPE:
        return s->burst_type;
    case AXI4_BURST_STATUS:
        return s->burst_status;
    default:
        return 0;
    }
}

static void axi4_burst_execute(Axi4Stub *s, bool is_write)
{
    ChardevStub *base = CHARDEV_STUB(s);
    uint32_t beat_bytes = 1U << s->burst_size;
    uint32_t num_beats = (uint32_t)s->burst_len + 1;
    uint32_t strb_bytes = (beat_bytes + 7) / 8;

    if (!chardev_stub_is_connected(base, "burst", s->burst_addr)) {
        s->burst_status = 3; /* error */
        return;
    }

    struct axi4_req_hdr hdr = {
        .op    = is_write ? AXI4_WRITE : AXI4_READ,
        .size  = s->burst_size,
        .burst = s->burst_type,
        .len   = s->burst_len,
        .addr  = s->burst_addr,
    };

    s->burst_status = 1; /* busy */

    if (!chardev_stub_send(base, &hdr, sizeof(hdr))) {
        s->burst_status = 3;
        return;
    }

    if (is_write) {
        /*
         * For burst writes, send data from the FIFO window.
         * In this stub, we send zeroes — real usage would have the guest
         * pre-fill a data buffer. This demonstrates the protocol framing.
         */
        uint8_t beat[AXI4_MAX_DATA_BYTES] = {};
        uint8_t strb[AXI4_MAX_DATA_BYTES / 8];
        memset(strb, 0xFF, strb_bytes);

        for (uint32_t i = 0; i < num_beats; i++) {
            if (!chardev_stub_send(base, beat, beat_bytes)) {
                s->burst_status = 3;
                return;
            }
            if (!chardev_stub_send(base, strb, strb_bytes)) {
                s->burst_status = 3;
                return;
            }
            uint8_t wlast = (i == num_beats - 1) ? 1 : 0;
            if (!chardev_stub_send(base, &wlast, 1)) {
                s->burst_status = 3;
                return;
            }
        }
    } else {
        /* For burst reads, receive data beats (discarded in this stub) */
        uint8_t beat[AXI4_MAX_DATA_BYTES];
        for (uint32_t i = 0; i < num_beats; i++) {
            if (!chardev_stub_recv(base, beat, beat_bytes)) {
                s->burst_status = 3;
                return;
            }
        }
    }

    /* Receive final response */
    struct axi4_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        s->burst_status = 3;
        return;
    }

    s->burst_status = (resp.resp == AXI4_RESP_OKAY ||
                       resp.resp == AXI4_RESP_EXOKAY) ? 2 : 3;
}

static void axi4_burst_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned len)
{
    Axi4Stub *s = AXI4_STUB(opaque);

    switch (addr) {
    case AXI4_BURST_ADDR_LO:
        s->burst_addr = (s->burst_addr & 0xFFFFFFFF00000000ULL) |
                         (uint32_t)val;
        break;
    case AXI4_BURST_ADDR_HI:
        s->burst_addr = (s->burst_addr & 0x00000000FFFFFFFFULL) |
                         ((uint64_t)(uint32_t)val << 32);
        break;
    case AXI4_BURST_LEN:
        s->burst_len = (uint8_t)val;
        break;
    case AXI4_BURST_SIZE:
        if (val > 7) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "axi4-stub: invalid burst_size %u (max 7)\n",
                          (unsigned)val);
            break;
        }
        s->burst_size = (uint8_t)val;
        break;
    case AXI4_BURST_TYPE:
        if (val > AXI4_BURST_WRAP) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "axi4-stub: invalid burst_type %u\n",
                          (unsigned)val);
            break;
        }
        s->burst_type = (uint8_t)val;
        break;
    case AXI4_BURST_GO:
        if (val == 1) {
            axi4_burst_execute(s, false); /* read burst */
        } else if (val == 2) {
            axi4_burst_execute(s, true);  /* write burst */
        }
        break;
    case AXI4_BURST_STATUS:
        /* Write to status clears it back to idle */
        s->burst_status = 0;
        break;
    default:
        break;
    }
}

static const MemoryRegionOps axi4_burst_ops = {
    .read  = axi4_burst_read,
    .write = axi4_burst_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* Virtual methods & registration */

static const MemoryRegionOps *axi4_stub_get_region_ops(ChardevStub *s)
{
    return &axi4_stub_ops;
}

static bool axi4_stub_post_realize(ChardevStub *s, Error **errp)
{
    Axi4Stub *dev = AXI4_STUB(s);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);

    /* Validate data_width is power of 2 in range [8, 1024] */
    if (dev->data_width < 8 || dev->data_width > 1024 ||
        (dev->data_width & (dev->data_width - 1)) != 0) {
        error_setg(errp, "axi4-stub: data-width must be power of 2, "
                   "8..1024 (got %u)", dev->data_width);
        return false;
    }

    /* Initialize burst engine control region */
    memory_region_init_io(&dev->burst_iomem, OBJECT(dev), &axi4_burst_ops,
                          dev, "axi4-burst", AXI4_BURST_REGION_SIZE);
    sysbus_init_mmio(sbd, &dev->burst_iomem);

    return true;
}

static const Property axi4_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", Axi4Stub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", Axi4Stub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("data-width", Axi4Stub, data_width, 32),
    DEFINE_PROP_UINT32("max-burst-len", Axi4Stub, max_burst_len, 256),
};

static void axi4_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = axi4_stub_get_region_ops;
    csc->post_realize   = axi4_stub_post_realize;
    device_class_set_props(dc, axi4_stub_properties);
}

static const TypeInfo axi4_stub_info = {
    .name          = TYPE_AXI4_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(Axi4Stub),
    .class_init    = axi4_stub_class_init,
};

static void axi4_stub_register_types(void)
{
    type_register_static(&axi4_stub_info);
}

type_init(axi4_stub_register_types)
