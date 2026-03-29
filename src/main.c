#include "main.h"

uint32_t *addr = (uint32_t *)0x08004000;  // base address sector 1
volatile uint8_t empty_sectorx = 0;
BootMode_t g_boot_mode = MODE_HUMAN;

int main(void) {
    uint8_t force_bootloader = 0;
    
    // --- 1. Unlock & Read RTC Backup Domain ---
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP; // Disable backup domain write protection

    uint32_t magic_word = RTC->BKP0R;
    
    // Determine Mode
    if (magic_word == 0xDEADBEEF) {       // 0x2A
        g_boot_mode = MODE_MACHINE;
        force_bootloader = 1;
    } else if (magic_word == 0xBEEFDEAD) {    // 0x5A
        g_boot_mode = MODE_HUMAN;
        force_bootloader = 1;
    }

    RTC->BKP0R = 0x00000000; 
  

    // --- 2. Initialize Peripherals ---
    uart_init();
    gpio_init();

    // --- 3. Entry Polling (Only run if NOT forced by Python/App reset) ---
    if (!force_bootloader) {
        // Startup Timer (1s Timeout)
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
        TIM2->PSC = 16000 - 1;
        TIM2->ARR = 1000 - 1;
        TIM2->EGR |= TIM_EGR_UG;      
        TIM2->SR &= ~TIM_SR_UIF;
        TIM2->CNT = 0;
        TIM2->CR1 |= TIM_CR1_CEN;

        while(1) {
            // Timeout: Try to jump to App
            if (TIM2->SR & TIM_SR_UIF) {
                jump_to_application(0x08004000); 
            }

            // Polling for UART '0xEE' or empty sector flag
            if ((USART2->SR & USART_SR_RXNE) || empty_sectorx) {
                if (empty_sectorx) break; // Drop into bootloader
                
                uint8_t rx = (uint8_t)(USART2->DR);
                if (rx == 0xEE) { 
                    // User trap: Stay in bootloader
                    TIM2->CR1 &= ~TIM_CR1_CEN; 
                    break;
                }
            }
        }
    }

    // --- 4. Bootloader Mode Setup ---
    USART2->CR1 |= (1 << USART_CR1_RXNEIE_Pos);
    NVIC_EnableIRQ(USART2_IRQn);

    // Initial Wakeup Feedback (0x79 = Standard ACK, 0x1F = Standard NACK)
    if (empty_sectorx) {
        bl_send_response("SECTOR EMPTY: CANNOT JUMP\r\n", 0x1F);
    } else {
        bl_send_response("\r\n--- STM32F4 CUSTOM BOOTLOADER READY ---\r\n", 0x79);
    }
    
    if (g_boot_mode == MODE_HUMAN) {
        USART2_SendString("-COMMANDS : F1: Erase S1 | F2: Erase S2 | AA: Send HEX | BB: Manual Jump\r\n");
    }
    
    flash_unlock();

    // --- 5. Main FSM Loop ---
    while (1) {
        if (cmd_rdy) {
            cmd_rdy = 0;
            switch (CMD) {
                case 0xF1:
                    flash_erase_sector(1);
                    bl_send_response("Sector 1 Erased\r\n", 0x79);
                    break;
                    
                case 0xF2:
                    flash_erase_sector(2);
                    bl_send_response("Sector 2 Erased\r\n", 0x79);
                    break;

                case 0xAA:
                    bl_send_response("Awaiting Intel HEX stream...\r\n", 0x79);
                    current_state = RECEIVING_DATA; 
                    break;

                case 0xBB:
                    bl_send_response("Jumping...\r\n", 0x79);
                    jump_to_application(0x08004000);
                    break;
                    
                default:
                    // Unrecognized command
                    bl_send_response("UNKNOWN COMMAND\r\n", 0x1F);
                    break;
            }
            CMD = 0;
        }

        if (transfer_complete && !checksum_err) {
            flash_write_segment(addr, word_buffer, word_index);
            bl_send_response("Flash Successful. Reset MCU to run.\r\n", 0x79);
            transfer_complete = 0;
            current_state = IDLE;
        }
    }
}
