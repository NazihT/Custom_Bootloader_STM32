# STM32F4 Custom IAP Bootloader

A bare-metal, protocol-aware In-Application Programming bootloader for the STM32F4, built from scratch with zero HAL dependencies. Supports dual-mode operation (human terminal / automated script), Intel HEX parsing via interrupt-driven FSM, and a full Python flashing pipeline.

---

## Architecture Overview

The bootloader lives in **Sector 0** (0x08000000). The application lives in **Sector 1** (0x08004000). On every power cycle, the bootloader runs first — always.

```
┌─────────────────────────────────────────────────────┐
│                   POWER ON / RESET                  │
└─────────────────────────┬───────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│            READ RTC BACKUP REGISTER (BKP0R)         │
│                                                     │
│  0xDEADBEEF → Machine Mode  (forced, script-driven) │
│  0xBEEFDEAD → Human Mode    (forced, terminal)      │
│  0x00000000 → Normal Boot   (countdown + polling)   │
│                                                     │
│             CLEAR BKP0R IMMEDIATELY                 │
└─────────────────────────┬───────────────────────────┘
                          │
             ┌────────────┴─────────────┐
             │                          │
         FORCED?                    NOT FORCED
             │                          │
             ▼                          ▼
    Skip countdown             TIM2 countdown (1s)
    Enter FSM directly         Poll UART for 0xEE
                               ├─ Timeout → jump_to_app()
                               └─ 0xEE received → Enter FSM
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│                  BOOTLOADER FSM                     │
│                                                     │
│  IDLE state:  waiting for command byte              │
│  0xF1 → Erase Sector 1                              │
│  0xF2 → Erase Sector 2                              │
│  0xAA → Enter RECEIVING_DATA state                  │
│  0xBB → jump_to_application(0x08004000)             │
│                                                     │
│  RECEIVING_DATA state: Intel HEX parser active      │
│  EOF record received → flash_write_segment()        │
│                       → ACK → back to IDLE          │
└─────────────────────────────────────────────────────┘
```

---

## Dual-Mode Operation

The bootloader has two feedback modes, selected by what the application writes to the RTC backup register before triggering a reset.

| Magic Word | Mode | Feedback Style |
|---|---|---|
| `0xDEADBEEF` | `MODE_MACHINE` | Raw ACK/NAK bytes (0x79 / 0x1F) |
| `0xBEEFDEAD` | `MODE_HUMAN` | Human-readable strings over UART |

This is handled by a single wrapper — no scattered if-statements in the FSM:

```c
void bl_send_response(const char *msg, uint8_t ack_byte) {
    if (g_boot_mode == MODE_MACHINE) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = ack_byte;
    } else {
        USART2_SendString(msg);
    }
}
```

Every response in the codebase goes through `bl_send_response()`. The mode logic lives in exactly one place.

---

## Intel HEX Parser

The parser runs entirely in the **USART2 IRQ handler** — no polling, no blocking. It processes the incoming HEX stream character by character as bytes arrive.

**Supported record types:**

| Type | Description | Handled |
|---|---|---|
| `00` | Data | ✅ Parsed and buffered |
| `01` | End of File | ✅ Triggers flash write |
| `04` | Extended Linear Address | ✅ Silently ignored (muted) |

**Parsing pipeline per line:**

```
':' received          → reset all line-local state
pos 1-2  (LLLL)       → byte count → expected_ascii_count
pos 3-6  (AAAA)       → 16-bit line offset
pos 7-8  (TT)         → record type gate (data / EOF / mute)
pos 9+   (data bytes) → fill ascii_buffer[]
                          → FillNibbleBuffer()
                          → FillHexBuffer()   (handles endian swap)
                          → FillWord()        (pack to uint32_t)
                          → map offset → word_buffer[] index
                          → update word_index high-water mark
EOF '\n' received     → set transfer_complete = 1
```

**Buffer mapping:**

The parser maps each line's flash offset directly to a RAM staging buffer index:

```c
uint16_t buffer_word_start = (current_line_offset - 0x4000) / 4;
```

The entire application image is staged in `word_buffer[6000]` before a single flash write operation. One call to `flash_write_segment()` writes everything.

---

## Flash Driver

All flash operations are bare-metal register-level. No HAL, no StdPeriph.

