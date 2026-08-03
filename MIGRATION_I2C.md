# Otto3 firmware migration: BCD parallel GPIO → I²C / RheoLink valve control

Branch: `i2c-rheolink-migration`

This change ports the three selector valves (reagent, sample, vacuum) from
4-bit **BCD parallel GPIO** control to **I²C** control using the lab's
**RheoLink** driver for IDEX MX Series II valves on TitanEX/RheoLink boards.

The port was done **at the seam** — only the three `Select*Port()` wrappers were
reimplemented. Every higher-level routine (`RunPumpLine`, aspirate/dispense,
calibration, `runAutomation`, etc.) calls those wrappers unchanged.

> **Status: hardware-verified 2026-08-03.** All five sketches compile clean for
> **both** `arduino:mbed_giga:giga` and `arduino:avr:mega`. All three valves are
> addressed and actuated over I²C from this driver: **270 moves, zero failures**,
> including round-robin across the fleet and with the solenoid interleaved.
> See "Open risks" below — protocol timing is the live one.

---

## Valve → I²C address mapping

Set in `OttoFns/constants.ino` as **7-bit** addresses (as printed by `i2c_scanner`):

| Valve   | Constant             | 7-bit addr | 8-bit write addr | Assign with |
|---------|----------------------|-----------|------------------|-------------|
| Reagent | `ReagentValveAddr7`  | `0x07`    | `0x0E` (factory) | ships this way |
| Sample  | `SampleValveAddr7`   | `0x08`    | `0x10`           | `address_change` sketch + power-cycle |
| Vacuum  | `VacuumValveAddr7`   | `0x09`    | `0x12`           | `address_change` sketch + power-cycle |

`RheoLink::begin()` takes the **8-bit** write address and shifts it internally,
so the init path passes `addr7 << 1`. Valves are 10-position: `VALVE_POS_MIN = 1`,
`VALVE_POS_MAX = 10`. Bus speed is fixed at **100 kHz** (`VALVE_I2C_HZ`); 1 MHz
hangs the RheoLink bus.

If you only have one addressed valve on the bench, temporarily point all three
constants at the same address to exercise the code path.

---

## What changed, file by file

### `OttoFns/constants.ino`
- **Removed** the three BCD pin arrays: `ReagentPins{36,37,38,39}`,
  `VacuumPins{42,43,44,45}`, `SamplePins{48,49,50,51}`.
- **Added** the three 7-bit I²C address constants plus `VALVE_POS_MIN/MAX` and
  `VALVE_I2C_HZ`.
- **Kept** `SolenoidPin = 32` and `PumpPin = 33` on plain GPIO — the pump and
  vacuum solenoid are **not** I²C devices and still use `digitalWrite`.

### `OttoFns/LowLevelFns.ino`
- `#include "RheoLink.h"` and three global `RheoLink` objects
  (`reagentValve`, `sampleValve`, `vacuumValve`).
- New `initValves()`: `Wire.begin()`, `Wire.setClock(100000)`, then
  `begin(Wire, addr7 << 1, 1, 10)` on each valve; prints each `begin=` result.
- `setup()`: the three BCD `pinMode` loops are replaced by a single
  `initValves()` call. Solenoid/pump `pinMode` unchanged.
- **Removed** the BCD `binaryTable[][]` lookup and `SelectPort(int[], int)`.
- The three wrappers `SelectReagentPort/SelectSamplePort/SelectVacuumPort` keep
  their **exact signatures** but now call
  `valve.set_position(port, true, RheoLink_TIMEOUT)`.

### `OttoFns/RheoLink.h`, `OttoFns/RheoLink.cpp`
- Originally K. Marx's lab driver, now **modified** — see "Why the driver goes
  quiet during a move" and "Bounded retry loops" below. `ValveControl/firmware/`
  carries the same modified copy; keep the two in step.

### Sketch folders (`RunOtto3/`, `ValidationScripts/`, `OneTimeCalibrationScript/`, `PreRunCalibrationScript/`, `ShutdownScript/`)
- Each `*.ino` previously `#include`d OttoFns via **hard-coded absolute paths**
  (`</Users/kirbybry/Documents/Otto3/OttoFns/...>` / `.../Arduino/OttoFns/...`),
  which only compiled on the original author's machine. These are now portable
  relative includes:
  ```
  #include "src/constants.h"
  #include "src/LowLevelFns.h"
  #include "src/OttoFns.h"
  ```
