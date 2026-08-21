# Otto3 operator interface guide

Otto3 is operated through **OttoPanel** — one firmware, two equivalent
interfaces: the **touchscreen** on the GIGA Display Shield (no computer
needed) and a **remote serial console** (9600 baud, for shell/Claude-driven
sessions). Both expose the same actions; the touchscreen adds checklists and
confirmation gates, the serial console skips them (the remote operator is
the confirmation).

---

## 1. Touchscreen

### Home → menu → checklist → run

- **Home** shows three categories: **CALIBRATION** (steps 1–9),
  **RUN** (the full sequencing protocol), **UTILITIES** (primes, rinses,
  dispense/aspirate-all, park, shutdown flush, and the calibration
  inspection tests).
- Tapping a category opens a **2-column button grid** (cell 1 is always
  `< BACK`).
- Tapping an action opens its **pre-run checklist**: short physical
  prerequisites ("RESERVOIRS FULL", "PUMP ARMED (STOP AFTER KEYPAD USE)",
  "VACUUM OPEN, TRAP IN LINE, 12V ON", ...). Every row must be ticked
  before **GO** enables. **CANCEL** returns to the menu.

### The run screen

While an action runs, the display is a live dashboard:

- **Four panels** — reagent valve (current port + reagent name), sample
  valve (current port), vacuum valve + solenoid state, pump state with a
  live countdown for the current pump segment.
- **Step timer** — elapsed vs expected duration for the whole action
  (expected times are computed from the live constants at confirm time).
- **Cue banner** — operator instructions timed to the fluidics:
  calm cues ("PRIMING - NO NEED TO WATCH YET"), armed countdowns
  ("EYES ON DISP VALVE - FRONT ARRIVES T-11s"), and a triple full-screen
  **strobe at T-0** so the watch moment is unmissable. During the purge
  loop each needle gets its own countdown ("RED AT NEEDLE 3").
- **The red STOP column** is always present and always hot. One tap sets
  the abort latch: the running routine unwinds within ~100 ms (`Wait()`
  polls the latch in 100 ms slices; pump/valve/solenoid primitives refuse
  new actions), then the instrument **parks safe** — pump stopped, vacuum
  solenoid closed, valves to reagent 4 / sample 1 / vacuum 1.

### The result screen

When an action finishes, a **DONE** screen shows the measured duration and,
for calibration tests, an inspection prompt (e.g. "INSPECT: AIR SLUG AT SAME
SPOT IN ALL 6 LINES?"). Tap to return to the checklist/menu.

### The Step 5 dispense wizard

CALIBRATION → step 5 runs as a guided wizard: dispense pass → operator
enters the measured volume on-screen → the wizard retunes `mLPumpTime`
**in RAM** and offers another pass. The converged value must still be
written into `OttoFns/constants.ino` by hand (no on-device persistence) —
the wizard shows the number to copy.

---

## 2. Remote serial console

Open the port at **9600 baud** (or use `tools/otto_console.py`, which does
everything below for you). The firmware prints the command table at boot and
on `?`. One character per command, newline-terminated.

### Command table

| Key | Action | Notes |
|-----|--------|-------|
| `1`–`9` | Calibration steps 1–9 | checklists skipped |
| `R` | FULL RUN (the sequencing protocol) | |
| `w` / `c` / `i` | Prime wash / cleavage / incorporation, 2 min | |
| `f` | FULL RINSE (the flush cycle, ~18 min) | |
| `d` | DISPENSE 1 mL to all wells | |
| `a` | ASPIRATE all wells | |
| `k` | PARK valves (reagent 4 / sample 1 / vacuum 1) | |
| `r` | CAL: RESET LINE (wash prime out the vent) | cleans reagent path + vent ONLY |
| `v` | CAL: BUBBLE TO VALVE (static) | superseded by `b` for tuning — static parks read ~0.1 mL low |
| `n` | CAL: BUBBLE TO NEEDLE 1 (static) | |
| `t` | CAL: SPLIT TEST — full production prefix, stops after the split | |
| `p` | CAL: PURGE TEST — the ascending purge only, run right after `t` | |
| `b` | CAL: PARK TEST — `t` truncated at the park; seals the line so the bubble front freezes for reading | |
| `m` | CAL: NEXT SPLIT SLICE — one production slice (descending from 7), reseal after | 6× = the full split, stepwise |
| `u` | CAL: NUDGE TO 7 — ~13 µL titration step; prints the running total since the park | |
| `M` | CAL: SPLIT ALL 6 — the verbatim production split loop in one step | |
| `x` | STOP / ABORT (only accepted during a run) | same latch as the red button |
| `s` | STATUS — state, elapsed, live constants | accepted any time |
| `?` | Help | |

While an action runs the console answers `BUSY (RUNNING)` to everything
except `x` and `s`.

### Phase markers

Every action streams progress markers, so a script (or Claude) can follow a
run without watching the screen:

```
RUN: CAL: SPLIT TEST        <- action accepted and started
STEP: CAL: SPLIT TEST       <- step timer started
CUE: PRIMING - NO NEED TO WATCH YET
CUE: EYES ON DISP VALVE - FRONT ARRIVES T-11s
CUE: NOW                    <- the strobe moment
DONE 183s                   <- finished, measured duration
ABORTED                     <- (instead of DONE) the stop latch fired
```

### The Step-8 stepwise toolkit (`b` → `u` → `m`/`M`)

Decoupled bubble-calibration protocol, every advance operator-gated:

1. `d a r` — the **clean regime**. Run it before every reading (`r` alone
   does not clean the six sample lines).
2. `b` — park the bubble with full production fidelity, then freeze it
   (line sealed; the front cannot creep). Read the front against the valve.
3. `u` (repeat) — advance ~13 µL per press until the first air tip exits
   into line 7. The printed total is the measured reagent-line volume.
4. `M` — the production split in one step; inspect the six air slugs.
   Or `m` six times — the same split one line at a time, resealing between
   slices so every intermediate state can be read.
5. `p` — the production purge; inspect the six wash edges at the needles.

See `KNOWN-ISSUES.md` for what these tests established on 2026-08-21 and
what remains open.

---

## 3. Rules that protect the hardware

- **Never flash while a run is active** — uploading reboots the board
  mid-motion. Confirm `s` shows IDLE first.
- **Pump arming:** after any front-panel keypad use, press the pump's STOP
  key once; remote control only arms after STOP. The keypad locks out while
  the remote contact is closed.
- **Power sequence:** GIGA on **first**, pump power off **first**. An
  unpowered GIGA drives the pump's start input low — the pump runs.
- **Port 6 of the reagent valve must stay dry** (no tubing): routines draw
  air through it. **Port 1 of the sample and vacuum valves is never
  plumbed** — the software parks there and the run logic requires it open.
