/*
 *  usbserial_compat.hpp
 *
 *  arduino-esp32 3.x dropped the USBSerial object the StampFly sources use
 *  for debug output. On this firmware wendy owns both UART0 (wendy_stdio)
 *  and the USB-Serial/JTAG port (wendy_usj), so Arduino serial must not be
 *  used at all: route everything to stdout (the wendy console) instead.
 */

#ifndef _USBSERIAL_COMPAT_HPP_
#define _USBSERIAL_COMPAT_HPP_

#include <Arduino.h>
#include <stdio.h>

class StdioPrint : public Print {
   public:
    void begin(unsigned long baud) { (void)baud; }
    size_t write(uint8_t c) override { return fputc(c, stdout) == EOF ? 0 : 1; }
    size_t write(const uint8_t *buffer, size_t size) override { return fwrite(buffer, 1, size, stdout); }
    void flush() { fflush(stdout); }
};

extern StdioPrint USBSerial;

#endif
