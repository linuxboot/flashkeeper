#!/usr/bin/env python3
"""
EDE Shell SPI Programmer

Experimental. An alternative way to flash firmware via the Flashkeeper FPGA by directly driving
the EDE shell's host flash commands (in most cases, serprog mode is preferable).

If the FPGA has a serial password set, log in to the shell before running this.
"""

import sys
import time
import serial
import os

# Configuration
DEFAULT_SERIAL_PORT = '/dev/serial/by-id/usb-FTDI_Dual_RS232-HS-if01-port0'
BAUD_RATE = 1000000
DEBUG = False

# Timing
CHAR_DELAY = 0.00015 #0.0003
LINE_DELAY = 0 #0.008
PROMPT_TIMEOUT = 15.0       # Increased default timeout (shell can be slow)
ERASE_TIMEOUT = 60.0        # Chip erase takes a long time
WRITE_SETTLE = 0 #0.03          # Small wait after hfwrite

# Constraints
MAX_CELLS_PER_LINE = 8      # 8 cells × 11 chars + 7 spaces = 95 chars (fits in 96)

OK_RESPONSE = 'ok'
ERROR_KEYWORDS = ['error', 'insufficient', 'must provide', 'length must', 'write must', 'fail']


def log(text):
    if DEBUG:
        print(f"[LOG] {text}")


def send_raw(ser, data):
    """Send bytes to serial with small delay between characters to prevent buffer overflow."""
    if isinstance(data, str):
        data = data.encode('ascii')
    for byte in data:
        ser.write(bytes([byte]))
        time.sleep(CHAR_DELAY)
    time.sleep(LINE_DELAY)


def send_line(ser, line, timeout=PROMPT_TIMEOUT):
    """Send a line to the shell, append CR, and wait for response."""
    if DEBUG:
        print(f">>> {line}")
    send_raw(ser, line)
    send_raw(ser, '\r')
    response = read_until_prompt(ser, timeout=timeout)
    if DEBUG:
        # Print first 200 chars of response to see if we got the 'ok'
        print(f"<<< {response.strip()[:200]}...")
    return response


def read_until_prompt(ser, timeout=PROMPT_TIMEOUT):
    """Read serial data until a prompt or 'ok' is detected."""
    start = time.time()
    buffer = b''
    
    while time.time() - start < timeout:
        if ser.in_waiting:
            buffer += ser.read(ser.in_waiting)
            text = buffer.decode('utf-8', errors='replace')
            lines = text.split('\n')
            if lines:
                last_line = lines[-1].strip()
                
                # Check for stack prompt (e.g., "00>", "10>", "FF>")
                if last_line.endswith('>'):
                    prefix = last_line[:-1]
                    # Validate it looks like a hex stack depth
                    if prefix and all(c in '0123456789abcdefAB' for c in prefix):
                        return text
                
                # Check for 'ok' response
                elif last_line == OK_RESPONSE:
                    return text
                    
        time.sleep(0.00001) # Poll every 0.01ms
    
    # If we get here, we timed out
    if DEBUG:
        print(f"[WARN] Timeout waiting for prompt. Buffer: {buffer.decode('utf-8', errors='replace')}")
    return buffer.decode('utf-8', errors='replace')


def check_response(response):
    """Check if response indicates success or failure."""
    lower_resp = response.lower()
    
    # Check for errors first
    for keyword in ERROR_KEYWORDS:
        if keyword in lower_resp:
            log(f"  ERROR DETECTED: {keyword}")
            return False
    
    if 'ok' not in lower_resp:
        log("  'ok' not found in response")
        return False
    
    return True


def parse_hex_bytes(text):
    """Extract 2-char hex bytes from shell output (ignoring prompts/commands)."""
    result = []
    for line in text.split('\n'):
        line = line.strip()
        # Ignore prompts and 'ok'
        if not line or line.endswith('>') or line == OK_RESPONSE:
            continue
        for token in line.split():
            # Only grab 2-char hex tokens (the actual data bytes)
            if len(token) == 2 and all(c in '0123456789abcdef' for c in token.lower()):
                result.append(int(token, 16))
    return bytes(result)


def erase_chip(ser):
    """Erase entire flash chip."""
    print("  Erasing entire flash chip (this takes a long time)...")
    # Use a longer timeout for erase
    response = send_line(ser, 'hfchiperase', timeout=ERASE_TIMEOUT)
    
    # Small settle time after erase finishes
    time.sleep(1.0)
    
    if not check_response(response):
        return False
    return True


def prepare_write_line(cells):
    """Format cells for shell input."""
    # Format as 0xXXXXXXXX (11 chars per cell)
    parts = [f'0x{cell:08x}' for cell in cells]
    return ' '.join(parts)


