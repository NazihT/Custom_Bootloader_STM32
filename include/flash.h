#ifndef FLASH_H
#define FLASH_H


#include "stm32f407xx.h"
#include "main.h"

extern volatile uint8_t nibble_buffer[8*4];
extern volatile char ascii_buffer [8*4] ;
extern volatile uint8_t hex_buffer[4*4];

extern volatile uint32_t final_word[4] ;  


typedef enum {
    MODE_HUMAN,
    MODE_MACHINE
} BootMode_t;

extern BootMode_t g_boot_mode ;


void flash_unlock();
void flash_lock(); 
void flash_erase_sector(uint8_t sector);
void flash_write( uint32_t* address ,  uint32_t data );
void flash_write_segment (uint32_t* address , const volatile uint32_t* buffer , uint16_t nb_words );
uint8_t AsciiToNibble(char c) ;
void FillNibbleBuffer(uint8_t total_bits);
void FillHexBuffer(uint8_t bytes_in_line);
void FillWord(uint8_t words_in_line);
void jump_to_application(uint32_t app_address);

#endif