#include "main.h"

uint32_t *addr = (uint32_t *)0x08004000;
volatile uint8_t empty_sectorx = 0;


int main() {
    // --- 1. Timer Initialization 3000ms ---
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    TIM2->PSC = 16000 - 1;
    TIM2->ARR = 3000 - 1;
    
    TIM2->EGR |= TIM_EGR_UG;      
    TIM2->SR &= ~TIM_SR_UIF;     // Clear the update flag
    TIM2->CNT = 0;
    TIM2->CR1 |= TIM_CR1_CEN;    // Start counter

    // --- 2. Peripheral Initialization ---
    uart_init();
    gpio_init();

    // --- 3. Polling Loop) ---
    while(1) {
        // Check if timeout reached
        if (TIM2->SR & TIM_SR_UIF) {
            jump_to_application(0x08004000);
        }

        // Check for UART  (0xEE)
        if ((USART2->SR & USART_SR_RXNE) || empty_sectorx  ) {
            if (empty_sectorx) break;
            uint8_t rx = (uint8_t)(USART2->DR);
            (void)USART2->SR;
            (void)USART2->DR;
            if (rx == 0xEE) {
                TIM2->CR1 &= ~TIM_CR1_CEN; // Kill the timer, we stay in S0
                break;
            }
        }
    }

    // --- 4. Bootloader Mode Configuration ---
    USART2->CR1 |= (1 << USART_CR1_RXNEIE_Pos);
    USART2->CR1 |= (1 << USART_CR1_IDLEIE_Pos);
    NVIC_EnableIRQ(USART2_IRQn);

    if (empty_sectorx)
    {
        USART2_SendString("SECTOR EMPTY .... CANT JUMP \r\n");
    }

    USART2_SendString("BOOTLOADER MODE\r\n-Transmit 0xF1 to erase sector 1 \r\n-Transmit 0xAA to send .bin\r\n-Transmit 0xBB to start application\r\n");
    flash_unlock();

    // --- 5. Main Bootloader Command Loop ---
    while (1) {
        if (cmd_rdy) {
            cmd_rdy = 0;
            switch (CMD) {
                case 0xF1: // Erase Sector 1
                    flash_erase_sector(1);
                    USART2_SendString("Sector 1 ERASED\r\n");
                    CMD=0;
                    break;

                case 0xAA: // Start Programming
                   USART2_SendString("Send File Now..\r\n");
                    current_state = RECEIVING_DATA; 
                    CMD=0;
                    break;

                case 0xBB: // Manual Jump
                    if ((*(volatile uint32_t*) 0x08004000) > 0x20020000  || (*(volatile uint32_t*) 0x08004000) < 0x20000000 ) 
                    {
                        USART2_SendString("No firmware in sector 1 .....  Staying in bootloader \r\n");
                    }
                    else {
                        USART2_SendString("Jumping to application..\r\n");
                        jump_to_application(0x08004000);
                    }
                   
                    break;
            }
        }

        if (transfer_complete) {
            flash_write_segment(addr, word_buffer, word_index);
            USART2_SendString("Flashing finished \r\n");
            USART2_SendString("Reset MCU \r\n");
            transfer_complete = 0;
           // current_state=IDLE;
        }
    }
}