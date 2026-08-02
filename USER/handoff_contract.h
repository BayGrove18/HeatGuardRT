#ifndef HANDOFF_CONTRACT_H
#define HANDOFF_CONTRACT_H

#include <stdint.h>

#define BOOT_MANIFEST_MAGIC 0x48475254UL
#define BOOT_MANIFEST_FORMAT 1UL

typedef struct {
    uint32_t magic;
    uint32_t format;
    uint32_t transaction_id;
    uint32_t stage_offset;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t image_version;
    uint32_t record_crc32;
} BootManifest;

void handoff_manifest_build(BootManifest *manifest,
                            uint32_t transaction_id,
                            uint32_t image_size,
                            uint32_t image_crc32,
                            uint32_t image_version);
uint8_t handoff_manifest_is_valid(const BootManifest *manifest);

#endif
