# Otto3 Firmware

Arduino firmware for **Otto3**, the Cheeseman Lab's automated in-situ-sequencing
fluidics instrument. An **Arduino GIGA R1** drives:

- three **IDEX MX Series II 10-position selector valves** (reagent, dispensing,
  aspiration) over a shared **I2C bus** using the RheoLink protocol,
- a **Gilson MINIPULS 3** peristaltic pump via its remote-control barrier strip
  (GPIO through a level shifter),
- a **12 V vacuum solenoid** via an optocoupler relay (GPIO).

The instrument runs one plate at a time: reagents are pumped from reservoirs
through the reagent valve, split across the plate's wells by the dispensing
valve, and pulled out through the aspiration valve under house vacuum.

## Getting set up from zero

```sh
# 1. Toolchain (macOS: brew install arduino-cli; or see arduino.github.io/arduino-cli)
arduino-cli core update-index
arduino-cli core install arduino:mbed_giga     # the controller in use
arduino-cli core install arduino:avr           # cross-compile check target

# 2. Libraries (OttoPanel / display builds only)
arduino-cli lib install Arduino_GigaDisplay_GFX Arduino_GigaDisplayTouch

# 3. Python serial driving (tools/otto_console.py)
python3 -m pip install pyserial

# 4. Sanity check: everything should compile clean
arduino-cli compile --fqbn arduino:mbed_giga:giga OttoPanel
arduino-cli compile --fqbn arduino:avr:mega PreRunCalibrationScript
```

On a Mac with USB-C only, connect the GIGA **directly** to the laptop with a
USB-C-to-USB-B cable — dock/monitor hubs silently swallow the serial device.
The board shows up as `/dev/cu.usbmodem*` (never use `/dev/cu.debug-console`).

Day-to-day, the firmware to have on the board is **`OttoPanel/`**: it exposes
everything (calibration steps, the full run, bench utilities) through both the
touchscreen and a remote serial console, so most work needs no reflashing.

Wiring, valve addressing, and the fluidics build live in the master protocol
document (`Otto3_Master_Protocol.html`, kept outside this repo); this README
covers only what the firmware assumes.

## Repository layout

| Path | What it is |
|------|-----------|
| `OttoFns/` | **The shared library — the only place code is edited.** `constants.ino` (pins, I2C addresses, ports, calibrated volumes/times), `LowLevelFns.ino` (valve/pump/solenoid primitives, `setup()`), `OttoFns.ino` (reagent, dispense, aspirate, rinse routines), `RheoLink.h/.cpp` (IDEX I2C valve driver) |
| `RunOtto3/` | Main sequencing run. Waits on serial for the line `BEGIN AUTOMATION`, then executes the full protocol (`runAutomation()` in `OttoFns/RunProtocol.ino`) |
| `OttoPanel/` | Standalone touchscreen control (GIGA Display Shield, no computer): CALIBRATION / RUN / UTILITIES menu tree (calibration steps 1–9, FULL RUN, one-off bench actions like line primes, aspirate-all and the shutdown flush), per-action pre-run checklists gating a GO/CANCEL confirm, live dashboard with an always-hot red STOP column that aborts immediately and parks the instrument, and a guided Step 5 dispense-calibration wizard that retunes `mLPumpTime` in RAM without reflashing. GIGA-only |
| `PreRunCalibrationScript/` | **The single calibration entry point** — Steps 1–9, protocol documented in-line in the sketch |
| `ShutdownScript/` | Post-run flush with water reservoirs, plus manual shutdown checklist |
| `ValidationScripts/` | Hardware exercisers: sweep each valve through its ports, cycle the solenoid, run the pump |
| `diagnostics/` | Tiny single-purpose bench sketches (pump/solenoid pulse tests, vacuum port scan, safe-state parkers, display smoke test) — see its README |
| `tools/` | `otto_console.py` — drive OttoPanel's serial console from the shell: `tools/otto_console.py d a r b` runs a whole bench sequence |
| `docs/` | `UI.md` / `UI.pdf` — how the touchscreen and serial console work, for operators |
| `KNOWN-ISSUES.md` | **Read before calibrating**: open bugs and unresolved calibration findings, with the evidence behind each |
| `MIGRATION_I2C.md` | Valve-control architecture: addressing, driver behavior (quiet window, bounded retries), bring-up procedure, build mechanics |

### How the shared library works

