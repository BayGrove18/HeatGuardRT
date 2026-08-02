#include "w25q64.h"

#include "FreeRTOS.h"
#include "task.h"

#include "heatguard_config.h"
#include "spi.h"
#include "spi_bus.h"
#include "sys.h"

#define W25Q_CMD_READ_ID 0x9FU
#define W25Q_CMD_READ_DATA 0x03U
#define W25Q_CMD_WRITE_ENABLE 0x06U
#define W25Q_CMD_PAGE_PROGRAM 0x02U
#define W25Q_CMD_SECTOR_ERASE 0x20U
#define W25Q_CMD_READ_STATUS 0x05U
#define W25Q_STATUS_BUSY 0x01U
#define W25Q_PAGE_SIZE 256UL
#define W25Q_SECTOR_SIZE 4096UL
#define W25Q_OPERATION_TIMEOUT_MS 500U

static void w25q_select(void)
{
    GPIO_ResetBits(GPIOA, HEATGUARD_W25Q_CS_PIN);
}

static void w25q_release(void)
{
    GPIO_SetBits(GPIOA, HEATGUARD_W25Q_CS_PIN);
}

void w25q64_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    gpio.GPIO_Pin = HEATGUARD_W25Q_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &gpio);
    w25q_release();
}

static uint8_t w25q_transfer(uint8_t value)
{
    return SPI_WriteByte(SPI1, value);
}

static void w25q_write_enable(void)
{
    w25q_select();
    (void)w25q_transfer(W25Q_CMD_WRITE_ENABLE);
    w25q_release();
}

static uint8_t w25q_wait_ready(void)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(W25Q_OPERATION_TIMEOUT_MS);

    do {
        uint8_t status;
        w25q_select();
        (void)w25q_transfer(W25Q_CMD_READ_STATUS);
        status = w25q_transfer(0xFFU);
        w25q_release();
        if ((status & W25Q_STATUS_BUSY) == 0U) {
            return 1U;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    } while ((int32_t)(xTaskGetTickCount() - deadline) < 0);

    return 0U;
}

static void w25q_send_address(uint32_t address)
{
    (void)w25q_transfer((uint8_t)(address >> 16));
    (void)w25q_transfer((uint8_t)(address >> 8));
    (void)w25q_transfer((uint8_t)address);
}

uint32_t w25q64_read_jedec_id(void)
{
    uint32_t id;

    if (spi_bus_lock(pdMS_TO_TICKS(20U)) != pdPASS) {
        return 0U;
    }
    w25q_select();
    (void)w25q_transfer(W25Q_CMD_READ_ID);
    id = (uint32_t)w25q_transfer(0xFFU) << 16;
    id |= (uint32_t)w25q_transfer(0xFFU) << 8;
    id |= w25q_transfer(0xFFU);
    w25q_release();
    spi_bus_unlock();
    return id;
}

uint8_t w25q64_read(uint32_t address, uint8_t *buffer, uint32_t length)
{
    uint32_t index;

    if (address + length > 0x00800000UL ||
        spi_bus_lock(pdMS_TO_TICKS(50U)) != pdPASS) {
        return 0U;
    }
    w25q_select();
    (void)w25q_transfer(W25Q_CMD_READ_DATA);
    w25q_send_address(address);
    for (index = 0U; index < length; ++index) {
        buffer[index] = w25q_transfer(0xFFU);
    }
    w25q_release();
    spi_bus_unlock();
    return 1U;
}

uint8_t w25q64_erase_range(uint32_t address, uint32_t length)
{
    uint32_t sector;
    uint32_t end;

    if (length == 0U || address + length > 0x00800000UL ||
        spi_bus_lock(portMAX_DELAY) != pdPASS) {
        return 0U;
    }
    sector = address & ~(W25Q_SECTOR_SIZE - 1UL);
    end = (address + length - 1UL) & ~(W25Q_SECTOR_SIZE - 1UL);
    do {
        w25q_write_enable();
        w25q_select();
        (void)w25q_transfer(W25Q_CMD_SECTOR_ERASE);
        w25q_send_address(sector);
        w25q_release();
        if (w25q_wait_ready() == 0U) {
            spi_bus_unlock();
            return 0U;
        }
        sector += W25Q_SECTOR_SIZE;
    } while (sector <= end);
    spi_bus_unlock();
    return 1U;
}

uint8_t w25q64_write(uint32_t address, const uint8_t *buffer, uint32_t length)
{
    uint32_t index = 0U;

    if (address + length > 0x00800000UL ||
        spi_bus_lock(portMAX_DELAY) != pdPASS) {
        return 0U;
    }
    while (index < length) {
        uint32_t page_remaining = W25Q_PAGE_SIZE - ((address + index) & (W25Q_PAGE_SIZE - 1UL));
        uint32_t chunk = length - index;
        uint32_t byte_index;

        if (chunk > page_remaining) {
            chunk = page_remaining;
        }
        w25q_write_enable();
        w25q_select();
        (void)w25q_transfer(W25Q_CMD_PAGE_PROGRAM);
        w25q_send_address(address + index);
        for (byte_index = 0U; byte_index < chunk; ++byte_index) {
            (void)w25q_transfer(buffer[index + byte_index]);
        }
        w25q_release();
        if (w25q_wait_ready() == 0U) {
            spi_bus_unlock();
            return 0U;
        }
        index += chunk;
    }
    spi_bus_unlock();
    return 1U;
}