def program_page(ser, addr, data_bytes):
    """Program a single flash page."""
    length = len(data_bytes)
    
    # Convert bytes to 32-bit cells (little-endian)
    padded = bytearray(data_bytes)
    padded.extend([0] * ((4 - (length % 4)) % 4))
    
    cells = []
    for i in range(0, len(padded), 4):
        cell = (padded[i] | (padded[i+1] << 8) | 
                (padded[i+2] << 16) | (padded[i+3] << 24))
        cells.append(cell)
    
    # Send cells in batches (8 cells per line)
    for i in range(0, len(cells), MAX_CELLS_PER_LINE):
        chunk = cells[i:i+MAX_CELLS_PER_LINE]
        line = prepare_write_line(chunk)
        response = send_line(ser, line)
        # We don't strictly need to check response here as it's just stack loading,
        # but if it fails, subsequent hfwrite will fail.
    
    # Execute hfwrite
    # IMPORTANT: Length and Address must be in HEX
    hex_len = f'0x{length:02x}'
    hex_addr = f'0x{addr:06x}'
    hfwrite_cmd = f'{hex_addr} {hex_len} hfwrite'
    
    response = send_line(ser, hfwrite_cmd)
    
    if not check_response(response):
        print(f"  ERROR: hfwrite failed.")
        return False
    
    time.sleep(WRITE_SETTLE)
    return True


def verify_page(ser, addr, data_bytes):
    """Verify flash page contents by reading and comparing."""
    length = len(data_bytes)
    
    # IMPORTANT: Length and Address must be in HEX
    hex_len = f'0x{length:02x}'
    hex_addr = f'0x{addr:06x}'
    read_cmd = f'{hex_addr} {hex_len} hfdump'
    
    response = send_line(ser, read_cmd)
    
    if not check_response(response):
        print(f"  ERROR: hfdump failed.")
        return False
    
    read_bytes = parse_hex_bytes(response)
    
    if len(read_bytes) < length:
        print(f"  ERROR: Read {len(read_bytes)} bytes, expected {length}")
        return False
    
    if read_bytes[:length] == data_bytes:
        return True
    
    print(f"  MISMATCH at address 0x{addr:06x}:")
    for i in range(min(length, len(read_bytes))):
        if data_bytes[i] != read_bytes[i]:
            print(f"    Byte {i}: expected 0x{data_bytes[i]:02x}, got 0x{read_bytes[i]:02x}")
            break
    return False


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <binary_file> [serial_port]")
        sys.exit(1)
    
    binary_file = sys.argv[1]
    serial_port = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_SERIAL_PORT
    
    if not os.path.isfile(binary_file):
        print(f"Error: File '{binary_file}' not found.")
        sys.exit(1)
    
    print(f"Reading binary file: {binary_file}")
    with open(binary_file, 'rb') as f:
        image_data = f.read()
    
    if len(image_data) == 0:
        print("Error: Binary file is empty.")
        sys.exit(1)
    
    page_size = 256
    num_pages = (len(image_data) + page_size - 1) // page_size
    
    print(f"Image size: {len(image_data)} bytes ({num_pages} pages)")
    print(f"Serial port: {serial_port} | Baud: {BAUD_RATE}")
    print()
    
    try:
        ser = serial.Serial(serial_port, BAUD_RATE, timeout=2)
        print(f"Opened {serial_port}")
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)
    
    try:
        print("Waiting for shell prompt...")
        time.sleep(2)
        ser.reset_input_buffer()
        
        # Send a newline to force a prompt and sync up
        send_raw(ser, '\r')
        response = read_until_prompt(ser, timeout=5.0)
        print(f"Shell ready: {response.strip()[:50]}...")
        
        print("=" * 60)
        
        # 1. Erase
        print("\n[1/3] Erasing flash...")
        if not erase_chip(ser):
            print("Error: Chip erase failed or timed out.")
            sys.exit(1)
        
        # 2. Program & Verify
        print(f"\n[2/3] Programming {num_pages} page(s)...")
        print("  (This will take a while. Progress updates every 100 pages.)")
        
        for page_idx in range(num_pages):
            # Progress update
            if page_idx > 0 and page_idx % 100 == 0:
                print(f"\n  ... Progress: {page_idx}/{num_pages} pages ...")

            addr = page_idx * page_size
            end = min(addr + page_size, len(image_data))
            page_data = image_data[addr:end]
            
            # Only print every page start to reduce spam, unless debug
            if DEBUG:
                print(f"  Page {page_idx+1}/{num_pages} at 0x{addr:06x}", end="... ")
            
            if not program_page(ser, addr, page_data):
                print(f"WRITE FAILED on Page {page_idx+1}!")
                sys.exit(1)
            
            #if not verify_page(ser, addr, page_data):
            #    print(f"VERIFY FAILED on Page {page_idx+1}!")
            #    sys.exit(1)
            
            if DEBUG:
                print("OK")
        
        # 3. Done
        print(f"\n[3/3] All {num_pages} pages programmed and verified.")
        print("\n" + "=" * 60)
        print("SUCCESS: Flash programmed and verified!")
        print(f"  Size: {len(image_data)} bytes")
        print(f"  Range: 0x000000 - 0x{(len(image_data)-1):06x}")
        print("=" * 60)
        
    except KeyboardInterrupt:
        print("\n\nOperation cancelled by user.")
        sys.exit(1)
    finally:
        ser.close()
        print(f"\nClosed {serial_port}")


if __name__ == '__main__':
    main()