Each sketch folder contains a `src/` directory of **relative symlinks** into
`OttoFns/` (`src/constants.h -> ../../OttoFns/constants.ino`, etc.). Every
sketch does:

```c
#include "src/constants.h"
#include "src/LowLevelFns.h"
#include "src/OttoFns.h"
```

so **an edit in `OttoFns/` propagates to every sketch** — there is exactly one
copy of the library. Never edit anything under a sketch's `src/`; those are the
same files. The symlinks are named `.h` deliberately so `arduino-cli` copies
them into its build tree without concatenating them — the reasons are spelled
out in [MIGRATION_I2C.md](MIGRATION_I2C.md), which is also the reference for
everything valve-driver related (never poll a moving valve, 100 kHz bus,
address assignment, retry bounds).

All sketches compile for both `arduino:mbed_giga:giga` (the controller in use)
and `arduino:avr:mega`, except `OttoPanel`, which is GIGA-only (it needs the
Display Shield: `Arduino_GigaDisplay_GFX` + `Arduino_GigaDisplayTouch`).

## Hardware map (what the firmware assumes is plumbed and wired)

### Selector valves (I2C, addresses in `OttoFns/constants.ino`)

| Valve | Role | 7-bit addr | Hardware |
|-------|------|-----------|----------|
| Reagent | selects which reservoir feeds the pump | `0x07` | MLP778-605 (1/16") |
| Sample / dispensing | routes pump output to one well's needle | `0x08` | MLP778-605 (1/16") |
| Vacuum / aspiration | connects one well's needle to house vacuum | `0x09` | MLP778-**606** (1/8") — verify the suffix on the physical valve |

**Reagent valve port map** (`getReagentPort()` in `constants.ino`):

| Port | Reagent |
|------|---------|
| 1 | CLEAVAGE |
| 2 | INCORPORATION |
| 3 | WASH |
| 4 | empty (fallback) |
| 6 | **AIR — must stay dry.** Routines draw air through it to push lines clear; fitting tubing here breaks every air push |

**Sample and vacuum valves:** well lines occupy **ports 2–7**
(`SampleWells[]`/`VacuumWells[]`). **Port 1 is never plumbed** — the software
requires it unconnected (`SafeVacA = 1` parks the vacuum valve there). Port 8
is the sample valve's vent (`VentPort`) and the vacuum valve's other safe park
(`SafeVacB`).

### Pump — Gilson MINIPULS 3 (GPIO 33, active-LOW, via BSS138 level shifter ch. 3)

Barrier strip wiring:

| Pin | Function |
|-----|----------|
| 1 | Direction — **permanently jumpered to pin 2 (ground)** = always CCW under remote. Remote start **ignores the keypad direction setting** |
| 2, 4, 6 | Ground |
| 3 | Start/stop — grounded = RUN. Driven by GIGA pin 33 through the shifter (the pump pulls this line to 5 V itself, so it must go through the shifter, never direct to a 3.3 V pin) |
| 5 | Analog speed — left open = pump runs at the panel-set speed |

Two operational rules that are easy to get wrong:

- **Remote control arms only after the front-panel STOP key.** After any manual
  keypad use, press STOP once or the remote contact does nothing. While the
  remote contact is closed, the keypad is locked out.
- **The pump RUNS whenever the GIGA is unpowered** — the shifter's pull-up sags
  to the dead 5 V rail, which reads as "grounded" to the pump. Therefore:
  **power the GIGA before the pump, and cut pump power first at shutdown.**

### Vacuum solenoid (GPIO 32, active-HIGH)

GIGA pin 32 drives an optocoupler relay input; the relay switches the 12 V
solenoid loop. HIGH = solenoid open = vacuum applied.

## Calibration workflow

Calibration is a **once-per-screen** activity, not a per-run one: tune the
constants at the start of a screening campaign (or after any plumbing change —
new tubing, new needle, moved reservoirs), validate, and then run the screen
on the frozen values. All calibrated values live in `OttoFns/constants.ino`
(`mLPumpTime`, `vacTime`, `SampleLineVolume`, `SampleNeedleVolume`,
`ReagentLineVolume`, `adjustSampleVolMicro`). The ladder is nine steps; the
full protocol — setup prerequisites, pass criteria per step, which constant
each step tunes — is documented in-line in
[`PreRunCalibrationScript.ino`](PreRunCalibrationScript/PreRunCalibrationScript.ino).

**Before starting, read [KNOWN-ISSUES.md](KNOWN-ISSUES.md)** — Step 8 has
unresolved findings that change how its results should be interpreted.

There are two ways to run the steps:

**A. OttoPanel (recommended — no reflashing between steps).** Flash
`OttoPanel/` once; run steps from the touchscreen (CALIBRATION menu, with
checklists) or the serial console (`1`–`9`, checklists skipped). Edit a
constant in `constants.ino`, reflash OttoPanel, repeat. Step 5 also has an
on-screen wizard that retunes `mLPumpTime` in RAM per pass. Step 8 has a
dedicated stepwise toolkit (`b`/`u`/`m`/`M` — see [docs/UI.md](docs/UI.md)).

**B. Flash-per-step (`PreRunCalibrationScript/`)** — the original method,
still the fallback when the panel build is unavailable:

1. **Uncomment exactly one step line** in the sketch (edit constants in
   `OttoFns/constants.ino` if the previous pass said to).
2. **Flash.** Upload both flashes and immediately starts the step — be ready at
   the instrument before uploading.
3. **Observe** against the step's pass criteria.
4. The step runs **once per boot**, then `stopLoop()` parks the instrument safe
   (pump stopped, solenoid closed). **Pressing the GIGA's RESET button re-runs
   the flashed step** without re-uploading.

Step summary: 1–3 — prime the wash / cleavage / incorporation lines; 4 —
end-to-end prime of all lines (doubles as the flush cycle); 5 — dispensation
volume calibration (`mLPumpTime`); 6 — dispense + aspirate (`vacTime`); 7 —
nested aspirate/dispense motion profile, unseeded then cell-seeded plate; 8 —
`AddSBSReagent` air-bubble tuning with incorporation (`ReagentLineVolume`,
then `adjustSampleVolMicro`); 9 — validation of those values with cleavage.
The line/needle dead volumes (`SampleLineVolume`, `SampleNeedleVolume`) are
computed constants, not a numbered step — see the comment block between
Steps 7 and 8 in the script.

## Driving the bench from a shell (Claude Code / arduino-cli / tools)

Everything is scriptable — no Arduino IDE required.

```sh
# Find the board (the GIGA appears as /dev/cu.usbmodem*)
arduino-cli board list

# Compile a sketch (any sketch folder)
arduino-cli compile --fqbn arduino:mbed_giga:giga OttoPanel

# Flash — NOTE: for the flash-per-step sketches this STARTS the run
# immediately; OttoPanel just boots to its menu (safe)
arduino-cli upload -p <port> --fqbn arduino:mbed_giga:giga OttoPanel
```

With OttoPanel flashed, `tools/otto_console.py` wraps the serial console —
it auto-detects the port, sends commands one at a time, streams the
`RUN:/STEP:/CUE:/DONE` phase markers, and refuses to race an active run:

```sh
tools/otto_console.py s            # status: state + live constants
tools/otto_console.py d a r        # the clean regime before any reading
tools/otto_console.py d a r b      # ...then park the calibration bubble
tools/otto_console.py -i           # interactive prompt
```

The full command table is in [docs/UI.md](docs/UI.md) (or send `?` to the
board). Two hard rules when driving remotely: **never flash while a run is
active** (flashing reboots the board mid-motion), and **run the clean regime
`d a r` before any calibration reading** (RESET LINE alone only cleans the
reagent path and vent, not the six sample lines).

Operational notes:

- **Serial is 9600 baud.** `setup()` prints the well/port arrays and
  `SETUP COMPLETE`; `initValves()` prints a `begin=` code per valve (0 = OK,
  anything else means wiring/address/pull-up trouble — see
  [MIGRATION_I2C.md](MIGRATION_I2C.md)).
- Each calibration/validation sketch runs its uncommented step **once per
  boot**, then parks safe in `stopLoop()` (pump stopped, solenoid closed).
  GIGA RESET re-runs it.
- `RunOtto3` is the exception: it idles until it receives `BEGIN AUTOMATION`
  (newline-terminated) on serial, then runs the full protocol.
- Respect the power sequencing and pump-arming rules above before any upload:
  the pump must be armed for remote (STOP pressed) and the GIGA must be
  powered before the pump is.
- On a Mac with USB-C only, connect the board **directly** to the laptop —
  dock/monitor hubs can silently swallow the serial device. Never select
  `/dev/cu.debug-console` as the port.
