#!/usr/bin/env python3
"""Drive OttoPanel's remote serial console from the command line.

The OttoPanel firmware exposes every action as a single-character serial
command at 9600 baud (send `?` for the on-board table). This script sends
commands and streams the firmware's phase markers (RUN:/STEP:/CUE:/DONE)
back, waiting for each command to finish before sending the next — so a
whole bench sequence is one shell line:

    tools/otto_console.py d a r b      # clean regime, then park test
    tools/otto_console.py s            # status (constants + state)
    tools/otto_console.py -i           # interactive: type commands live

The port is auto-detected (GIGA via `arduino-cli board list`, falling back
to /dev/cu.usbmodem*). Requires pyserial.

Rules that cost hours to learn — the script enforces the first two:
  * One command at a time; the firmware replies BUSY during a run
    (only `x` = abort and `s` = status are accepted mid-run).
  * Every command runs to DONE <secs> or ABORTED; a timeout here usually
    means the board reset or the run hung — check the bench.
  * Never flash while a run is active: flashing kills the run mid-motion.
"""

import argparse
import glob
import subprocess
import sys
import time

import serial

# Generous per-command time limits (seconds). Anything not listed gets
# DEFAULT_LIMIT. Derived from bench-measured durations + margin.
LIMITS = {
    "d": 220,   # dispense 1 mL to all wells         (~120 s)
    "a": 180,   # aspirate all wells                 (~100 s)
    "f": 1400,  # full rinse                         (~18 min)
    "r": 90,    # CAL reset line (wash prime)        (~31 s)
    "v": 60,    # CAL bubble to valve (static)
    "n": 90,    # CAL bubble to needle 1 (static)
    "t": 320,   # CAL split test (full production)   (~183 s)
    "b": 320,   # CAL park test (t, stops at park)   (~182 s)
    "p": 90,    # CAL purge test                     (~39 s)
    "m": 30,    # CAL next split slice
    "u": 30,    # CAL nudge to 7 (titration step)
    "M": 60,    # CAL split all 6 (production split) (~11 s)
    "w": 150, "c": 150, "i": 150,   # 2-minute primes
    "k": 30,    # park valves
    "s": 10,    # status
    "?": 10,    # help
}
DEFAULT_LIMIT = 600
# Multi-hour actions: no automatic limit, stream until DONE.
UNLIMITED = {"R", "1", "2", "3", "4", "5", "6", "7", "8", "9"}


def find_port():
    try:
        out = subprocess.run(
            ["arduino-cli", "board", "list"],
            capture_output=True, text=True, timeout=15,
        ).stdout
        for line in out.splitlines():
            if "Giga" in line:
                return line.split()[0]
    except Exception:
        pass
    hits = sorted(glob.glob("/dev/cu.usbmodem*"))
    if hits:
        return hits[0]
    sys.exit("No GIGA found (arduino-cli board list / /dev/cu.usbmodem*).")


def open_port(port):
    s = serial.Serial(port, 9600, timeout=1)
    time.sleep(1.5)          # USB CDC settles after open
    s.reset_input_buffer()
    return s


def run_command(s, cmd, limit):
    """Send one command; stream output until DONE/ABORTED or limit."""
    s.write((cmd + "\n").encode())
    buf, t0 = "", time.time()
    while limit is None or time.time() - t0 < limit:
        chunk = s.read(256).decode(errors="replace")
        if not chunk:
            continue
        sys.stdout.write(chunk)
        sys.stdout.flush()
        buf += chunk
        done = (
            "DONE" in buf or "ABORTED" in buf or "UNKNOWN CMD" in buf
            or "BUSY" in buf or "EXHAUSTED" in buf or "NOT RUNNING" in buf
            or (cmd == "s" and "STATUS" in buf)
            or (cmd == "?" and "STATUS" in buf.replace("s STATUS", ""))
        )
        if done:
            time.sleep(0.5)
            sys.stdout.write(s.read(1024).decode(errors="replace"))
            sys.stdout.flush()
            return buf
    print(f"\n!! TIMEOUT after {limit}s on '{cmd}' — check the bench",
          file=sys.stderr)
    sys.exit(1)


def interactive(s):
    print("Interactive console. Type a command + return; Ctrl-C to quit.")
    run_command(s, "?", 10)
    while True:
        try:
            cmd = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if not cmd:
            continue
        limit = None if cmd in UNLIMITED else LIMITS.get(cmd, DEFAULT_LIMIT)
        run_command(s, cmd, limit)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("commands", nargs="*",
                    help="single-char commands, run in order (e.g. d a r b)")
    ap.add_argument("-p", "--port", help="serial port (default: autodetect)")
    ap.add_argument("-i", "--interactive", action="store_true",
                    help="interactive prompt instead of a command list")
    ap.add_argument("--limit", type=int,
                    help="override the per-command time limit (seconds)")
    args = ap.parse_args()

    if not args.interactive and not args.commands:
        ap.error("give at least one command, or -i")

    s = open_port(args.port or find_port())
    try:
        if args.interactive:
            interactive(s)
            return
        for k, cmd in enumerate(args.commands):
            print(f"\n===== {cmd} =====")
            limit = args.limit or (
                None if cmd in UNLIMITED else LIMITS.get(cmd, DEFAULT_LIMIT))
            out = run_command(s, cmd, limit)
            if "ABORTED" in out:
                sys.exit("Run aborted — stopping the sequence.")
            if k < len(args.commands) - 1:
                time.sleep(2)   # let the panel settle between actions
    finally:
        s.close()


if __name__ == "__main__":
    main()
