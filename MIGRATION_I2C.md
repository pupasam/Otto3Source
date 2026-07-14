# Otto3 firmware migration: BCD parallel GPIO → I²C / RheoLink valve control

Branch: `i2c-rheolink-migration`

This change ports the three selector valves (reagent, sample, vacuum) from
4-bit **BCD parallel GPIO** control to **I²C** control using the lab's
**RheoLink** driver for IDEX MX Series II valves on TitanEX/RheoLink boards.

The port was done **at the seam** — only the three `Select*Port()` wrappers were
reimplemented. Every higher-level routine (`RunPumpLine`, aspirate/dispense,
calibration, `runAutomation`, etc.) calls those wrappers unchanged.

> **Status: compile-checked only, NOT hardware-verified.** All five sketches
> compile clean for `arduino:avr:mega`. No valve has been actuated by this
> firmware. See "What could NOT be validated" below.

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

### `OttoFns/RheoLink.h`, `OttoFns/RheoLink.cpp` (new)
- Copied verbatim from `ValveControl/firmware/OttoValve/` (K. Marx's unchanged
  lab driver). This is the canonical copy for the firmware.

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
per valve. (The compile target here is still `arduino:avr:mega` only because
pump/solenoid live on pins 32/33, which a Uno does not have. Move those two to
Uno-range pins and the whole thing fits a 5 V Uno — the controller the RheoLink
bring-up was verified on.)

---

## Hardware steps still required (not done here)

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

## What could NOT be validated without hardware

- **No valve was actuated** by this firmware. Only compilation is verified.
- **Timing behavior changed.** BCD `SelectPort()` was an instantaneous
  `digitalWrite`. `set_position(..., wait_for_completion=true, ...)` now
  **blocks** until the valve confirms the position (or times out at
  `RheoLink_TIMEOUT` = 2000 ms). Routines that assumed near-instant port
  switching and hand-tuned the surrounding `delay()`s (e.g. `RunPumpLine`'s
  `delay(20)/delay(30)`, the tight aspirate/dispense loops in
  `AddSBSReagentMulti`) may need **re-tuning against a wall clock**, since valve
  moves now consume real, variable time inside those sequences. This is the
  top open risk.
- **Error handling is fire-and-forget.** The wrappers ignore the `uint8_t`
  return of `set_position()` to preserve the `void Select*Port(int)` signatures.
  A failed/timed-out move will not halt the protocol. Consider adding a checked
  variant before unattended runs.
- **Address assignment is unverified** — the mapping above is the intended plan;
  the `address_change` step must actually be performed and confirmed on the
  bench.
- **Port range:** the protocol uses ports 1–8 (`VentPort`/`SafeVacB` = 8), well
  within 1–10. BCD previously clamped out-of-range ports to a default; RheoLink
  instead **rejects** a port outside 1–10 (returns code 11 and does not move).
  No current call passes >10, but this is a behavior change to be aware of.
