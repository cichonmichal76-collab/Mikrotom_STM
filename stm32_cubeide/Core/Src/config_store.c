#include "config_store.h"
#include "limits.h"
#include "safety_config.h"
#include "commissioning.h"
#include "calibration.h"
#include <string.h>

#if defined(__has_include)
#if __has_include("main.h")
#include "main.h"
#elif __has_include("stm32g4xx_hal.h")
#include "stm32g4xx_hal.h"
#endif
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 0x800U
#endif

#ifndef FLASHSIZE_BASE
#define FLASHSIZE_BASE 0x1FFF75E0UL
#endif

#ifndef FLASH_BASE
#define FLASH_BASE 0x08000000UL
#endif

#ifndef CONFIG_STORE_MAGIC
#define CONFIG_STORE_MAGIC 0x4D4B5453UL
#endif

#define CONFIG_STORE_VERSION 1u
#define CONFIG_STORE_PROGRAM_GRANULARITY 8u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t crc;
    Limits_t limits;
    SafetyConfig_t safety;
    Commissioning_t commissioning;
    Calibration_t calibration;
} ConfigStoreImage_t;

static uint8_t g_config_loaded = 0u;

static uint32_t config_store_flash_size_bytes(void)
{
    return ((uint32_t)(*((const uint16_t*)FLASHSIZE_BASE))) * 1024u;
}

static uint32_t config_store_page_address(void)
{
    return FLASH_BASE + config_store_flash_size_bytes() - FLASH_PAGE_SIZE;
}

static const ConfigStoreImage_t* config_store_flash_image(void)
{
    return (const ConfigStoreImage_t*)config_store_page_address();
}

static uint32_t config_store_crc32(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    uint32_t bit;

    for (i = 0u; i < length; i++)
    {
        crc ^= (uint32_t)bytes[i];
        for (bit = 0u; bit < 8u; bit++)
            crc = (crc & 1u) ? ((crc >> 1u) ^ 0xEDB88320u) : (crc >> 1u);
    }

    return ~crc;
}

static void config_store_capture(ConfigStoreImage_t *image)
{
    if (image == 0) return;

    memset(image, 0, sizeof(*image));
    image->magic = CONFIG_STORE_MAGIC;
    image->version = CONFIG_STORE_VERSION;
    image->payload_size = (uint32_t)(sizeof(*image) - (4u * sizeof(uint32_t)));
    image->limits = *Limits_Get();
    image->safety = *SafetyConfig_Get();
    image->commissioning = *Commissioning_Get();
    image->calibration = (Calibration_t){
        Calibration_GetZeroPosUm(),
        Calibration_GetPitchUm(),
        Calibration_GetSign(),
        Calibration_IsValid()
    };
    image->crc = config_store_crc32(&image->limits, image->payload_size);
}

static uint8_t config_store_limits_valid(const Limits_t *limits)
{
    return Limits_IsValid(limits);
}

static uint8_t config_store_calibration_valid(const Calibration_t *calibration)
{
    if (calibration == 0) return 0u;
    if (calibration->electrical_pitch_um <= 0.0f) return 0u;
    if ((calibration->sign != -1) && (calibration->sign != 1)) return 0u;
    if ((calibration->valid != 0u) && (calibration->valid != 1u)) return 0u;
    return 1u;
}

static uint8_t config_store_commissioning_valid(const Commissioning_t *commissioning)
{
    if (commissioning == 0) return 0u;
    if (commissioning->stage < 1u || commissioning->stage > 3u) return 0u;
    return 1u;
}

static uint8_t config_store_image_valid(const ConfigStoreImage_t *image)
{
    uint32_t crc;

    if (image == 0) return 0u;
    if (image->magic != CONFIG_STORE_MAGIC) return 0u;
    if (image->version != CONFIG_STORE_VERSION) return 0u;
    if (image->payload_size != (uint32_t)(sizeof(*image) - (4u * sizeof(uint32_t)))) return 0u;

    crc = config_store_crc32(&image->limits, image->payload_size);
    if (crc != image->crc) return 0u;
    if (!config_store_limits_valid(&image->limits)) return 0u;
    if (!config_store_commissioning_valid(&image->commissioning)) return 0u;
    if (!config_store_calibration_valid(&image->calibration)) return 0u;
    return 1u;
}

static void config_store_apply(const ConfigStoreImage_t *image)
{
    if (image == 0) return;
    Limits_Apply(&image->limits);
    SafetyConfig_Apply(&image->safety);
    Commissioning_Apply(&image->commissioning);
    Calibration_Apply(&image->calibration);
}

#if defined(HAL_FLASH_MODULE_ENABLED) || defined(HAL_FLASH_MODULE_ONLY)
static uint32_t config_store_bank_for_address(uint32_t address)
{
#ifdef FLASH_BANK_2
    uint32_t bank_size = config_store_flash_size_bytes() / 2u;
    return (address < (FLASH_BASE + bank_size)) ? FLASH_BANK_1 : FLASH_BANK_2;
#else
    (void)address;
    return FLASH_BANK_1;
#endif
}

static HAL_StatusTypeDef config_store_erase_page(uint32_t address)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0u;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = config_store_bank_for_address(address);
    erase.Page = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages = 1u;
    return HAL_FLASHEx_Erase(&erase, &page_error);
}
#endif

void ConfigStore_Init(void)
{
    g_config_loaded = 0u;
}

uint8_t ConfigStore_Load(void)
{
    const ConfigStoreImage_t *image = config_store_flash_image();

    if (!config_store_image_valid(image))
    {
        ConfigStore_LoadDefaults();
        g_config_loaded = 0u;
        return 0u;
    }

    config_store_apply(image);
    g_config_loaded = 1u;
    return 1u;
}

uint8_t ConfigStore_Save(void)
{
#if !defined(HAL_FLASH_MODULE_ENABLED) && !defined(HAL_FLASH_MODULE_ONLY)
    return 0u;
#else
    ConfigStoreImage_t image;
    uint8_t staged[((sizeof(ConfigStoreImage_t) + (CONFIG_STORE_PROGRAM_GRANULARITY - 1u)) / CONFIG_STORE_PROGRAM_GRANULARITY) * CONFIG_STORE_PROGRAM_GRANULARITY];
    const uint64_t *words;
    uint32_t address = config_store_page_address();
    uint32_t i;
    uint32_t word_count;

    config_store_capture(&image);
    memset(staged, 0, sizeof(staged));
    memcpy(staged, &image, sizeof(image));
    words = (const uint64_t*)staged;
    word_count = (uint32_t)((sizeof(image) + (CONFIG_STORE_PROGRAM_GRANULARITY - 1u)) / CONFIG_STORE_PROGRAM_GRANULARITY);

    HAL_FLASH_Unlock();
    if (config_store_erase_page(address) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 0u;
    }

    for (i = 0u; i < word_count; i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address + (i * CONFIG_STORE_PROGRAM_GRANULARITY), words[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 0u;
        }
    }

    HAL_FLASH_Lock();
    g_config_loaded = 1u;
    return 1u;
#endif
}

void ConfigStore_LoadDefaults(void)
{
    Limits_LoadDefaults();
    SafetyConfig_LoadDefaults();
    Commissioning_Init();
    Calibration_LoadDefaults();
}

uint8_t ConfigStore_IsLoaded(void)
{
    return g_config_loaded;
}
