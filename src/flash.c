#include "flash.h"

void flash_unlock()
{
    while (FLASH->SR & FLASH_SR_BSY) ;
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;

}
void flash_lock()
{
    while (FLASH->SR & FLASH_SR_BSY) ;
    FLASH->CR |= FLASH_CR_LOCK;

}
void flash_erase_sector(uint8_t sector)
{
    
    FLASH->CR &= ~FLASH_CR_SNB;  // Clear sector number
    FLASH->CR |= (sector<<FLASH_CR_SNB_Pos);  // set sector number
    FLASH->CR |= FLASH_CR_SER;  // sector erase  
    FLASH->CR |= FLASH_CR_STRT;  // start erase 
    while (FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_SER;
}

void flash_write( uint32_t* address ,  uint32_t data )
{
    while(FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1 ;  // x32 architecture
    FLASH->CR |=FLASH_CR_PG;  // enable programming
    *((volatile uint32_t*)address) = data ;
    while(FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &=~(FLASH_CR_PG);

}

void flash_write_segment (uint32_t* address , const volatile uint32_t* buffer , uint16_t nb_words )
{
    while(FLASH->SR & FLASH_SR_BSY);
    FLASH->CR &= ~FLASH_CR_PSIZE;
    FLASH->CR |= FLASH_CR_PSIZE_1 ;  // x32 architecture
    FLASH->CR |=FLASH_CR_PG;  // enable programming

    for (volatile int counter = 0 ; counter < nb_words ; counter ++ )
    {
        *((volatile uint32_t*)address) = *(buffer+counter);
        address++;
        while(FLASH->SR & FLASH_SR_BSY) ;
    }

    FLASH->CR &=~FLASH_CR_PG;
}

void jump_to_application(uint32_t app_address)
{
    
    uint32_t stack_ptr = *(volatile uint32_t*)app_address;
    if (stack_ptr > 0x20020000  ||  stack_ptr < 0x20000000)
    {
        empty_sectorx = 1;
        return ;
        
    }

    uint32_t reset_handler = *(volatile uint32_t*)(app_address + 4);
    
    __disable_irq();
    
  
    // Set vector table
    SCB->VTOR = app_address;
    
    // Set stack pointer
    __set_MSP(stack_ptr);
    
    // Jump to app
    void (*app_reset_handler)(void) = (void*)reset_handler;
    app_reset_handler();
    // Never returns
}