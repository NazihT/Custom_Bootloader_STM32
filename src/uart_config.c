#include "uart_config.h"

volatile uint32_t word_buffer[6000];
volatile uint8_t byte_buffer[4];
volatile uint8_t CMD = 0;
volatile Bootloader_State current_state = IDLE;
volatile uint8_t cmd_rdy = 0;

volatile uint8_t transfer_complete = 0;
volatile uint8_t byte_index = 0;
volatile uint16_t word_index = 0;

UART_Config_t usart2 =
{
    .BaudRate = 28800,
    .Usage = TX_RX,
    .wordlength = _8_bit
};

void USART2_SendString(const char *buf)
{
    UART_TransmitString(USART2, buf);
}

void uart_init()
{
    
    UART_Init(USART2, &usart2);
}

void USART2_IRQHandler()
{
    if (USART2->SR & USART_SR_RXNE)
    {
        uint8_t rx_byte = USART2->DR;
        
        if (current_state == IDLE)
        {
            CMD = rx_byte;
            cmd_rdy = 1; // Flag for main
        }
        else if (current_state == RECEIVING_DATA)
        {
            *(byte_buffer + byte_index) = rx_byte;
            byte_index++;
            
            if (byte_index == 4)
            {
                word_buffer[word_index] =
                    (byte_buffer[3] << 24) |
                    (byte_buffer[2] << 16) |
                    (byte_buffer[1] << 8) |
                    (byte_buffer[0]);
                // word_rdy=1;
                word_index++;
                byte_index = 0;
            }
        }
    }

    if (USART2->SR & USART_SR_IDLE)
    {
        (void)USART2->SR;
        (void)USART2->DR;
        
        if (current_state == RECEIVING_DATA)
        {
            transfer_complete = 1;
            //current_state==IDLE;
        }
    }
}