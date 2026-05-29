/*
 * LittleOS Gajah PHP - rtc.cpp
 * Real-Time Clock driver — C++ HAL
 * Membaca waktu dari CMOS RTC
 */

#include "hal.hpp"

namespace hal {
namespace rtc {

static const uint16_t CMOS_ADDR = 0x70;
static const uint16_t CMOS_DATA = 0x71;

static uint8_t read_cmos(uint8_t reg) {
    hal::ports::outb(CMOS_ADDR, reg);
    return hal::ports::inb(CMOS_DATA);
}

static bool is_updating() {
    hal::ports::outb(CMOS_ADDR, 0x0A);
    return (hal::ports::inb(CMOS_DATA) & 0x80) != 0;
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

void init() {
    /* Tidak perlu inisialisasi khusus */
}

DateTime get_time() {
    /* Tunggu sampai RTC tidak sedang update */
    while (is_updating());

    DateTime dt;
    dt.second = read_cmos(0x00);
    dt.minute = read_cmos(0x02);
    dt.hour   = read_cmos(0x04);
    dt.day    = read_cmos(0x07);
    dt.month  = read_cmos(0x08);
    dt.year   = read_cmos(0x09);

    /* Cek format BCD (register B bit 2) */
    uint8_t regB = read_cmos(0x0B);
    if (!(regB & 0x04)) {
        dt.second = bcd_to_bin(dt.second);
        dt.minute = bcd_to_bin(dt.minute);
        dt.hour   = bcd_to_bin(dt.hour & 0x7F) | (dt.hour & 0x80);
        dt.day    = bcd_to_bin(dt.day);
        dt.month  = bcd_to_bin(dt.month);
        dt.year   = bcd_to_bin((uint8_t)dt.year);
    }

    /* Convert 12-hour ke 24-hour jika perlu */
    if (!(regB & 0x02) && (dt.hour & 0x80)) {
        dt.hour = ((dt.hour & 0x7F) + 12) % 24;
    }

    /* Year: asumsi 2000-an */
    dt.year += 2000;

    /* Hitung hari dalam minggu (Zeller's / simple offset) */
    dt.weekday = 0; /* placeholder */

    return dt;
}

} // namespace rtc
} // namespace hal
