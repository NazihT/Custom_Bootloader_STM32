#include "gpio.h"


void gpio_init() 

{
    RCC->AHB1ENR |= (1 << 0);

// Configure PA2 (TX) and PA3 (RX) as Alternate Function
    GPIOA->MODER &= ~(0xF << (2 * 2)); 
    GPIOA->MODER |= (0xA << (2 * 2));  

// Set Alternate Function to AF7 (USART2)
    GPIOA->AFR[0] &= ~(0xFF << (4 * 2)); 
    GPIOA->AFR[0] |= (0x77 << (4 * 2));  
}