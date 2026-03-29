import os
import sys
import time
import serial
from rich.progress import Progress, SpinnerColumn, BarColumn, TextColumn, TransferSpeedColumn, FileSizeColumn

# --- Configuration ---
FILE_PATH = r"C:\Users\nazih\Desktop\BOOTLOADER\RTOOS.hex"
PORT = "COM3"
BAUDRATE = 115200

# --- Protocol Bytes ---
CMD_MACHINE_MODE = b'\x2A'
CMD_ERASE_S1     = b'\xF1'
CMD_ERASE_S2     = b'\xF2'
CMD_PREP_HEX     = b'\xAA'
CMD_JUMP_APP     = b'\xBB'
ACK_BYTE         = b'\x79'

CHUNK_SIZE = 256

def wait_for_ack(ser, step_name, timeout=5.0):
    """Waits for the 0x79 ACK byte from the STM32. Returns True if received."""
    ser.timeout = timeout
    print(f"[*] {step_name}... waiting for ACK")
    
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        rx = ser.read(1)
        if rx == ACK_BYTE:
            print(f"[+] SUCCESS: ACK received for {step_name}!")
            return True
        elif rx:
            print(f"[!] Warning: Received unexpected byte {rx.hex()} during {step_name}")
            
    print(f"[-] FATAL: Timeout waiting for ACK during {step_name}")
    return False

def run_flashing_pipeline():
    if not os.path.exists(FILE_PATH):
        print(f"[-] ERROR: Hex file not found at {FILE_PATH}")
        return

    file_size = os.path.getsize(FILE_PATH)
    print(f"\n🚀 Starting Automated Firmware Pipeline on {PORT}")
    print(f"📦 Target Image: {FILE_PATH} ({file_size} bytes)\n")

    try:
        # Open Serial Port
        ser = serial.Serial(PORT, BAUDRATE)

        # STEP 1: Enter Machine Mode / Reset
        ser.write(CMD_MACHINE_MODE)
        if not wait_for_ack(ser, "Entering Machine Mode / Rebooting", timeout=3.0): return

        # STEP 2: Erase Sector 1
        ser.write(CMD_ERASE_S1)
        if not wait_for_ack(ser, "Erasing Sector 1", timeout=5.0): return # Flash erase takes time!

        # STEP 3: Erase Sector 2
        ser.write(CMD_ERASE_S2)
        if not wait_for_ack(ser, "Erasing Sector 2", timeout=5.0): return

        # STEP 4: Prepare to Receive HEX
        ser.write(CMD_PREP_HEX)
        if not wait_for_ack(ser, "Preparing Flash for HEX", timeout=2.0): return

        # STEP 5: Stream the File
        print("\n[*] Commencing Data Transmission...")
        with open(FILE_PATH, 'rb') as f:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                BarColumn(),
                TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
                FileSizeColumn(),
                TransferSpeedColumn(),
            ) as progress:
                
                sending_task = progress.add_task("[cyan]Flashing RTOOS.hex...", total=file_size)

                while True:
                    chunk = f.read(CHUNK_SIZE)
                    if not chunk:
                        break
                    
                    ser.write(chunk)
                    ser.flush()
                    
                    # Tiny delay to prevent blowing up the STM32's UART RX buffer
                    # if you aren't using Circular DMA
                    time.sleep(0.005) 
                    
                    progress.update(sending_task, advance=len(chunk))

        print("\n[*] File transfer complete.")

        # STEP 6: Wait for Final Flashing ACK
        if not wait_for_ack(ser, "Final Flash Verification", timeout=10.0): return

        # STEP 7: Jump to Application
        print("\n[*] Executing Jump Command...")
        ser.write(CMD_JUMP_APP)
        print("[+] Firmware Update Complete. Device is now running Sector 1.")

        ser.close()

    except serial.SerialException as e:
        print(f"\n[-] COM Port Error: {e}")
    except Exception as e:
        print(f"\n[-] Unexpected Pipeline Crash: {e}")

if __name__ == "__main__":
    run_flashing_pipeline()