// ============================================================================
// Otto3 — OttoPanel: standalone touchscreen control (no computer attached)
// ============================================================================
// Phase 1 of no-computer operation. GIGA R1 + GIGA Display Shield (GT911
// capacitive touch, polled). The panel loops forever:
//
//   TOP MENU - three big categories:
//     CALIBRATION - steps 1-9 (same labels, function calls and
//                   expected-duration formulas as PreRunCalibrationScript):
//                   1 PRIME WASH, 2 PRIME CLEAVAGE, 3 PRIME INCORPORATION,
//                   4 FULL RINSE, 5 DISPENSE CALIBRATION (guided wizard),
//                   6 DISPENSE + ASPIRATE, 7 NESTED CYCLE,
//                   8 ADDREAGENT INC TUNING, 9 ADDREAGENT CLV VALIDATE
//     RUN         - FULL RUN (RunProtocol.ino)
//     UTILITIES   - one-off bench actions reusing existing functions only:
//                   2-minute line primes, FULL RINSE, DISPENSE 1 mL ALL
//                   WELLS, ASPIRATE ALL WELLS, SHUTDOWN FLUSH (water
//                   reservoirs, per ShutdownScript), PARK VALVES
//   Each submenu has a BACK button (top-left cell).
//
//   CONFIRM  - that action's pre-run checklist as tappable checkboxes;
//              GO stays disabled until every box is checked, so nothing
//              wet ever starts from a single accidental tap
//   RUNNING  - the live 4-panel dashboard, with a full-height red STOP
//              column on the right. One press aborts IMMEDIATELY (no
//              confirmation): the in-flight routine unwinds in
//              milliseconds, then the instrument parks (pump off, vacuum
//              closed, reagent->4, sample->1, vacuum->1)
//   RESULT   - DONE + measured duration, or an unmistakable red ABORTED
//              screen; touch anywhere returns to the submenu. Steps with a
//              follow-up operator action (empty the plate after Step 4,
//              seeded-plate repeat after the first Step 7 pass) show it
//              full-screen first
//   STEP 5   - guided dispense-calibration wizard: run, enter the measured
//              average uL/well on a keypad, answer the <50 uL delta
//              question, and the panel either declares a pass (showing the
//              mLPumpTime value to save to constants.ino) or retunes
//              mLPumpTime in RAM and loops - no reflash between iterations
//
// Calibrated values adjusted by the wizard live in RAM only: copy the
// SAVE screen's number into OttoFns/constants.ino afterwards. (Persisting
// them in the GIGA's QSPI KVStore was considered and deliberately skipped
// for now: it cannot be bench-verified without hardware and a mis-configured
// store could stall boot on the live instrument.)
//
// Power sequencing still applies (see README): GIGA powered BEFORE pump
// power on; pump power off FIRST at shutdown; pump front-panel STOP pressed
// after any keypad use or the remote contact does nothing.

#define OTTO_DISPLAY_ENABLED     // dashboard on the GIGA Display Shield
#define OTTO_DISPLAY_STOP_ZONE   // reserve the right column for the STOP button
#define OTTO_PANEL_ENABLED       // touch + abort hooks in LowLevelFns

#include "src/constants.h"
#include "src/LowLevelFns.h"     // setup() lives here (display, touch, valves)
#include "src/OttoFns.h"
#include "src/RunProtocol.h"     // runAutomation() — shared with RunOtto3
#include "src/OttoPanelUI.h"     // menu tree / confirm / wizard / STOP machine

void loop() {
  ottoPanelLoop();               // never parks the CPU; always returns to menu
}