```c
// Unlock sequence (FPEC key registers)
FLASH->KEYR = 0x45670123;
FLASH->KEYR = 0xCDEF89AB;

// Sector erase
FLASH->CR |= (sector << FLASH_CR_SNB_Pos);
FLASH->CR |= FLASH_CR_SER | FLASH_CR_STRT;
while (FLASH->SR & FLASH_SR_BSY);

// 32-bit word programming
FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_PG;
*((volatile uint32_t*)address) = word;
while (FLASH->SR & FLASH_SR_BSY);
```

---

## Application Jump

```c
void jump_to_application(uint32_t app_address) {
    uint32_t stack_ptr = *(volatile uint32_t*)app_address;

    // Stack pointer sanity check (must be in SRAM range)
    if (stack_ptr > 0x20020000 || stack_ptr < 0x20000000) {
        empty_sectorx = 1;
        return;
    }

    uint32_t reset_handler = *(volatile uint32_t*)(app_address + 4);

    __disable_irq();
    SCB->VTOR = app_address;   // Relocate vector table
    __set_MSP(stack_ptr);      // Set main stack pointer

    void (*app_reset_handler)(void) = (void*)reset_handler;
    app_reset_handler();
}
```

The stack pointer range check acts as a validity gate. If Sector 1 is erased or contains garbage, the SP will be 0xFFFFFFFF — caught, rejected, bootloader stays resident.

---

## Python Flashing Pipeline

A fully automated end-to-end flashing script. Sends the magic byte to the running application, waits for reset and ACK, erases flash, streams the HEX file, and jumps to the new application. No manual intervention required.

**Full pipeline:**

```
Script sends 0x2A to APP
    → APP writes 0xDEADBEEF to RTC BKP0R
    → APP triggers NVIC_SystemReset()
    → Bootloader reads BKP0R → MODE_MACHINE
    → Bootloader sends 0x79 ACK

Script sends 0xF1 (Erase S1)  → waits 0x79
Script sends 0xF2 (Erase S2)  → waits 0x79
Script sends 0xAA (Prep HEX)  → waits 0x79
Script streams .hex file       → waits 0x79 (EOF triggers flash write)
Script sends 0xBB (Jump)       → device boots new firmware
```

**Dependencies:**

```bash
pip install pyserial rich
```

**Configuration** (top of script):

```python
FILE_PATH = r"C:\path\to\your\firmware.hex"
PORT      = "COM3"
BAUDRATE  = 115200
```

**Run:**

```bash
python flash.py
```

---

## Memory Map

```
STM32F407 Flash (1MB)
┌──────────────────┬────────────┬──────────┐
│ Sector 0         │ 0x08000000 │ 16 KB    │  ← Bootloader
│ Sector 1         │ 0x08004000 │ 16 KB    │  ← Application
│ Sector 2         │ 0x08008000 │ 16 KB    │  ← Application (overflow)
│ Sector 3         │ 0x0800C000 │ 16 KB    │
│ Sector 4         │ 0x08010000 │ 64 KB    │
│ Sectors 5-11     │ 0x08020000 │ 128 KB × │
└──────────────────┴────────────┴──────────┘

SRAM (192 KB)
┌──────────────────┬────────────┬──────────┐
│ word_buffer[6000]│ 0x20000000 │ ~23 KB   │  ← HEX staging buffer
│ Stack / Heap     │            │ remaining│
└──────────────────┴────────────┴──────────┘
```

---

## Project Structure

```
BOOTLOADER/
├── Core/
│   ├── Src/
│   │   ├── main.c          # Boot logic, RTC read, countdown, FSM
│   │   ├── flash.c         # Flash unlock/erase/write + HEX parser
│   │   ├── uart_config.c   # UART init, bl_send_response, IRQ handler
|   |   ├── gpio.c          # GPIO init
│   └── Inc/
│       ├── main.h
│       ├── flash.h
│       ├── uart_config.h
|       ├── gpio.h
├── script.py                # Automated Python flashing pipeline
└── README.md
```

---

## Hardware

| Item | Detail |
|---|---|
| MCU | STM32F407VGT6 |
| Core | ARM Cortex-M4 @ 16MHz (HSI) |
| Flash | 1MB |
| SRAM | 192KB |
| UART | USART2, 115200 8N1 |
| Debugger | ST-Link V2 / SWD |

---

## Build

Compiled with **Keil MDK** or **STM32CubeIDE**. No HAL. No StdPeriph. Direct CMSIS register access throughout.

The application project must be configured with a custom linker script setting the flash origin to `0x08004000` and the vector table offset accordingly. The application's startup code does not need modification — VTOR relocation is handled by the bootloader at jump time.