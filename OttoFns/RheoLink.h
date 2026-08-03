/* RheoLink.h
   Define RheoLink Class
   https://www.idex-hs.com/docs/default-source/product-manuals/rheolink-i2c-communication-protocol-for-titanex.pdf.zip
   Control and IDEX selector valve using I2C

   This class implements the folowing features:
        - Initialization
        - Set new I2C address
        - Set position (with direction of rotation)
        - Get current position
        - Report status

   Class Members:
    Variables:
        uint8_t address_ - I2C address of the device
        TwoWire *w_ - Pointer to Wire object
    Functions:
        RheoLink: set the private variables
        bool begin: Initialize the device. Return false if failed
        uint8_t send_command: Send a command to the selector valve and return an error
        uint8_t read_register: Read a register in the selector valve and return its value or an error

       Created on: 8/10/2023
         Author: Kevin Marx
*/
#ifndef RHEOLINK_H_
#define RHEOLINK_H_

#include <Wire.h>
#include <Arduino.h>

/* Error codes:
              99 – valve failure (valve can not be homed)
              88 – non-volatile memory error
              77 – valve configuration error or command mode error
              66 – valve positioning error
              55 – data integrity error
              44 – data CRC error
              3x – Problem connecting over I2C
                  31 – Data too long for buffer
                  32 – NACK on address tx
                  33 – NACK on data tx
                  34 – other error
                  35 – timeout
                  36 – other other error
              22 – Not initialized
              11 – Position out of range
              current valve position (1 to N) otherwise
*/

// Commands - accessible outside this file
enum RheoLinkCommand_t {
  RheoLink_POS = 'P',
  RheoLink_CW = '-',
  RheoLink_CCW = '+',
  RheoLink_NEW_ADDR = 'N',
  RheoLink_STATUS = 'S'
};
// Definitions for this file only
#define RheoLink_DUMMY_DATA 'x'
#define RheoLink_TIMEOUT 2000
#define RheoLink_MAX_RETRIES 5
#define RheoLink_RETRY_DELAY 10

// Quiet period after commanding a move, during which we put NO traffic on the
// bus at all. A valve NACKs every transaction while it is physically moving,
// and polling it through that window makes its I2C interface stop answering
// entirely for 30-60 s -- measured, reproducibly, on all three valves after
// 9-14 consecutive moves. Staying silent while it travels: 180 moves, zero
// failures. A one-step move measures ~276 ms; 600 ms covers a multi-step move
// with margin. Cheap next to the seconds of pumping between valve changes.
#define RheoLink_QUIET_MS 600
// How often to check AFTER the quiet period, if the valve still is not there.
// Deliberately slow: this path should rarely run, and 5 ms polling is exactly
// what caused the fault above.
#define RheoLink_POLL_MS 100

class RheoLink {
  public:
    RheoLink();
    uint8_t begin(TwoWire &w, uint8_t address, uint8_t p_min, uint8_t p_max);
    // max_retries defaults preserve the original behaviour for one-shot commands.
    // Pass 0 when polling: a poll loop must not retry, because the valve NACKs
    // by design while it is moving and a retry storm there wedges the bus.
    uint8_t send_command(RheoLinkCommand_t cmd, uint8_t data = RheoLink_DUMMY_DATA,
                         uint8_t max_retries = RheoLink_MAX_RETRIES);
    uint8_t read_register(RheoLinkCommand_t target,
                          uint8_t max_retries = RheoLink_MAX_RETRIES);
    uint8_t block_until_done(uint32_t timeout = RheoLink_TIMEOUT);
    uint8_t block_until_position_reached(uint8_t pos, uint32_t timeout = RheoLink_TIMEOUT);
    uint8_t set_position(uint8_t pos, bool wait_for_completion = true, uint32_t timeout = RheoLink_TIMEOUT);
    uint8_t pos_min;
    uint8_t pos_max;
  private:
    uint8_t address_;
    TwoWire *w_;
    bool init_;
};

// RheoLink_DUMMY_DATA is deliberately NOT #undef'd here: RheoLink.cpp needs it
// to forward the default data byte when passing an explicit max_retries, and
// the other RheoLink_* defines above are left visible too.
#endif /* RHEOLINK_H_ */
