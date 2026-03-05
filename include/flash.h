#include "stm32f407xx.h"
#include "main.h"
void flash_unlock();
void flash_lock(); 
void flash_erase_sector(uint8_t sector);
void flash_write( uint32_t* address ,  uint32_t data );
void flash_write_segment (uint32_t* address , const volatile uint32_t* buffer , uint16_t nb_words );
void jump_to_application(uint32_t app_address);


