// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


// The termios Structure
typedef unsigned char cc_t;             // Used for terminal special characters
typedef unsigned int speed_t;           // Used for terminal baud rates
typedef unsigned int tcflag_t;          // Used for terminal modes

#define NCCS 12                         // Size of the array c_cc for control characters

struct termios {
    tcflag_t c_iflag;                   // Input modes
    tcflag_t c_oflag;                   // Output modes
    tcflag_t c_cflag;                   // Control modes
    tcflag_t c_lflag;                   // Local modes
    cc_t c_cc[NCCS];                    // Control characters
    speed_t c_ispeed;
    speed_t c_ospeed;
};

#define VEOF 0
#define VEOL 1
#define VERASE 2
#define VINTR 3
#define VKILL 4
#define VMIN 5
#define VQUIT 6
#define VSTART 7
#define VSTOP 8
#define VSUSP 9
#define VTIME 10

// Input modes
#define BRKINT  0x00000001              // Signal interrupt on break.
#define ICRNL   0x00000002              // Map CR to NL on input.
#define IGNBRK  0x00000004              // Ignore break condition.
#define IGNCR   0x00000008              // Ignore CR.
#define IGNPAR  0x00000010              // Ignore characters with parity errors.
#define INLCR   0x00000020              // Map NL to CR on input.
#define INPCK   0x00000040              // Enable input parity check.
#define ISTRIP  0x00000080              // Strip 8th bit off characters.
#define IXANY   0x00000100              // Enable any character to restart output.
#define IXOFF   0x00000200              // Enable start/stop input control.
#define IXON    0x00000400              // Enable start/stop output control.
#define PARMRK  0x00000800              // Mark parity and framing errors.

// Output modes
#define OPOST   0x00000001              // Post-process output.
#define ONLCR   0x00000002              // Map NL to CR-NL on output.
#define OCRNL   0x00000004              // Map CR to NL on output.
#define ONOCR   0x00000008              // No CR output at column 0.
#define ONLRET  0x00000010              // NL performs CR function.
#define OFDEL   0x00000020              // Fill is DEL.
#define OFILL   0x00000040              // Use fill characters for delay.
#define NLDLY   0x00000080              // Select newline delays:
#define NL0     0x00000000              //   Newline type 0.
#define NL1     0x00000080              //   Newline type 1.
#define CRDLY   0x00000300              // Select carriage-return delays:
#define CR0     0x00000000              //   Carriage-return delay type 0.
#define CR1     0x00000100              //   Carriage-return delay type 1.
#define CR2     0x00000200              //   Carriage-return delay type 2.
#define CR3     0x00000300              //   Carriage-return delay type 3.
#define TABDLY  0x00000c00              // Select horizontal-tab delays:
#define TAB0    0x00000000              //   Horizontal-tab delay type 0.
#define TAB1    0x00000400              //   Horizontal-tab delay type 1.
#define TAB2    0x00000800              //   Horizontal-tab delay type 2.
#define TAB3    0x00000c00              // Expand tabs to spaces.
#define BSDLY   0x00001000              // Select backspace delays:
#define BS0     0x00000000              //   Backspace-delay type 0.
#define BS1     0x00001000              //   Backspace-delay type 1.
#define VTDLY   0x00002000              // Select vertical-tab delays:
#define VT0     0x00000000              //   Vertical-tab delay type 0.
#define VT1     0x00002000              //   Vertical-tab delay type 1.
#define FFDLY   0x00004000              // Select form-feed delays:
#define FF0     0x00000000              //   Form-feed delay type 0.
#define FF1     0x00004000              //   Form-feed delay type 1.

// Baud Rate Selection
#define B0 0 // hang up
#define B50 50
#define B75 75
#define B110 110
#define B134 134
#define B150 150
#define B200 200
#define B300 300
#define B600 600
#define B1200 1200
#define B1800 1800
#define B2400 2400
#define B4800 4800
#define B9600 9600
#define B19200 19200
#define B38400 38400

// Control Modes
#define CBAUD   0x00000000              // Reserved (not in POSIX).
#define CSIZE   0x00000003              // Character size:
#define CS5     0x00000000              //   5 bits
#define CS6     0x00000001              //   6 bits
#define CS7     0x00000002              //   7 bits
#define CS8     0x00000003              //   8 bits
#define CSTOPB  0x00000004              // Send two stop bits, else one.
#define CREAD   0x00000008              // Enable receiver.
#define PARENB  0x00000010              // Parity enable.
#define PARODD  0x00000020              // Odd parity, else even.
#define HUPCL   0x00000040              // Hang up on last close.
#define CLOCAL  0x00000080              // Ignore modem status lines.

// Local Modes
#define ECHO    0x00000001              // Enable echo.
#define ECHOCTL 0x00000002              // Reserved (not in POSIX).
#define ECHOE   0x00000004              // Echo erase character as error-correcting backspace.
#define ECHOK   0x00000008              // Echo KILL.
#define ECHOKE  0x00000010              // Reserved (not in POSIX).
#define ECHONL  0x00000020              // Echo NL.
#define ECHOPRT 0x00000040              // Reserved (not in POSIX).
#define ICANON  0x00000080              // Canonical input (erase and kill processing).
#define IEXTEN  0x00000100              // Enable implementation-defined input processing.
#define ISIG    0x00000200              // Enable signals.
#define NOFLSH  0x00000400              // Disable flush after interrupt or quit.
#define TOSTOP  0x00000800              // Send SIGTTOU for background output.

// The winsize Structure
struct winsize {
    unsigned short ws_row;              // Rows, in characters.
    unsigned short ws_col;              // Columns, in characters.
};

// Attribute Selection
#define TCSANOW 0                       // Change attributes immediately.
#define TCSADRAIN 1                     // Change attributes when output has drained.
#define TCSAFLUSH 2                     // Change attributes when output has drained; also flush pending input.

// Line Control
#define TCIFLUSH 0                      // Flush pending input.
#define TCIOFLUSH 1                     // Flush both pending input and untransmitted output.
#define TCOFLUSH 2                      // Flush untransmitted output.

#define TCIOFF 0                        // Transmit a STOP character, intended to suspend input data.
#define TCION 1                         // Transmit a START character, intended to restart input data.
#define TCOOFF 2                        // Suspend output.
#define TCOON 3                         // Restart output.


speed_t cfgetispeed(const struct termios *termios_p);

speed_t cfgetospeed(const struct termios *termios_p);

int cfsetispeed(struct termios *termios_p, speed_t speed);

int cfsetospeed(struct termios *termios_p, speed_t speed);

int tcdrain(int fd);

int tcflow(int fd, int action);

int tcflush(int fd, int queue_selector);

int tcgetattr(int fd, struct termios *termios_p);

pid_t tcgetsid(int fd);

int tcgetwinsize(int fd, struct winsize *ws);

int tcsendbreak(int fd, int duration);

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p);

int tcsetwinsize(int fd, const struct winsize *ws);
