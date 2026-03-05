#ifndef UART_HAL_H
#define UART_HAL_H
#include "stm32f407xx.h"

typedef enum
{
    RXNE_Interrupt_Disable,
    RXNE_Interrupt_Enable,
    
}RXNE_Interrupt;

typedef enum
{
    _8_bit,
    _9_bit,
}Word_Length;

typedef enum
{
    TX,
    RX,
    TX_RX

}UART_Usage;


typedef struct {

       
   uint32_t BaudRate;
   UART_Usage Usage;
   RXNE_Interrupt interrupt_mode;
   Word_Length wordlength;
    
}UART_Config_t;

void UART_TransmitString(USART_TypeDef *uartx, const char* string);


void UART_SetUsage(USART_TypeDef *uartx,UART_Usage usage);
void UART_EnableClock(USART_TypeDef *uartx);
void UART_SetInterrupts(USART_TypeDef *uartx,UART_Config_t *confg);
void UART_Init(USART_TypeDef *uartx , UART_Config_t *confg);
IRQn_Type UART_GetIRQn(USART_TypeDef *uartx);
void UART_SetWordLength(USART_TypeDef *uartx,Word_Length wordlength);

void UART_RxClearFlag(USART_TypeDef *uart);
void UART_Transmit(USART_TypeDef *uart  , uint8_t data);

#endif
