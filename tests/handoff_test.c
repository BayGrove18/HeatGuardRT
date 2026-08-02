#include <assert.h>
#include <stdio.h>

#include "crc32.h"
#include "handoff_contract.h"

static void test_crc32_reference_vector(void)
{
    static const uint8_t text[] = "123456789";

    assert(crc32_calculate(text, 9U) == 0xCBF43926UL);
}

static void test_manifest_round_trip(void)
{
    BootManifest manifest;

    handoff_manifest_build(&manifest, 42U, 32768U, 0x12345678UL, 7U);
    assert(handoff_manifest_is_valid(&manifest) != 0U);
    assert(manifest.transaction_id == 42U);
    assert(manifest.image_size == 32768U);
    assert(manifest.image_version == 7U);
}

static void test_manifest_corruption_is_rejected(void)
{
    BootManifest manifest;

    handoff_manifest_build(&manifest, 1U, 4096U, 0xAABBCCDDUL, 2U);
    manifest.image_crc32 ^= 1U;
    assert(handoff_manifest_is_valid(&manifest) == 0U);
}

int main(void)
{
    test_crc32_reference_vector();
    test_manifest_round_trip();
    test_manifest_corruption_is_rejected();
    puts("handoff tests passed");
    return 0;
}
