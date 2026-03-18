#include "main.h"

uint32_t *addr = (uint32_t *)0x08004000;
volatile uint8_t empty_sectorx = 0;

int main() {
    // --- 1. Startup Timer (3s Timeout) ---
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = 16000 - 1;
    TIM2->ARR = 3000 - 1;
    TIM2->EGR |= TIM_EGR_UG;      
    TIM2->SR &= ~TIM_SR_UIF;
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;

    uart_init();
    gpio_init();

    // --- 2. Entry Polling ---
    while(1) {
        if (TIM2->SR & TIM_SR_UIF) {
            jump_to_application(0x08004000); // Timeout: Try to jump
        }

        if ((USART2->SR & USART_SR_RXNE) || empty_sectorx) {
            if (empty_sectorx) break;
            uint8_t rx = (uint8_t)(USART2->DR);
            if (rx == 0xEE) { // User trap: Stay in bootloader
                TIM2->CR1 &= ~TIM_CR1_CEN; 
                break;
            }
        }
    }

    // --- 3. Bootloader Mode ---
    USART2->CR1 |= (1 << USART_CR1_RXNEIE_Pos);
    NVIC_EnableIRQ(USART2_IRQn);

    if (empty_sectorx) {
        USART2_SendString("SECTOR EMPTY: CANNOT JUMP\r\n");
    }

    USART2_SendString("\r\n--- STM32F4 CUSTOM BOOTLOADER ---\r\n");
    USART2_SendString("\r\n-COMMANDS : \r\n");
    USART2_SendString("F1: Erase S1 | F2: Erase S2 | AA: Send HEX | BB: Manual Jump\r\n");
    
    flash_unlock();

    while (1) {
        if (cmd_rdy) {
            cmd_rdy = 0;
            switch (CMD) {
                case 0xF1:
                    flash_erase_sector(1);
                    USART2_SendString("Sector 1 Erased\r\n");
                    break;
                case 0xF2:
                    flash_erase_sector(2);
                    USART2_SendString("Sector 2 Erased\r\n");
                    break;


                case 0xAA:
                    USART2_SendString("Awaiting Intel HEX stream...\r\n");
                    current_state = RECEIVING_DATA; 
                    break;

                case 0xBB:
                    USART2_SendString("Jumping...\r\n");
                    jump_to_application(0x08004000);
                    break;
            }
            CMD = 0;
        }

        if (transfer_complete) {
            flash_write_segment(addr, word_buffer, word_index);
            USART2_SendString("Flash Successful. Reset MCU to run.\r\n");
            transfer_complete = 0;
            current_state = IDLE;
        }
    }
}