- Each sketch has a `src/` subfolder of **relative symlinks** pointing at the
  single canonical source in `OttoFns/`:
  - `src/constants.h`   → `../../OttoFns/constants.ino`
  - `src/LowLevelFns.h` → `../../OttoFns/LowLevelFns.ino`
  - `src/OttoFns.h`     → `../../OttoFns/OttoFns.ino`
  - `src/RheoLink.h`    → `../../OttoFns/RheoLink.h`
  - `src/RheoLink.cpp`  → `../../OttoFns/RheoLink.cpp`

  **Why this shape (build-mechanics note):** the OttoFns files must be
  *`#include`d as one translation unit*, not auto-concatenated — the `Reagent`
  enum is defined in `constants.ino` and used in function signatures in
  `OttoFns.ino`, and Arduino's auto-prototype generator reorders concatenated
  `.ino` files and breaks on it (verified: it fails with "'Reagent' has not been
  declared"). `arduino-cli` copies the sketch tree to a build dir before
  compiling, so a sibling `../OttoFns/` include does not survive the copy, but a
  `src/*.h` file inside the sketch **is** copied. Naming the symlinks `.h`
  (instead of `.ino`) keeps `arduino-cli` from concatenating them while still
  copying them. `RheoLink.cpp` in `src/` compiles as a normal unit.
  `OttoFns/` remains the single editable source of truth — edit there, all five
  sketches pick it up through the symlinks.

---

## GPIO freed by this migration

I²C now drives all three valves over **2 shared wires** (SDA/SCL) plus GND.
The 12 valve GPIO pins are freed: reagent **36–39**, vacuum **42–45**, sample
**48–51**. Only **pump (33)** and **solenoid (32)** remain on GPIO.

Consequence: the instrument no longer needs a Mega's high pin count *for the
valves*, and the bus scales to more valves by address instead of by 4 more pins
per valve. (All five sketches build for **both** `arduino:mbed_giga:giga` — the controller
actually in use — and `arduino:avr:mega`. Pump/solenoid live on pins 32/33, which
a Uno does not have; move those two to Uno-range pins and the whole thing would
fit a 5 V Uno as well.)

---

## Bring-up procedure (completed 2026-08-03 — repeat for valves 4-6)

1. **Assign a unique even 8-bit address to each valve** with the
   `address_change` sketch, **one valve at a time**, power-cycling after each:
   sample → `0x10`, vacuum → `0x12` (reagent keeps factory `0x0E`). Confirm with
   `i2c_scanner` that 0x07/0x08/0x09 all appear on the bus.
2. **Wire the I²C bus:** each valve J3 → SDA (J3-1), SCL (J3-2), GND (J3-4);
   **leave J3-3 (+5 V) open** on a 5 V controller. J3 is 2.00 mm Milli-Grid, not
   2.54 mm Dupont. Valve power J1: J1-1 = GND, J1-2 = +24 V.
3. **Add 4.7 kΩ pull-ups** on SDA→5 V and SCL→5 V for a reliable bus.
4. **Controller voltage:** a 5 V Uno/Mega wires straight through. A **3.3 V
   controller (e.g. GIGA R1) needs a bidirectional level shifter** — it is not
   5 V tolerant and can be damaged otherwise.
5. Confirm `initValves()` prints `begin=0` for all three valves before running a
   protocol; a non-zero code means wiring/address/pull-up trouble.

---

## Open risks

- **Protocol timing is the live risk.** BCD `SelectPort()` was an instantaneous
  `digitalWrite`. A valve move now takes a **measured ~723 ms** end to end: the
  travel itself is ~276 ms, plus `RheoLink_QUIET_MS` (600 ms) during which the
  driver must not touch the bus (see below), plus confirmation. Routines that
  hand-tuned the surrounding `delay()`s around near-instant port switching
  (`RunPumpLine`'s `delay(20)/delay(30)`, the tight aspirate/dispense loops in
  `AddSBSReagentMulti`) need **re-tuning against a wall clock**. Nothing about
  the fluidics has been timed yet.
- **Error handling is fire-and-forget.** The wrappers ignore the `uint8_t`
  return of `set_position()` to preserve the `void Select*Port(int)` signatures,
  so a failed move will not halt the protocol. This matters more now that we
  know moves *can* fail and that a failure is silent — add a checked variant
  before any unattended run.
- **Six-valve build untested.** Valves 4-6 are not built or addressed, and the
  24 V 6 A supply covers three with headroom but wants 8 A for six.

## Bounded retry loops

Both retry loops in the original driver spun forever: at `retry_count == max`
they stopped incrementing while the loop condition still passed, so a
persistently failing command locked the CPU at full speed with no delay. That —
not `Wire` blocking — was the "hang" that froze unrelated GPIO, leaving pump and
solenoid in whatever state they held. Both loops now `break`, so every driver
call is bounded and returns.

## Why the driver goes quiet during a move

A valve NACKs every I²C transaction while it is physically travelling. Polling it
through that window makes its I²C interface stop acknowledging its own address
for 30-60 s — reproducible on all three valves after 9-14 consecutive moves, with
no valve fault code reported. `set_position()` therefore stays silent for
`RheoLink_QUIET_MS` after issuing a move, then confirms at `RheoLink_POLL_MS`
intervals. Do not reduce the quiet period to speed up moves without re-running
the stress test.

## Verified on the bench

- Addresses `0x07` (reagent), `0x08` (sample), `0x09` (vacuum) all answer and
  move; assignment persists across power-down.
- **Port range:** the protocol uses ports 1-8 (`VentPort`/`SafeVacB` = 8), well
  within 1-10. BCD previously clamped out-of-range ports to a default; RheoLink
  instead **rejects** a port outside 1-10 (returns code 11 and does not move).
  No current call passes >10, but this is a behavior change to be aware of.
