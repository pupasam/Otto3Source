// ============================================================================
// Otto3 — Pre-run calibration & checkout (single entry point)
// ============================================================================
// HOW TO USE
//   Exactly ONE step line is uncommented at a time. Flash, watch, re-comment,
//   move on. The GIGA RESET button re-runs the flashed step. Each step runs
//   once per boot, then stopLoop() parks the instrument safe (pump stopped,
//   vacuum solenoid closed).
//
// SETUP (before any step)
//   - All reagent slots loaded with PR2; lines reach the reservoir bottoms.
//   - Manifold on an EMPTY, UNSEEDED test plate (Step 7 is later repeated on
//     a cell-seeded plate).
//   - Wall vacuum open, trap flask in line, 12 V solenoid supply on.
//   - Pump: after ANY manual keypad use, press the front-panel STOP key once —
//     remote control only arms after STOP, and the keypad locks out while the
//     remote contact is closed.
//   - Power sequence: GIGA powered BEFORE pump power on; pump power off FIRST
//     at shutdown — an unpowered GIGA drives the pump remote input low (=RUN).
//
// CALIBRATED VALUES live in src/constants.h (a symlink to
// ../../OttoFns/constants.ino — one edit propagates to every sketch):
//   mLPumpTime, vacTime, SampleLineVolume, SampleNeedleVolume,
//   ReagentLineVolume, adjustSampleVolMicro
// ============================================================================

// Live dashboard + step timer on the GIGA Display Shield. Remove the define
// (or build for a board without ARDUINO_GIGA) to compile display-less; the
// ottoStep*/ottoPanel* calls then become no-ops.
#if defined(ARDUINO_GIGA)
#define OTTO_DISPLAY_ENABLED
#endif

#include "src/constants.h"
#include "src/LowLevelFns.h"
#include "src/OttoFns.h"

void loop() {

  // --------------------------------------------------------------------------
  // STEP 1 — prime the wash line (reagent valve -> pump -> vent 8).
  // PASS: line draws for the full run; flow out the vent is bubble-free.
  // Repeat passes (reset) until clean — ~2 min total priming per line.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 1 PRIME WASH", 20); RunPumpLine(WASH, 8, 20);

  // --------------------------------------------------------------------------
  // STEP 2 — prime the cleavage line. Same PASS criteria as Step 1.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 2 PRIME CLEAVAGE", 20); RunPumpLine(CLEAVAGE, 8, 20);

  // --------------------------------------------------------------------------
  // STEP 3 — prime the incorporation line. Same PASS criteria as Step 1.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 3 PRIME INCORPORATION", 20); RunPumpLine(INCORPORATION, 8, 20);

  // --------------------------------------------------------------------------
  // STEP 4 — prime ALL lines end to end (~18 min): dispenses each reagent
  // through every sample line into the plate, aspirating between rounds.
  // The IMAGE rounds intentionally pull air (empty port 4) — not a fault.
  // This is also THE FLUSH CYCLE: run it with fresh reservoirs to change the
  // working fluid (ShutdownScript reuses it with water).
  // AFTER THIS STEP: empty the test plate before Step 5.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 4 FULL RINSE", (unsigned long)(61 * mLPumpTime + 2 * fillTime)); fullRinse(WellLength, SampleWells, mLPumpTime);

  // --------------------------------------------------------------------------
  // STEP 5 — dispensation volume calibration. The ONLY value edited is
  // mLPumpTime in constants (s per mL).
  // PASS: average dispensation = 1 mL (aim a hair over — the target is just
  // slightly more than 1 mL) and max well-to-well delta < 50 uL.
  // Empty the plate, edit+save, re-flash, repeat. A fresh or rested pump line
  // DRIFTS until it softens — trust a value only after two consecutive passes
  // at the same setting agree. Weighing the plate beats eyeballing.
  // (OttoPanel runs this same step as a guided on-screen wizard.)
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 5 DISPENSE CALIBRATION", (unsigned long)(WellLength * mLPumpTime)); DispenseLines(WellLength, SampleWells, WASH, mLPumpTime);

  // --------------------------------------------------------------------------
  // STEP 6 — dispense then aspirate the whole plate.
  // PASS: every well pulled to <= 50 uL (ideally <= 20 uL) remaining.
  // vacTime in constants = seconds of vacuum per well (7-15 s is sensible;
  // if 15 s still leaves liquid the problem is suction or needle height,
  // not time).
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 6 DISPENSE + ASPIRATE", (unsigned long)(WellLength * (mLPumpTime + vacTime) + fillTime)); DispenseLines(WellLength, SampleWells, WASH, mLPumpTime); AspirateLines(WellLength, VacuumWells, vacTime, SafeVacA, SafeVacB, fillTime);

  // --------------------------------------------------------------------------
  // STEP 7 — nested aspirate/dispense cycle + incubation wrapper: the motion
  // profile of a real run (aspirates well N while dispensing well N-1).
  // Run first on the unseeded plate for general vacuum performance, then
  // REPEAT ON A CELL-SEEDED PLATE (cells change the glass surface):
  // <= 20 uL/well remaining after aspirations, and ~1 mL left per well at the
  // end — this step finishes wet; that is the incubation state.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 7 NESTED CYCLE", (unsigned long)(WellLength * 2 * mLPumpTime + fillTime)); AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 1);

  // --------------------------------------------------------------------------
  // COMPUTED CONSTANTS (no machine run — not a numbered step). Edit+save in
  // constants between Steps 7 and 8:
  //   - SampleLineVolume   = sample line length x volume-per-inch for the
  //     tubing ID (0.02" ID = ~5.2 uL/inch).
  //   - SampleNeedleVolume from the needle gauge chart:
  //     https://www.hamiltoncompany.com/knowledge-base/article/needle-gauge-chart
  //     (0.0427 mL = 1.5 inches of 16 gauge).
  // --------------------------------------------------------------------------

  // --------------------------------------------------------------------------
  // STEP 8 — addReagent (incorporation) tuning. Re-run repeatedly, tuning in
  // this order:
  //   (a) edit+save ReagentLineVolume until the LEADING edge of the air
  //       bubble disappears into the sample valve BEFORE being split into
  //       the sample lines;
  //   (b) then edit+save adjustSampleVolMicro until the LAGGING edge
  //       disappears into the sample needles at the end of the function.
  // PASS: ~SBSVolume dispensed to each well after the operation.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 8 ADDREAGENT INC TUNING", 0); AddSBSReagent(WellLength, SampleWells, VacuumWells, INCORPORATION, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);

  // --------------------------------------------------------------------------
  // STEP 9 — addReagent (cleavage) validation: repeat with cleavage to
  // validate the values tuned in Step 8. Same PASS criteria as Step 8.
  // --------------------------------------------------------------------------
  //ottoStepBegin("STEP 9 ADDREAGENT CLV VALIDATE", 0); AddSBSReagent(WellLength, SampleWells, VacuumWells, CLEAVAGE, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);

  stopLoop(); stopLoop();

}
