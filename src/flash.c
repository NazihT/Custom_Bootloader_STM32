#include "flash.h"

/* Parsing Buffers */
volatile uint8_t nibble_buffer[32];
volatile char ascii_buffer[32]; 
volatile uint8_t hex_buffer[16];
volatile uint32_t final_word[4] = {0, 0, 0, 0};

void flash_unlock() {
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;
}

void flash_lock() {
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR |= FLASH_CR_LOCK;
}

void flash_erase_sector(uint8_t sector) {
    FLASH->CR &= ~FLASH_CR_SNB; 
    FLASH->CR |= (sector << FLASH_CR_SNB_Pos); 
    FLASH->CR |= FLASH_CR_SER;  
    FLASH->CR |= FLASH_CR_STRT; 
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_SER;
}

void flash_write_segment(uint32_t* address, const volatile uint32_t* buffer, uint16_t nb_words) {
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1; // 32-bit programming
    FLASH->CR |= FLASH_CR_PG;

    for (int counter = 0; counter < nb_words; counter++) {
        *((volatile uint32_t*)address) = *(buffer + counter);
        address++;
        while (FLASH->SR & FLASH_SR_BSY);
    }
    FLASH->CR &= ~FLASH_CR_PG;
}

uint8_t AsciiToNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void FillNibbleBuffer(uint8_t total_bits) {
    for (int i = 0; i < total_bits; i++) {
        nibble_buffer[i] = AsciiToNibble(ascii_buffer[i]);
    }
}

/* Converts nibbles to raw bytes, handles the "Intel HEX" mapping */
void FillHexBuffer(uint8_t bytes_in_line) {
    uint8_t hex_index = bytes_in_line - 4;
    uint8_t counter = 0;
    uint8_t index_swap_number = 1;

    for (int i = (bytes_in_line * 2) - 1; i > 0; i -= 2) {
        hex_buffer[hex_index++] = (nibble_buffer[i - 1] << 4) | nibble_buffer[i];
        counter++;
        if (counter == 4) {
            counter = 0;
            index_swap_number++;
            hex_index = bytes_in_line - 4 * index_swap_number;
        }
    }
}

/* Reconstructs 32-bit words from the byte buffer */
void FillWord(uint8_t words_in_line) {
    int j = 0;
    for (int i = 0; i < words_in_line; i++) {
        final_word[i] = (hex_buffer[j] << 24)   |
                        (hex_buffer[j + 1] << 16) |
                        (hex_buffer[j + 2] << 8)  |
                        (hex_buffer[j + 3]);
        j = 4 * (i + 1);
    }
}

void jump_to_application(uint32_t app_address) {
    uint32_t stack_ptr = *(volatile uint32_t*)app_address;
    
    // Safety check for Stack Pointer range
    if (stack_ptr > 0x20020000 || stack_ptr < 0x20000000) {
        empty_sectorx = 1;
        return;
    }

    uint32_t reset_handler = *(volatile uint32_t*)(app_address + 4);
    
    __disable_irq();
    SCB->VTOR = app_address; // Relocate Vector Table
    __set_MSP(stack_ptr);    // Set Main Stack Pointer
    
    void (*app_reset_handler)(void) = (void*)reset_handler;
    app_reset_handler(); // Jump
}