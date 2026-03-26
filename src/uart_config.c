#include "uart_config.h"

/* Global Buffers & States */
volatile uint32_t word_buffer[6000];
volatile uint8_t CMD = 0;
volatile Bootloader_State current_state = IDLE;
volatile uint8_t cmd_rdy = 0;
volatile uint8_t transfer_complete = 0;
volatile uint16_t word_index = 0;

/* UART Config */
UART_Config_t usart2 = {
    .BaudRate = 115200,
    .Usage = TX_RX,
    .wordlength = _8_bit
};

void uart_init() {
    UART_Init(USART2, &usart2);
}

void USART2_SendString(const char *buf) {
    UART_TransmitString(USART2, buf);
}
void bl_send_response(const char *msg, uint8_t ack_byte) {
    if (g_boot_mode == MODE_MACHINE) {
        // Wait for TX buffer to be empty, then send 1 raw byte
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = ack_byte;
    } else {
        // Send human-readable text
        USART2_SendString((char *)msg);
    }
}
/* Parser State Variables */
static uint8_t line_pos = 0;
static uint8_t ascii_idx = 0;
static uint8_t record_is_eof = 0;
static uint8_t record_is_data = 0;
static uint8_t expected_ascii_count = 0;
static char len_chars[2];
static char addr_chars[4];
static uint16_t current_line_offset = 0;

void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        uint8_t rx_byte = USART2->DR;
        
        if (current_state == IDLE) {
            CMD = rx_byte;
            cmd_rdy = 1;  
        }
        else if (current_state == RECEIVING_DATA) {
            if (rx_byte == ':') {
                line_pos = 0;
                ascii_idx = 0;
                record_is_eof = 0;
                record_is_data = 0;
                expected_ascii_count = 0;
                return; 
            }

            line_pos++;

            // 1. Capture Length
            if (line_pos == 1) len_chars[0] = rx_byte;
            if (line_pos == 2) {
                len_chars[1] = rx_byte;
                uint8_t byte_count = (AsciiToNibble(len_chars[0]) << 4) | AsciiToNibble(len_chars[1]);
                expected_ascii_count = byte_count * 2;
            }

            // 2. Capture Address to Temp
            static uint16_t temp_addr = 0;
            if (line_pos >= 3 && line_pos <= 6) {
                addr_chars[line_pos - 3] = rx_byte;
                if (line_pos == 6) {
                    uint16_t high = (AsciiToNibble(addr_chars[0]) << 4) | AsciiToNibble(addr_chars[1]);
                    uint16_t low  = (AsciiToNibble(addr_chars[2]) << 4) | AsciiToNibble(addr_chars[3]);
                    temp_addr = (high << 8) | low;
                }
            }

            // 3. Record Type Shield (Ignores 04 records)
            if (line_pos == 8) {
                if (rx_byte == '0') {
                    record_is_data = 1;
                    current_line_offset = temp_addr; 
                } 
                else if (rx_byte == '1') {
                    record_is_eof = 1;
                }
                else {
                    record_is_data = 0; // Mutes the rest of the line (e.g. Type 04)
                }
            }
            
            // 4. Collect & Parse Data
            else if (line_pos > 8 && record_is_data && expected_ascii_count > 0) {
                if (ascii_idx < expected_ascii_count && rx_byte != '\r' && rx_byte != '\n') {
                    ascii_buffer[ascii_idx++] = rx_byte;
                    
                    if (ascii_idx == expected_ascii_count) {
                        uint8_t bytes = expected_ascii_count / 2;
                        uint8_t words = bytes / 4;

                        FillNibbleBuffer(expected_ascii_count);
                        FillHexBuffer(bytes);
                        FillWord(words);
                        
                        // Maps FLASH offset to RAM buffer index
                        uint16_t buffer_word_start = (current_line_offset - 0x4000) / 4; 
                        
                        for(int w = 0; w < words; w++) {
                            word_buffer[buffer_word_start + w] = final_word[w];
                        }
                        
                        if ((buffer_word_start + words) > word_index) {
                            word_index = buffer_word_start + words;
                        }
                        expected_ascii_count = 0; 
                    }
                }
            }

            // 5. End of File check
            if (record_is_eof && (rx_byte == '\r' || rx_byte == '\n')) {
                transfer_complete = 1;
                current_state = IDLE; 
            }
        }
    }
}