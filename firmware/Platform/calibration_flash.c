#include "calibration_flash.h"
#include "main.h"
#include <string.h>
#define CAL_BASE 0x08007000U
static uint32_t erase_attempts, erase_checked, erase_bad=0xFFFFFFFFU;
static void read_flash(void *ctx, uint32_t off, void *out, uint32_t len)
{
    (void)ctx;
    if (off+len<=2U*CAL_PAGE_BYTES) memcpy(out,(const void *)(CAL_BASE+off),len);
}
static bool erase_flash(void *ctx, unsigned page)
{
    (void)ctx;
    if (page>1) return false;
    ++erase_attempts;erase_checked=0;erase_bad=0xFFFFFFFFU;
    FLASH_EraseInitTypeDef erase={0};uint32_t error=0;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;erase.Page=14U+page;erase.NbPages=1;
    if (HAL_FLASH_Unlock()!=HAL_OK) return false;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_StatusTypeDef result=HAL_FLASHEx_Erase(&erase,&error);
    HAL_FLASH_Lock();
    if (result!=HAL_OK || error!=0xFFFFFFFFU) return false;
    /* HAL success is insufficient. Verify every word of the dedicated page. */
    const volatile uint32_t *words=(const volatile uint32_t *)(CAL_BASE+page*CAL_PAGE_BYTES);
    for (unsigned i=0;i<CAL_PAGE_BYTES/sizeof(*words);i++) {
        ++erase_checked;
        if (words[i]!=0xFFFFFFFFU) { erase_bad=page*CAL_PAGE_BYTES+i*4U;return false; }
    }
    return true;
}
static bool program_flash(void *ctx, uint32_t off, const uint8_t bytes[8])
{
    (void)ctx;uint64_t value;
    if (off>2U*CAL_PAGE_BYTES-8 || off%8U) return false;
    memcpy(&value,bytes,8);
    if (HAL_FLASH_Unlock()!=HAL_OK) return false;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_StatusTypeDef result=HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,CAL_BASE+off,value);
    HAL_FLASH_Lock();
    return result==HAL_OK;
}
CalibrationMedia bench_flash_media(void)
{
    CalibrationMedia m={NULL,read_flash,erase_flash,program_flash};return m;
}
void bench_flash_diagnostics(uint32_t *attempts, uint32_t *checked, uint32_t *bad)
{
    *attempts=erase_attempts;*checked=erase_checked;*bad=erase_bad;
}
