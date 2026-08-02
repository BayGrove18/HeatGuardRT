#include "upgrade_transport.h"

#include "queue.h"
#include "task.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

#include "heatguard_config.h"

static QueueHandle_t rx_queue;
static uint8_t rx_ring[HEATGUARD_UART_RX_RING_SIZE];
static uint16_t ring_read_index;
static volatile uint8_t transport_overflow;

static void drain_to_index_from_isr(uint16_t ring_write_index,
                                    BaseType_t *higher_priority_task_woken)
{
    while (ring_read_index != ring_write_index) {
        if (xQueueSendFromISR(rx_queue, &rx_ring[ring_read_index],
                              higher_priority_task_woken) != pdPASS) {
            transport_overflow = 1U;
            ring_read_index = ring_write_index;
            return;
        }
        ++ring_read_index;
        if (ring_read_index == HEATGUARD_UART_RX_RING_SIZE) {
            ring_read_index = 0U;
        }
    }
}

void upgrade_transport_init(void)
{
    DMA_InitTypeDef dma;
    NVIC_InitTypeDef nvic;

    rx_queue = xQueueCreate(HEATGUARD_UART_RX_QUEUE_LENGTH, sizeof(uint8_t));
    configASSERT(rx_queue != NULL);

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    DMA_Cmd(DMA1_Channel5, DISABLE);
    DMA_DeInit(DMA1_Channel5);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)rx_ring;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize = HEATGUARD_UART_RX_RING_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Circular;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &dma);
    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT | DMA_IT_TC, ENABLE);
    nvic.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 3;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    DMA_Cmd(DMA1_Channel5, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
}

void upgrade_transport_on_idle_from_isr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    uint16_t ring_write_index;
    volatile uint32_t clear;

    clear = USART1->SR;
    clear = USART1->DR;
    (void)clear;
    ring_write_index = (uint16_t)(HEATGUARD_UART_RX_RING_SIZE -
                                  DMA_GetCurrDataCounter(DMA1_Channel5));
    if (ring_write_index == HEATGUARD_UART_RX_RING_SIZE) {
        ring_write_index = 0U;
    }

    drain_to_index_from_isr(ring_write_index, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void upgrade_transport_on_dma_from_isr(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (DMA_GetITStatus(DMA1_IT_HT5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_HT5);
        drain_to_index_from_isr(HEATGUARD_UART_RX_RING_SIZE / 2U,
                                &higher_priority_task_woken);
    }
    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET) {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        drain_to_index_from_isr(0U, &higher_priority_task_woken);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void DMA1_Channel5_IRQHandler(void)
{
    upgrade_transport_on_dma_from_isr();
}

BaseType_t upgrade_transport_receive(uint8_t *byte, TickType_t timeout)
{
    return xQueueReceive(rx_queue, byte, timeout);
}

uint8_t upgrade_transport_take_overflow(void)
{
    uint8_t overflow;

    taskENTER_CRITICAL();
    overflow = transport_overflow;
    transport_overflow = 0U;
    taskEXIT_CRITICAL();
    return overflow;
}
