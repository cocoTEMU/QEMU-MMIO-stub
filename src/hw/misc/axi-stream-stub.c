#include "qemu/osdep.h"
#include "hw/misc/axi-stream-stub.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "qemu/log.h"
#include "qemu/module.h"

/*
 * AXI-Stream is address-less. The MMIO region acts as a FIFO window:
 *   - Guest writes to offset 0 => PUSH data into stream
 *   - Guest reads from offset 0 => PULL data from stream
 * The address field is ignored (or could select a stream ID in the future).
 */

static uint64_t axi_stream_stub_read(void *opaque, hwaddr addr, unsigned len)
{
    AxiStreamStub *s = AXI_STREAM_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_is_connected(base, "pull", addr)) {
        return 0;
    }

    struct axi_stream_pull_req req = {
        .op      = AXI_STREAM_PULL,
        .max_len = len,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return 0;
    }

    struct axi_stream_pull_resp resp_hdr = {};
    if (!chardev_stub_recv(base, &resp_hdr, sizeof(resp_hdr))) {
        return 0;
    }

    uint64_t data = 0;
    uint32_t to_read = resp_hdr.tdata_len;
    if (to_read > sizeof(data)) {
        to_read = sizeof(data);
    }

    if (to_read > 0) {
        if (!chardev_stub_recv(base, &data, to_read)) {
            return 0;
        }
    }

    /* Drain excess TDATA bytes to keep the chardev stream in sync */
    if (resp_hdr.tdata_len > to_read) {
        uint32_t excess = resp_hdr.tdata_len - to_read;
        uint8_t drain[AXI_STREAM_MAX_TDATA_BYTES];
        while (excess > 0) {
            uint32_t chunk = (excess > sizeof(drain)) ? sizeof(drain) : excess;
            if (!chardev_stub_recv(base, drain, chunk)) {
                return 0;
            }
            excess -= chunk;
        }
    }

    /* Consume TKEEP bytes */
    if (resp_hdr.tkeep_len > 0) {
        uint8_t tkeep[AXI_STREAM_MAX_TDATA_BYTES / 8];
        uint32_t keep_read = resp_hdr.tkeep_len;
        if (keep_read > sizeof(tkeep)) {
            keep_read = sizeof(tkeep);
        }
        if (!chardev_stub_recv(base, tkeep, keep_read)) {
            return 0;
        }
    }

    return data;
}

static void axi_stream_stub_write(void *opaque, hwaddr addr, uint64_t val,
                                  unsigned len)
{
    AxiStreamStub *s = AXI_STREAM_STUB(opaque);
    ChardevStub *base = CHARDEV_STUB(s);

    if (!chardev_stub_is_connected(base, "push", addr)) {
        return;
    }

    uint32_t tkeep_bytes = (len + 7) / 8;

    struct axi_stream_push_req req = {
        .op        = AXI_STREAM_PUSH,
        .tlast     = 1, /* single-word push is always end-of-packet */
        .tkeep_len = tkeep_bytes,
        .tdata_len = len,
    };

    if (!chardev_stub_send(base, &req, sizeof(req))) {
        return;
    }

    /* Send TDATA */
    if (!chardev_stub_send(base, &val, len)) {
        return;
    }

    /* Send TKEEP (all lanes valid) */
    uint8_t tkeep[AXI_STREAM_MAX_TDATA_BYTES / 8];
    memset(tkeep, 0xFF, tkeep_bytes);
    if (!chardev_stub_send(base, tkeep, tkeep_bytes)) {
        return;
    }

    /* Receive push response */
    struct axi_stream_push_resp resp = {};
    if (!chardev_stub_recv(base, &resp, sizeof(resp))) {
        return;
    }

    if (resp.ok) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "axi-stream-stub: push rejected (backpressure)\n");
    }
}

static const MemoryRegionOps axi_stream_stub_ops = {
    .read  = axi_stream_stub_read,
    .write = axi_stream_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
};

static const MemoryRegionOps *axi_stream_stub_get_region_ops(ChardevStub *s)
{
    return &axi_stream_stub_ops;
}

static const Property axi_stream_stub_properties[] = {
    DEFINE_PROP_CHR("chardev", AxiStreamStub, parent_obj.chr),
    DEFINE_PROP_UINT64("size", AxiStreamStub, parent_obj.size,
                       CHARDEV_STUB_DEFAULT_SIZE),
    DEFINE_PROP_UINT32("tdata-width", AxiStreamStub, tdata_width, 32),
};

static void axi_stream_stub_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ChardevStubClass *csc = CHARDEV_STUB_CLASS(klass);

    csc->get_region_ops = axi_stream_stub_get_region_ops;
    device_class_set_props(dc, axi_stream_stub_properties);
}

static const TypeInfo axi_stream_stub_info = {
    .name          = TYPE_AXI_STREAM_STUB,
    .parent        = TYPE_CHARDEV_STUB,
    .instance_size = sizeof(AxiStreamStub),
    .class_init    = axi_stream_stub_class_init,
};

static void axi_stream_stub_register_types(void)
{
    type_register_static(&axi_stream_stub_info);
}

type_init(axi_stream_stub_register_types)
