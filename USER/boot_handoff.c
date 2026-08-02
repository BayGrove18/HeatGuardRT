#include "boot_handoff.h"

#include "heatguard_config.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "w25q64.h"

#define BOOT_MAILBOX_MAGIC 0xB007U

static AppResetCause reset_cause;

void boot_handoff_init(void)
{
    if (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET) {
        reset_cause = APP_RESET_WATCHDOG;
    } else if (RCC_GetFlagStatus(RCC_FLAG_SFTRST) != RESET) {
        reset_cause = APP_RESET_SOFTWARE;
    } else if (RCC_GetFlagStatus(RCC_FLAG_PINRST) != RESET) {
        reset_cause = APP_RESET_EXTERNAL;
    } else {
        reset_cause = APP_RESET_POWER_ON;
    }
    RCC_ClearFlag();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

AppResetCause boot_handoff_reset_cause(void)
{
    return reset_cause;
}

uint8_t boot_handoff_publish(const BootManifest *manifest)
{
    BootManifest verify;

    if (handoff_manifest_is_valid(manifest) == 0U ||
        w25q64_erase_range(HEATGUARD_STAGE_MANIFEST_OFFSET, 4096U) == 0U ||
        w25q64_write(HEATGUARD_STAGE_MANIFEST_OFFSET,
                     (const uint8_t *)manifest, sizeof(BootManifest)) == 0U ||
        w25q64_read(HEATGUARD_STAGE_MANIFEST_OFFSET,
                    (uint8_t *)&verify, sizeof(BootManifest)) == 0U) {
        return 0U;
    }
    return handoff_manifest_is_valid(&verify);
}

void boot_handoff_commit_and_reset(void)
{
    BKP_WriteBackupRegister(BKP_DR1, BOOT_MAILBOX_MAGIC);
    NVIC_SystemReset();
}
