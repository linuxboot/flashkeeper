/**
 * Serprog for Flashkeeper
 *
 * Based on "pico-serprog" by Mate Kukri <km@mkukri.xyz> and Thomas Roth <code@stacksmashing.net>
 * 
 * Licensed under GPLv3
 *
 * Also based on stm32-vserprog:
 *  https://github.com/dword1511/stm32-vserprog
 * 
 */

#include "picosoc.h"
#include "serprog.h"

static inline uint8_t readbyte_blocking(void){
    return getchar();
}

// If a byte gets dropped (either on the wire, or by the UART due to a
// framing error), fewer than the desired number of bytes will arrive
// and this will time out. In that case, NAK. flashrom will abort.
#define SPIOP_BYTE_TIMEOUT_CYCLES (100u * 12000u)

static inline uint8_t uart_rx_ferr(void){
    return (uint8_t) ((reg_uart_sts >> 8) & 0xFF);
}

// Framing-error count at the start of the current command: any increase
// by the time the command's payload has been received means the RX dropped
// a byte inside this command. 
static uint8_t serprog_ferr_base;

static inline int readbyte_blocking_tmo(void){
    return getchar_tmo(SPIOP_BYTE_TIMEOUT_CYCLES);
}

// Blocking read of len bytes; returns 0 on success, -1 on byte timeout
static int readbytes_blocking_tmo(uint8_t *b, uint32_t len){
    while (len) {
        int c = readbyte_blocking_tmo();
        if (c < 0)
            return -1;
        *b = (uint8_t) c;
        b += 1;
        len -= 1;
    }
    return 0;
}

static inline void sendbytes_blocking(const uint8_t *b, uint32_t len){
    while (len) {
        putchar_raw(*b);
        b += 1;
        len -= 1;
    }
}

static inline void sendbyte_blocking(uint8_t b){
    putchar_raw(b);
}

void serprog_command_loop(){
    for (;;) {
        serprog_ferr_base = uart_rx_ferr();
        switch (readbyte_blocking()) {
        case S_CMD_NOP:
            sendbyte_blocking(S_ACK);
            break;
        case S_CMD_Q_IFACE:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0x01);
            sendbyte_blocking(0x00);
            break;
        case S_CMD_Q_CMDMAP:
            {
                static const uint32_t cmdmap[8] = {
                    (1 << S_CMD_NOP)       |
                      (1 << S_CMD_Q_IFACE)   |
                      (1 << S_CMD_Q_CMDMAP)  |
                      (1 << S_CMD_Q_PGMNAME) |
                      (1 << S_CMD_Q_SERBUF)  |
                      (1 << S_CMD_Q_BUSTYPE) |
                      (1 << S_CMD_SYNCNOP)   |
                      (1 << S_CMD_O_SPIOP)   |
                      (1 << S_CMD_S_BUSTYPE) |
                      (1 << S_CMD_Q_WRNMAXLEN) |
                      (1 << S_CMD_Q_RDNMAXLEN)
                };

                sendbyte_blocking(S_ACK);
                sendbytes_blocking((const uint8_t *) cmdmap, sizeof cmdmap);
                break;
            }
        case S_CMD_Q_PGMNAME:
            {
                static const char progname[16] = "FlashkeeperFPGA";

                sendbyte_blocking(S_ACK);
                sendbytes_blocking((const uint8_t *) progname, sizeof progname);
                break;
            }
        case S_CMD_Q_SERBUF:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0xFF);
            sendbyte_blocking(0xFF);
            break;
        case S_CMD_Q_BUSTYPE:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking((1 << 3)); // BUS_SPI
            break;
        case S_CMD_SYNCNOP:
            sendbyte_blocking(S_NAK);
            sendbyte_blocking(S_ACK);
            break;
        case S_CMD_S_BUSTYPE:
            // If SPI is among the requested bus types we succeed, fail otherwise
            if((uint8_t) readbyte_blocking() & (1 << 3))
                sendbyte_blocking(S_ACK);
            else
                sendbyte_blocking(S_NAK);
            break;
        case S_CMD_Q_WRNMAXLEN:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0x00); // 256 byte max write length: one full page.
            sendbyte_blocking(0x01);
            sendbyte_blocking(0x00);
        break;
        case S_CMD_Q_RDNMAXLEN:
            sendbyte_blocking(S_ACK);
            sendbyte_blocking(0x00);
            sendbyte_blocking(0x10); // specify 4096 byte max read length - this works reliably
            sendbyte_blocking(0x00);
        break;
        case S_CMD_O_SPIOP:
            {
                static uint8_t buf[8200]; 
                uint8_t hdr[6];

                // In case of UART timeout, send a NAK so Flashrom aborts rather than hanging
                if (readbytes_blocking_tmo(hdr, 6) != 0) {
                    sendbyte_blocking(S_NAK);
                    break;
                }
                uint32_t wlen = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16);
                uint32_t rlen = hdr[3] | (hdr[4] << 8) | (hdr[5] << 16);

                // Validate buffer size FIRST
                if (wlen + rlen > sizeof(buf)) {
                    sendbyte_blocking(S_NAK);
                    break;
                }

                // Read the Write data from the host
                if (wlen > 0 && readbytes_blocking_tmo(buf, wlen) != 0) {
                    sendbyte_blocking(S_NAK);
                    break;
                }
                if (uart_rx_ferr() != serprog_ferr_base) {
                    sendbyte_blocking(S_NAK);
                    break;
                }

                // Send ACK ONLY if we accept the transaction
                sendbyte_blocking(S_ACK);

                // Execute the SPI Transaction
                flashio_call(buf, (int)(wlen + rlen), SPI_CMD_WE, HSPI_CTL_ADDR, 0);

                // Send Response Data back to host
                if (rlen > 0) {
                    sendbytes_blocking(&buf[wlen], rlen);
                }
                
                break;
            }
            break;
        default:
            sendbyte_blocking(S_NAK);
            break;
        }
    }
}