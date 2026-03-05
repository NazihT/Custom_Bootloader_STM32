#include "UART_HAL.h"
#define PCLK1_FREQ  16000000
void UART_Init(USART_TypeDef *uartx , UART_Config_t *confg)
{
    UART_EnableClock(uartx);
    uartx->BRR=(PCLK1_FREQ/confg->BaudRate); // GADIT JUSTE LI  F APB1
    UART_SetWordLength(uartx,confg->wordlength);
    UART_SetUsage(uartx,confg->Usage);
    UART_SetInterrupts(uartx,confg);
   USART2->CR1 |=(1<<USART_CR1_UE_Pos);

     


}



void UART_SetUsage(USART_TypeDef *uartx,UART_Usage usage)
{
   if (usage==TX)
   {
      uartx->CR1 |=(1<<USART_CR1_TE_Pos);
      uartx->CR1 &=~(1<<USART_CR1_RE_Pos);

   }
   else if (usage==RX)
   {
      uartx->CR1 |=(1<<USART_CR1_RE_Pos);
      uartx->CR1 &=~(1<<USART_CR1_TE_Pos);


   }
   else if (usage==TX_RX)
   {
       uartx->CR1 |=(1<<USART_CR1_RE_Pos);
      uartx->CR1  |=(1<<USART_CR1_TE_Pos);
   }
}



void UART_RxClearFlag(USART_TypeDef *uart)
{
    (void)uart->DR;
}


void UART_Transmit(USART_TypeDef *uartx  , uint8_t data)
{
      while(!(uartx->SR & (1 << USART_SR_TXE_Pos))) {};
      uartx->DR = data;
}

void UART_TransmitString(USART_TypeDef *uartx, const char* string)
{
   while(*string)
   {
      UART_Transmit(uartx,*string);
      string++;
   }
   
}



void UART_SetInterrupts(USART_TypeDef *uartx,UART_Config_t *confg)
{
    if (confg->interrupt_mode==RXNE_Interrupt_Enable)
    {
      uartx->CR1 |=(1<<USART_CR1_RXNEIE_Pos);
      uartx->CR1 |=(1<<USART_CR1_IDLEIE_Pos);

        IRQn_Type IRQn=UART_GetIRQn(uartx);
        NVIC_EnableIRQ(IRQn);
    }
}

void UART_SetWordLength(USART_TypeDef *uartx,Word_Length wordlength)
{
   if (wordlength==_8_bit)
   {
      uartx->CR1 &=~(USART_CR1_M_Pos);

   }
   else if (wordlength==_9_bit)
   {
      uartx->CR1 |=(USART_CR1_M_Pos);
      
   }
}


IRQn_Type UART_GetIRQn(USART_TypeDef *uartx)
{
    if(uartx==USART1)
     {
        return (USART1_IRQn);
     }
     else if(uartx==USART2)
     {
        return (USART2_IRQn);
     }
     else if(uartx==USART3)
     {
        return (USART3_IRQn);
     }

     else if(uartx==UART4)
     {
       return (UART4_IRQn);
     }
    else if(uartx==UART5)
     { 
        return (UART5_IRQn);

     }
     else if(uartx==USART6)
     { 
        return (USART6_IRQn);

     }
     else return 0;


   }



void UART_EnableClock(USART_TypeDef *uartx)
{
     if(uartx==USART1)
     {
        RCC->APB2ENR |=(1<<RCC_APB2ENR_USART1EN_Pos);
     }
     else if(uartx==USART2)
     {
       RCC->APB1ENR |=(1<<RCC_APB1ENR_USART2EN_Pos);
     }
     else if(uartx==USART3)
     {
       RCC->APB1ENR |=(1<<RCC_APB1ENR_USART3EN_Pos);
     }

     else if(uartx==UART4)
     {
      RCC->APB1ENR |=(1<<RCC_APB1ENR_UART4EN_Pos);
     }
    else if(uartx==UART5)
     { 
        RCC->APB1ENR |=(1<<RCC_APB1ENR_UART5EN_Pos);

     }
     else if(uartx==USART6)
     { 
        RCC->APB2ENR |=(1<<RCC_APB2ENR_USART6EN_Pos);

     }

}