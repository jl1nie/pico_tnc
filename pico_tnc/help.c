#include "help.h"

static const char *const help_lines[] = {
    "",
    "Commands are Case Insensitive",
    "Use Backspace Key (BS) for Correction",
    "Use the DISP command to desplay all options",
    "Connect GPS for APRS Operation, (GP4/GP5/9600bps)",
    "Connect to Terminal for Command Interpreter, (USB serial or GP0/GP1/115200bps)",
    "",
    "Commands (with example):",
    "MYCALL (mycall jn1dff-2)",
    "UNPROTO (unproto jn1dff-14 v jn1dff-1) - 3 digis max",
    "BTEXT (btext Bob)-100 chars max",
    "BEACON (beacon every n)- n=0 is off and 1<n<60 mins",
    "MONitor (mon all,mon me, or mon off)",
    "DIGIpeat (digi on or digi off)",
    "MYALIAS (myalias RELAY)",
    "PERM (PERM)",
    "ECHO (echo on or echo off)",
    "GPS (gps $GPGGA or gps $GPGLL or gps $GPRMC)",
    "TRace (tr xmit or tr rcv) - For debugging only",
    "TXDELAY (txdelay n 0<n<201 n is number of delay flags to send)",
    "CALIBRATE (Calibrate Mode - Testing Only)",
    "CONverse (con)",
    "",
    NULL,
};

static tty_t *help_ttyp;
static int help_line_idx;
static bool help_active;
static bool help_ok_pending;

bool help_handle_command(tty_t *ttyp, uint8_t *buf, int len)
{
    (void)buf;
    (void)len;

    help_ttyp = ttyp;
    help_line_idx = 0;
    help_active = true;
    help_ok_pending = false;

    return true;
}

bool cmd_help(tty_t *ttyp, uint8_t *buf, int len)
{
    return help_handle_command(ttyp, buf, len);
}

void help_poll(void)
{
    if (help_active) {
        const char *line = help_lines[help_line_idx];

        if (line) {
            tty_write_str(help_ttyp, line);
            tty_write_str(help_ttyp, "\r\n");
            help_line_idx++;
            return;
        }

        help_active = false;
        help_ok_pending = true;
        return;
    }

    if (help_ok_pending) {
        tty_write_str(help_ttyp, "\r\nOK\r\n");
        help_ok_pending = false;
        help_ttyp = NULL;
    }
}

bool help_is_response_pending(void)
{
    return help_active || help_ok_pending;
}
