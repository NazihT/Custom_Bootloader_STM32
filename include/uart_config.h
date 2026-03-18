#ifndef UART_CONFIG_H
#define UART_CONFIG_H
#include "UART_HAL.h"
#include "flash.h"


void USART2_SendString(const char* buf);
void uart_init();





extern volatile uint32_t word_buffer[6000];
extern volatile uint8_t byte_buffer[4]; 
extern volatile uint8_t CMD;


extern volatile uint8_t transfer_complete ;
extern volatile uint16_t word_index ;
extern volatile uint8_t byte_index ;

typedef enum {
    IDLE,
    RECEIVING_DATA,
    
}Bootloader_State ;

extern volatile Bootloader_State current_state;
extern volatile uint8_t cmd_rdy;

#endif