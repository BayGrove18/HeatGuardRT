#include "handoff_contract.h"

#include "crc32.h"
#include "heatguard_config.h"

void handoff_manifest_build(BootManifest *manifest,
                            uint32_t transaction_id,
                            uint32_t image_size,
                            uint32_t image_crc32,
                            uint32_t image_version)
{
    manifest->magic = BOOT_MANIFEST_MAGIC;
    manifest->format = BOOT_MANIFEST_FORMAT;
    manifest->transaction_id = transaction_id;
    manifest->stage_offset = HEATGUARD_STAGE_IMAGE_OFFSET;
    manifest->image_size = image_size;
    manifest->image_crc32 = image_crc32;
    manifest->image_version = image_version;
    manifest->record_crc32 = crc32_calculate((const uint8_t *)manifest,
                                              (uint32_t)(sizeof(BootManifest) - sizeof(uint32_t)));
}

uint8_t handoff_manifest_is_valid(const BootManifest *manifest)
{
    if (manifest->magic != BOOT_MANIFEST_MAGIC ||
        manifest->format != BOOT_MANIFEST_FORMAT ||
        manifest->stage_offset != HEATGUARD_STAGE_IMAGE_OFFSET ||
        manifest->image_size == 0U ||
        manifest->image_size > HEATGUARD_STAGE_IMAGE_MAX_SIZE) {
        return 0U;
    }
    return (uint8_t)(manifest->record_crc32 ==
                     crc32_calculate((const uint8_t *)manifest,
                                     (uint32_t)(sizeof(BootManifest) - sizeof(uint32_t))));
}
