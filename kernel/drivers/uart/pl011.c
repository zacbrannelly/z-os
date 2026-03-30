#include "pl011.h"

// Register offsets
static const uint32_t DR_OFFSET = 0x000;    // Data Register
static const uint32_t FR_OFFSET = 0x018;    // Flag Register
static const uint32_t IBRD_OFFSET = 0x024;  // Integer Baud Rate Divisor
static const uint32_t FBRD_OFFSET = 0x028;  // Fractional Baud Rate Divisor
static const uint32_t LCR_OFFSET = 0x02c;   // Line Control Register
static const uint32_t CR_OFFSET = 0x030;    // Control Register
static const uint32_t IMSC_OFFSET = 0x038;  // Interrupt Mask and Status Register
static const uint32_t DMACR_OFFSET = 0x048; // DMA Control Register

// Flag Register bits
static const uint32_t FR_BUSY = (1 << 3); // UART is busy
static const uint32_t FR_RXFE = (1 << 4); // Recieve FIFO is empty
static const uint32_t FR_TXFE = (1 << 7); // Transmit FIFO is empty

// Line Control Register bits
static const uint32_t LCR_FIFO_ENABLE = (1 << 4); // FIFO enable (1 = enable, 0 = disable)
static const uint32_t LCR_DATA_BITS_8 = (3 << 5); // 8 data bits
static const uint32_t LCR_STOP_BITS_2 = (1 << 3); // 2 stop bits flag

// Control Register bits
static const uint32_t CR_UART_ENABLE = (1 << 0); // UART enable
static const uint32_t CR_TX_ENABLE = (1 << 8);   // Transmit (TX) enable
static const uint32_t CR_RX_ENABLE = (1 << 9);   // Receive (RX) enable

static volatile uint32_t* pl011_get_register_ptr(const pl011_driver_t *driver, uint32_t offset) {
    return (volatile uint32_t*)(driver->base_address + offset);
}

static uint32_t pl011_read_register(const pl011_driver_t *driver, uint32_t offset) {
    return *pl011_get_register_ptr(driver, offset);
}

static void pl011_write_register(const pl011_driver_t *driver, uint32_t offset, uint32_t value) {
    *pl011_get_register_ptr(driver, offset) = value;
}

static void pl011_wait_for_busy_clear(const pl011_driver_t *driver) {
    // Spin until the busy flag is clear.
    while ((pl011_read_register(driver, FR_OFFSET) & FR_BUSY) != 0) {}
}

int pl011_init(pl011_driver_t *driver, uint64_t base_address, uint64_t base_clock) {
    driver->base_address = base_address;
    driver->base_clock = base_clock;
    driver->baud_rate = 115200;
    driver->data_bits = 8;
    driver->stop_bits = 1;

    return pl011_reset(driver);
}

int pl011_reset(const pl011_driver_t *driver) {
    // Disable the UART.
    uint32_t cr_reg = pl011_read_register(driver, CR_OFFSET);
    cr_reg &= ~CR_UART_ENABLE;
    pl011_write_register(driver, CR_OFFSET, cr_reg);

    // Wait for any pending operations to complete.
    pl011_wait_for_busy_clear(driver);

    // Disable the FIFO and flush the FIFO.
    uint32_t lcr_reg = pl011_read_register(driver, LCR_OFFSET);
    lcr_reg &= ~LCR_FIFO_ENABLE;
    pl011_write_register(driver, LCR_OFFSET, lcr_reg);

    // Set the baud rate.
    // Divisor = (Base Clock / (16 * Baud Rate))
    // 64 * Divisor = 4 * Base Clock / Baud Rate -- Multiply by 64 (2**6) to put in 6 fixed point format, making the fractional part the lower 6 bits.
    uint32_t divisor = 4 * driver->base_clock / driver->baud_rate;
    uint32_t integer_part = (divisor >> 6) & 0xFFFF;
    uint32_t fractional_part = divisor & 0x3F;
    pl011_write_register(driver, IBRD_OFFSET, integer_part);
    pl011_write_register(driver, FBRD_OFFSET, fractional_part);

    lcr_reg = 0x0;

    if (driver->data_bits == 8) {
        lcr_reg |= LCR_DATA_BITS_8;
    } else {
        // TODO: Support other data bits.
        return -1;
    }

    if (driver->stop_bits == 2) {
        lcr_reg |= LCR_STOP_BITS_2;
    } else if (driver->stop_bits != 1) {
        // Invalid stop bits.
        return -1;
    }

    // Configure the line control register (data bits, stop bits, etc.).
    pl011_write_register(driver, LCR_OFFSET, lcr_reg);

    // Disable all interrupts.
    pl011_write_register(driver, IMSC_OFFSET, 0x7FF);

    // Disable DMA.
    pl011_write_register(driver, DMACR_OFFSET, 0);

    // Enable TX and RX.
    pl011_write_register(driver, CR_OFFSET, CR_TX_ENABLE | CR_RX_ENABLE);

    // Enable the UART.
    cr_reg = pl011_read_register(driver, CR_OFFSET);
    cr_reg |= CR_UART_ENABLE;
    pl011_write_register(driver, CR_OFFSET, cr_reg);

    return 0;
}

int pl011_send_char(const pl011_driver_t *driver, char c) {
    pl011_wait_for_busy_clear(driver);
    pl011_write_register(driver, DR_OFFSET, (uint32_t)c);
    pl011_wait_for_busy_clear(driver);
    return 0;
}

int pl011_send(const pl011_driver_t *driver, const char *data, uint64_t size) {
    for (int i = 0; i < size; i++) {
        if (data[i] == '\n') {
            pl011_send_char(driver, '\r');
        }
        pl011_send_char(driver, data[i]);
    }
    return 0;
}

char pl011_receive_char(const pl011_driver_t *driver) {
    // Wait for the receive FIFO to not be empty.
    while ((pl011_read_register(driver, FR_OFFSET) & FR_RXFE) != 0) {}

    // Read the character from the receive FIFO.
    return (char)pl011_read_register(driver, DR_OFFSET);
}
