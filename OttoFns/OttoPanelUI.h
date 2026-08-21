// OttoPanelUI.h — standalone touchscreen control for Otto3 (OttoPanel sketch).
//
// GIGA R1 + GIGA Display Shield (GT911 capacitive touch, polled — no
// interrupts). Header-only, C-style, no dynamic allocation. Include from the
// OttoPanel sketch AFTER constants / LowLevelFns / OttoFns / RunProtocol, with
// OTTO_DISPLAY_ENABLED, OTTO_DISPLAY_STOP_ZONE and OTTO_PANEL_ENABLED defined.
//
// What lives here:
//   - GT911 polling + press-edge detection + coordinate mapping to the
//     landscape (setRotation(1), 800x480) frame the dashboard draws in
//   - ottoTouchBegin()/ottoTouchPoll(): the two functions LowLevelFns.ino
//     forward-declares under OTTO_PANEL_ENABLED. ottoTouchPoll() runs from
//     Wait()'s 100 ms slices during an action; while a run is live it treats
//     any press in the right STOP column as an emergency stop and latches
//     ottoAbortFlag (defined in LowLevelFns.ino)
//   - ottoPanelPark(): post-abort park (pump off, vacuum closed, reagent 4,
//     sample 1, vacuum 1) — also offered as the PARK VALVES utility
//   - ottoPanelLoop(): the whole UI state machine —
//       TOP (CALIBRATION / RUN / UTILITIES)
//        -> submenu (BACK + the category's actions)
//        -> CONFIRM (checklist gates GO) -> run (dashboard + STOP)
//        -> RESULT (DONE/ABORTED) -> optional POST instruction -> submenu
//     plus the STEP 5 (dispense calibration) guided wizard:
//       run -> keypad "avg uL/well" -> "delta < 50 uL?" -> SUCCESS (save
//       value) or ADJUST (retune mLPumpTime in RAM, empty plate, loop)
//
// Menu tree:
//   CALIBRATION - steps 1..9 (same calls/formulas as PreRunCalibrationScript):
//     1 PRIME WASH, 2 PRIME CLEAVAGE, 3 PRIME INCORPORATION, 4 FULL RINSE,
//     5 DISPENSE CALIBRATION (wizard), 6 DISPENSE + ASPIRATE, 7 NESTED CYCLE,
//     8 ADDREAGENT INC TUNING, 9 ADDREAGENT CLV VALIDATE
//   RUN         - FULL RUN (RunProtocol.ino)
//   UTILITIES   - one-off bench actions reusing existing functions only:
//     2-minute line primes (wash/cleavage/incorporation), FULL RINSE,
//     DISPENSE 1 mL ALL WELLS, ASPIRATE ALL WELLS, SHUTDOWN FLUSH (water,
//     per ShutdownScript), PARK VALVES, and the static bubble-inspection
//     cals (CAL: RESET LINE / BUBBLE TO VALVE / BUBBLE TO NEEDLE 1 -
//     calibrateReagentRuntime / testSampleRuntime park the marker bubble
//     at a landmark and stop, for still-line inspection)
//
// STOP semantics (the safety core): while an action runs, one press on the
// red STOP column aborts with NO confirmation. The latch makes Wait() return
// immediately and turns StartPump/RunPump/Select*Port/OpenVacuumLine into
// no-ops (guards at the LowLevelFns seam), so the in-flight routine unwinds
// in milliseconds; then ottoPanelPark() puts the hardware in a known state
// and the ABORTED screen takes over. A run that is never aborted executes
// exactly the same calls as the serial-driven sketches.

#ifndef OTTO_PANEL_UI_H
#define OTTO_PANEL_UI_H

#ifndef OTTO_DISPLAY_H
#error "OttoPanelUI.h needs OttoDisplay.h first (define OTTO_DISPLAY_ENABLED)"
#endif
#ifndef OTTO_DISPLAY_STOP_ZONE
#error "OttoPanelUI.h needs OTTO_DISPLAY_STOP_ZONE (STOP button geometry)"
#endif

#include <Arduino_GigaDisplayTouch.h>

extern volatile bool ottoAbortFlag;   // defined in LowLevelFns.ino

static void ottoPanelPark();          // defined below; also a UTILITIES action

// ------------------------------------------------------------- UI colors ---
#define OTTO_UI_BTN_FILL   0x18E3    // dark grey button
#define OTTO_UI_BTN_NAVY   0x0210    // FULL RUN accent
#define OTTO_UI_GO_OFF_TXT OTTO_COL_GREY

// --------------------------------------------------- top-menu geometry -----
// Three full-width category buttons under a 48 px title bar.
#define OTTO_UI_TOP_X       8
#define OTTO_UI_TOP_W     784
#define OTTO_UI_TOP_H     124
#define OTTO_UI_TOP_Y0     56
#define OTTO_UI_TOP_STEP  140

// ---------------------------------------------------- submenu geometry -----
// 2 cols x 5 rows of 384x78 buttons under a 48 px title bar (all >= 78 px
// tall, ~13 mm on the 4" panel). Cell 0 is always BACK.
#define OTTO_UI_MENU_TOP   48
#define OTTO_UI_ROW_H      86
#define OTTO_UI_BTN_W     384
#define OTTO_UI_BTN_H      78
#define OTTO_UI_COL0_X      8
#define OTTO_UI_COL1_X    408

// -------------------------------------- confirm / checklist geometry -------
// Checklist rows (whole row is the touch target) + CANCEL / GO. GO is
// disabled (grey) until every box is checked — nothing wet from one tap.
#define OTTO_CK_MAX         5
#define OTTO_UI_CK_TOP     52
#define OTTO_UI_CK_ROW_H   60
#define OTTO_UI_CK_BOX     36
#define OTTO_UI_CF_BTN_Y  384
#define OTTO_UI_CF_BTN_H   88
#define OTTO_UI_CF_CAN_X   20
#define OTTO_UI_CF_GO_X   420
#define OTTO_UI_CF_BTN_W  360

// ------------------------------------------------- wizard keypad geometry --
// 3 x 4 keypad (1..9 / DEL 0 OK), 122x98 keys, right half of the screen.
#define OTTO_UI_KP_X0     410
#define OTTO_UI_KP_Y0      44
#define OTTO_UI_KP_W      122
#define OTTO_UI_KP_H       98
#define OTTO_UI_KP_XSTEP  130
#define OTTO_UI_KP_YSTEP  106
#define OTTO_UI_KP_ENTRY_MAX 4      // digits

// STEP 5 pass window (uL/well): "average = 1 mL, aim a hair over".
#define OTTO_WIZ_TARGET_LO 1000.0f
#define OTTO_WIZ_TARGET_HI 1080.0f
// Sanity window for keypad entry (uL) — OK is ignored outside it.
#define OTTO_WIZ_ENTRY_LO   200
#define OTTO_WIZ_ENTRY_HI  3000

// ------------------------------------------------------------ UI states ----
#define OTTO_ST_TOP         0   // CALIBRATION / RUN / UTILITIES
#define OTTO_ST_MENU        1   // submenu of the selected category
#define OTTO_ST_CONFIRM     2
#define OTTO_ST_RESULT      3   // DONE or ABORTED
#define OTTO_ST_POST        4   // post-step operator instruction
#define OTTO_ST_WIZ_VOL     5   // keypad: avg uL per well
#define OTTO_ST_WIZ_DELTA   6   // yes/no: delta < 50 uL
#define OTTO_ST_WIZ_OUTCOME 7   // SUCCESS or ADJUST screen

// Post-instruction ids.
#define OTTO_POST_NONE        0
#define OTTO_POST_EMPTY_PLATE 1
#define OTTO_POST_SEEDED      2

// --------------------------------------------------------------- actions ---
// Calibration labels, expected-duration formulas and function calls are
// EXACTLY the ones in PreRunCalibrationScript.ino (steps 1..9); FULL RUN is
// RunProtocol.ino; UTILITIES reuse existing OttoFns functions only.
#define OTTO_ACT_STEP4RINSE  3   // STEP 4 FULL RINSE (empty-plate post screen)
#define OTTO_ACT_DISPCAL     4   // STEP 5 DISPENSE CALIBRATION (the wizard)
#define OTTO_ACT_NESTED      6   // STEP 7 NESTED CYCLE (seeded-repeat post)
#define OTTO_ACT_FULLRUN     9

static const char* const OTTO_UI_LABEL[] = {
  // CALIBRATION (indices 0..8 = steps 1..9)
  "STEP 1 PRIME WASH",
  "STEP 2 PRIME CLEAVAGE",
  "STEP 3 PRIME INCORPORATION",
  "STEP 4 FULL RINSE",
  "STEP 5 DISPENSE CALIBRATION",
  "STEP 6 DISPENSE + ASPIRATE",
  "STEP 7 NESTED CYCLE",
  "STEP 8 ADDREAGENT INC TUNING",
  "STEP 9 ADDREAGENT CLV VALIDATE",
  // RUN (index 9)
  "FULL RUN",
  // UTILITIES (indices 10..17)
  "PRIME WASH (2 MIN)",
  "PRIME CLEAVAGE (2 MIN)",
  "PRIME INCORPORATION (2 MIN)",
  "FULL RINSE",
  "DISPENSE 1 mL ALL WELLS",
  "ASPIRATE ALL WELLS",
  "SHUTDOWN FLUSH (WATER)",
  "PARK VALVES",
  // Static bubble-inspection helpers (OttoFns.ino calibrate/test fns): draw
  // the marker bubble, then STOP with it parked at the landmark so the
  // operator inspects a still line (the original intended tuning method).
  "CAL: RESET LINE (WASH PRIME)",
  "CAL: BUBBLE TO VALVE",
  "CAL: BUBBLE TO NEEDLE 1",
  // Phase-isolated AddSBSReagentMulti verification (WASH only, no vacuum):
  // SPLIT TEST reproduces the end-of-split state and stops; PURGE TEST runs
  // the ascending purge loop on that state. Helpers below, production code
  // in OttoFns.ino untouched.
  "CAL: SPLIT TEST",
  "CAL: PURGE TEST",
};
#define OTTO_UI_NACT 23

// Menu tree: category -> action indices. Submenu cell 0 is BACK, so a
// category holds at most 9 actions (2x5 grid).
static const char* const OTTO_CAT_NAME[3] = {"CALIBRATION", "RUN", "UTILITIES"};
static const uint8_t OTTO_CAT_CAL_A[]  = {0, 1, 2, 3, 4, 5, 6, 7, 8};
static const uint8_t OTTO_CAT_RUN_A[]  = {9};
static const uint8_t OTTO_CAT_UTIL_A[] = {10, 11, 12, 13, 14, 15, 16, 17,
                                          18, 19, 20, 21, 22};

static const uint8_t* ottoCatActs(uint8_t cat, uint8_t& n) {
  switch (cat) {
    case 0:  n = sizeof(OTTO_CAT_CAL_A);  return OTTO_CAT_CAL_A;
    case 1:  n = sizeof(OTTO_CAT_RUN_A);  return OTTO_CAT_RUN_A;
    default: n = sizeof(OTTO_CAT_UTIL_A); return OTTO_CAT_UTIL_A;
  }
}

// Expected duration (s) — calibration formulas are the calibration script's
// call-site formulas; 0 = no estimate (footer shows elapsed only). Computed
// at confirm time so a wizard-adjusted mLPumpTime is reflected immediately.
static unsigned long ottoActExpectedS(uint8_t i) {
  switch (i) {
    case 0: case 1: case 2:                        // steps 1-3: 20 s primes
      return 20;
    case 3: case 13: case 16:                      // fullRinse (all flavors)
      return (unsigned long)(61 * mLPumpTime + 2 * fillTime);
    case 4: case 14:                               // DispenseLines, 1 mL/well
      return (unsigned long)(WellLength * mLPumpTime);
    case 5: return (unsigned long)(WellLength * (mLPumpTime + vacTime) + fillTime);
    case 6: return (unsigned long)(WellLength * 2 * mLPumpTime + fillTime);
    case 10: case 11: case 12:                     // utility 2-minute primes
      return 120;
    case 15:                                       // AspirateLines
      return (unsigned long)(WellLength * vacTime + fillTime);
    case 17: return 5;                             // park: 3 valve moves
    case 18:                                       // wash prime out the vent
      return (unsigned long)(airTime * 5);
    case 19: case 20: {                            // bubble-to-landmark cals
      // air draw + wash chase to the sample valve (calibrateReagentRuntime)
      float chase = ReagentLineVolume * mLPumpTime - airTime;
      if (chase < 0) chase = 0;
      float s = airTime + chase;
      if (i == 20) {                               // + pause + sample leg
        s += 3 + SampleLineTotalVolume * mLPumpTime;
      }
      return (unsigned long)s;
    }
    case 21: {                                     // split test: full production
      // prefix of AddSBSReagentMulti through the split loop (~3 min):
      // fill wait + vent prime + sample primes + first-well vac + the six
      // well dispensations (totalPumpTime includes the air draw) + the
      // interleaved addlVacTime holds + settle + split loop (= airTime)
      float v    = ReagentLineVolume * mLPumpTime;
      float sbsw = (SBSVolume - SampleLineTotalVolume) * mLPumpTime;
      float sp   = SampleLineTotalVolume * mLPumpTime;
      float fill = fillTime - v;    if (fill < 0) fill = 0;
      float av   = vacTime - sbsw;  if (av < 0)   av = 0;
      return (unsigned long)(fill + 2 * v + WellLength * sp * 1.2f +
                             vacTime + WellLength * sbsw +
                             (WellLength - 1) * av + 5 + airTime);
    }
    case 22:                                       // purge test: 6 purges + vent
      return (unsigned long)(WellLength * (SampleLineTotalVolume * mLPumpTime) +
                             ReagentLineVolume * mLPumpTime);
    default: return 0;   // steps 8, 9 and the full run have no formula
  }
}

// ---- phase-isolated AddSBSReagentMulti verification helpers ----------------
// Panel-only, WASH-only, no vacuum; the production function in OttoFns.ino
// is untouched. Runtimes come from the same constants expressions production
// uses (ventRuntime = ReagentLineVolume*mLPumpTime, SamplePrimeRuntime =
// SampleLineTotalVolume*mLPumpTime), evaluated when the test runs.

// A) Reproduce the state at the END of the bubble-split phase with FULL
// production fidelity, then stop for inspection instead of purging.
//
// verbatim prefix of AddSBSReagentMulti - keep in step if that function
// changes. Runs the real thing: vacuum choreography, INCORPORATION reagent
// priming of vent + all sample lines, the six ascending well dispensations
// with the interleaved air draw (production cues/countdowns included via
// the copied lines), bubbleIdx spanning logic, final-well handling, the
// settle, and the 7->2 descending split loop - then parks (no purge).
// Runtimes come from the same expressions as AddSBSReagent's dispatcher,
// so bubbleIdx lands exactly where production puts it.
static void ottoCalSplitTest() {
  // --- dispatcher expressions (AddSBSReagent), INCORPORATION as reagent ---
  Reagent reagentName = INCORPORATION;
  float ventRuntime = getPumpRuntime(ReagentLineVolume, mLPumpTime);
  float SBSWellRuntime = getPumpRuntime(SBSVolume - SampleLineTotalVolume, mLPumpTime);
  float SamplePrimeRuntime = getPumpRuntime(SampleLineTotalVolume, mLPumpTime);
  float totalPumpTime = SBSWellRuntime * WellLength;
  float pumpTimeBeforeBubble = totalPumpTime - ventRuntime;
  (void)totalPumpTime;
  // --- production parameter names used below ---
  float AirTime = airTime, FillTime = fillTime, VacTime = vacTime;
  int VacStartPort = SafeVacA, VacEndPort = SafeVacB;

  ottoCue("PRIMING - NO NEED TO WATCH YET", OTTO_CUE_CALM); // wrapper cue

  // ======================= verbatim prefix begins ==========================
  float BubbleTimePerSampleLine = AirTime/WellLength; // dividing up the bubble between the sample lines
  int bubbleIdx = (int)floor(max(0,pumpTimeBeforeBubble)/SBSWellRuntime);
  float AirTimeBubbleIdx = AirTime;
  float pumpTimeBeforeBubbleIdx = ((pumpTimeBeforeBubble/SBSWellRuntime) - bubbleIdx) * SBSWellRuntime; // remainder of pump time
  float pumpTimeAfterBubbleIdx = SBSWellRuntime - AirTime - pumpTimeBeforeBubbleIdx;

  float bubbleTimeNextIdx;
  float pumpTimeNextIdx;

  float addlVacTime = max(VacTime-SBSWellRuntime,0); //we are pumping our initial reagent prime into existing fluid so need to vac for the whole time

  if (pumpTimeAfterBubbleIdx <= 0) { //we run the bubble spanned across 2 idx's
    bubbleTimeNextIdx = pumpTimeAfterBubbleIdx * -1;
    pumpTimeNextIdx = SBSWellRuntime - bubbleTimeNextIdx;
    AirTimeBubbleIdx = AirTime - bubbleTimeNextIdx;
  }

  // set up vacuum line
  SelectVacuumPort(VacStartPort);
  OpenVacuumLine();
  Wait(max(0, FillTime-ventRuntime)); // wait for any necessary fill

  // we want to purge the old stuck reagent all the way through the line
  RunPumpLine(reagentName, VentPort, ventRuntime*2); //prime reagent line with > ventRuntime to ensure completion

  for (int w = 0; w<WellLength; w++) {
    RunPumpLine(reagentName, SampleWells[w], SamplePrimeRuntime * 1.2); //prime sample lines with a little extra to make sure
  }

  SelectSamplePort(SampleWells[0]); //select first well
  SelectVacuumPort(VacuumWells[0]); //vacuum first well
  Wait(VacTime);

  for (int i = 0; i<bubbleIdx; i++) {
    SelectVacuumPort(VacuumWells[i + 1]); //vacuum next well
    RunPumpLine(reagentName, SampleWells[i], SBSWellRuntime); //dispense to current well
    Wait(addlVacTime); //finish vacuuming
  }

  if (bubbleIdx + 1 <= WellLength-1) {SelectVacuumPort(VacuumWells[bubbleIdx + 1]);} //vacuum next well
  else {SelectVacuumPort(VacEndPort);}

  ottoCue("AIR BUBBLE ENTERING LINE NEXT", OTTO_CUE_CALM); // cue: air draw starts at the end of this segment

  RunPumpLine(reagentName, SampleWells[bubbleIdx], pumpTimeBeforeBubbleIdx);

  ottoCueArm("EYES ON DISP VALVE - FRONT ARRIVES",
             (unsigned long)(ventRuntime * 1000.0f));

  RunPumpLine(AIR, SampleWells[bubbleIdx], AirTimeBubbleIdx);// + pumpTimeBeforeBubble); //dispense bubble to bubble well

  // if the bubble concludes vs extends over multiple wells
  int continueIdx;
  if (pumpTimeAfterBubbleIdx > 0) {
    continueIdx = 0;
      RunPumpLine(WASH, SampleWells[bubbleIdx], pumpTimeAfterBubbleIdx); //dispense post-bubble wash to bubble well
      Wait(addlVacTime); //finish vacuuming bubble well
    }
  else {
    continueIdx = 1;
    Wait(addlVacTime); //finish vacuuming bubble well
    if (bubbleIdx + 2 <= WellLength-1) {SelectVacuumPort(VacuumWells[bubbleIdx + 2]);} //vacuum next well
    else {SelectVacuumPort(VacEndPort);}
    RunPumpLine(AIR, SampleWells[bubbleIdx + 1], bubbleTimeNextIdx); // dispense remaining bubble
    RunPumpLine(WASH, SampleWells[bubbleIdx + 1], pumpTimeNextIdx); // dispense rest of wash well
    if (bubbleIdx + 2 <= WellLength-1) {Wait(addlVacTime);} //finish vacuuming after bubble well
  }

  // wells after bubble well excepting last well
  for (int ii = bubbleIdx + 1 + continueIdx; ii<WellLength-1; ii++) {
    Serial.print("simple wells after complex well ");Serial.println(ii);
    SelectVacuumPort(VacuumWells[ii + 1]); //vacuum next well
    RunPumpLine(WASH, SampleWells[ii], SBSWellRuntime); //dispense to current well
    Wait(addlVacTime); //finish vacuuming
  }

  SelectVacuumPort(VacEndPort);
  CloseVacuumLine();
  if ((bubbleIdx + 1) < WellLength-1) {
    RunPumpLine(WASH, SampleWells[WellLength-1], SBSWellRuntime);} //dispense to final well with wash

  Wait(5); // let bubble settle

  for (int w=WellLength-1; w>=0; w--) {
    RunPumpLine(WASH, SampleWells[w], BubbleTimePerSampleLine); //split bubble evenly between sample lines
  }
  // ======================== verbatim prefix ends ===========================

  // STOP instead of purging: park for still-line inspection.
  StopPump();
  CloseVacuumLine();
  SelectSamplePort(1);   // park (port 1 unplumbed)
  SelectVacuumPort(1);   // park
}

// B) The VERBATIM ascending purge loop only (run right after the split
// test): per-needle countdown + strobe like production, then vent washout.
static void ottoCalPurgeTest() {
  float ventRuntime        = ReagentLineVolume * mLPumpTime;
  float SamplePrimeRuntime = SampleLineTotalVolume * mLPumpTime;
  static char cueBuf[36];
  for (int ww = 0; ww < WellLength; ww++) {
    snprintf(cueBuf, sizeof(cueBuf), "RED AT NEEDLE %d", ww + 1);
    ottoCueArm(cueBuf, (unsigned long)(SamplePrimeRuntime * 1000.0f));
    RunPumpLine(WASH, SampleWells[ww], SamplePrimeRuntime);
  }
  RunPumpLine(WASH, VentPort, ventRuntime);  // wash out vent line
  SelectSamplePort(SampleWells[0]);          // park, as production's tail does
}

// The same function calls as the calibration script / run protocol; the
// utilities reuse those functions with bench-friendly durations.
static void ottoActRun(uint8_t i) {
  switch (i) {
    case 0: RunPumpLine(WASH, 8, 20); break;
    case 1: RunPumpLine(CLEAVAGE, 8, 20); break;
    case 2: RunPumpLine(INCORPORATION, 8, 20); break;
    case 3: case 13: case 16:
            fullRinse(WellLength, SampleWells, mLPumpTime); break;
    case 4: case 14:
            DispenseLines(WellLength, SampleWells, WASH, mLPumpTime); break;
    case 5: DispenseLines(WellLength, SampleWells, WASH, mLPumpTime);
            AspirateLines(WellLength, VacuumWells, vacTime, SafeVacA, SafeVacB, fillTime);
            break;
    case 6: AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells,
              WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 1);
            break;
    case 7: AddSBSReagent(WellLength, SampleWells, VacuumWells, INCORPORATION,
              SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort,
              mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);
            break;
    case 8: AddSBSReagent(WellLength, SampleWells, VacuumWells, CLEAVAGE,
              SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort,
              mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);
            break;
    case 9:  runAutomation(); break;
    case 10: RunPumpLine(WASH, 8, 120); break;
    case 11: RunPumpLine(CLEAVAGE, 8, 120); break;
    case 12: RunPumpLine(INCORPORATION, 8, 120); break;
    case 15: AspirateLines(WellLength, VacuumWells, vacTime, SafeVacA, SafeVacB, fillTime);
             break;
    case 17: ottoPanelPark(); break;
    case 18: RunPumpLine(WASH, VentPort, airTime*5); break;  // flush old bubble
    case 19: calibrateReagentRuntime(ReagentLineVolume, mLPumpTime, airTime, VentPort);
             break;
    case 20: testSampleRuntime(SampleWells, SampleLineTotalVolume, mLPumpTime, airTime, VentPort);
             break;
    case 21: ottoCalSplitTest(); break;
    case 22: ottoCalPurgeTest(); break;
  }
}

// Header step label (ottoStepBegin): defaults to the menu label; the two
// bubble-inspection cals get the short bench names.
static const char* ottoActStepLabel(uint8_t i) {
  if (i == 19) return "CAL BUBBLE - VALVE";
  if (i == 20) return "CAL BUBBLE - NEEDLE";
  return OTTO_UI_LABEL[i];
}

// Optional DONE-screen inspection line (amber, under MEASURED): tells the
// operator exactly where to look while the bubble sits parked.
static const char* ottoActResultNote(uint8_t i) {
  if (i == 19) return "INSPECT: BUBBLE FRONT AT SAMPLE VALVE?";
  if (i == 20) return "INSPECT: BUBBLE TAIL AT NEEDLE 1 TIP?";
  if (i == 21) return "INSPECT: AIR SLUG AT SAME SPOT IN ALL 6 LINES?";
  if (i == 22) return "INSPECT: WASH EDGE AT EVERY NEEDLE HUB?";
  return nullptr;
}

// Optional second DONE-screen line (below the first note).
static const char* ottoActResultNote2(uint8_t i) {
  if (i == 21) return "WELLS ~1 mL EACH";
  return nullptr;
}

// Pre-run checklists, from the calibration script's SETUP banner + README
// operational rules (SHUTDOWN FLUSH: water reservoirs, per ShutdownScript).
// Short texts, drawn big; GO stays disabled until every row is checked.
#define OTTO_CK_RES   "RESERVOIRS FULL, LINES TO BOTTOM"
#define OTTO_CK_PUMP  "PUMP ARMED (STOP AFTER KEYPAD USE)"
#define OTTO_CK_VAC   "VACUUM OPEN, TRAP IN LINE, 12V ON"
#define OTTO_CK_VENT  "VENT LINE (PORT 8) TO WASTE"
#define OTTO_CK_MANI  "MANIFOLD SEATED ON PLATE"

static uint8_t ottoActChecklist(uint8_t i, const char** items) {
  switch (i) {
    case 0: case 1: case 2:                       // STEPS 1-3 — prime to vent
    case 10: case 11: case 12:                    // utility 2-minute primes
    case 18:                                      // CAL reset (wash prime)
      items[0] = OTTO_CK_RES;
      items[1] = OTTO_CK_VENT;
      items[2] = OTTO_CK_PUMP;
      return 3;
    case 19: case 20:                             // bubble-to-landmark cals
      items[0] = OTTO_CK_RES;                     // (no vacuum: solenoid untouched)
      items[1] = "PORT 6 (AIR) DRY - NO TUBING";
      items[2] = OTTO_CK_PUMP;
      return 3;
    case 21:                                      // split test: full production
      items[0] = "PR2 IN ALL REAGENT SLOTS";
      items[1] = OTTO_CK_MANI;
      items[2] = OTTO_CK_VAC;
      items[3] = "PORT 6 (AIR) DRY - NO TUBING";
      items[4] = OTTO_CK_PUMP;
      return 5;
    case 22:                                      // purge test (no vacuum)
      items[0] = "RUN IMMEDIATELY AFTER CAL: SPLIT TEST";
      items[1] = "WASH RESERVOIR LOADED";
      items[2] = OTTO_CK_PUMP;
      return 3;
    case 3: case 13:                              // FULL RINSE (step 4 / util)
      items[0] = OTTO_CK_RES;
      items[1] = "MANIFOLD ON EMPTY TEST PLATE";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 4:                                       // STEP 5 — dispense cal
      items[0] = "PLATE EMPTY";
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_MANI;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 5:                                       // STEP 6 — disp + asp
      items[0] = "PLATE EMPTY";
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 6:                                       // STEP 7 — nested cycle
      items[0] = "UNSEEDED FIRST, THEN SEEDED PLATE";
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 7: case 8:                               // STEPS 8/9 — addReagent
      items[0] = "SBS REAGENTS LOADED (INC + CLV)";
      items[1] = "PORT 6 (AIR) DRY - NO TUBING";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      items[4] = OTTO_CK_MANI;
      return 5;
    case 14:                                      // DISPENSE 1 mL ALL WELLS
      items[0] = OTTO_CK_MANI;
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_PUMP;
      return 3;
    case 15:                                      // ASPIRATE ALL WELLS
      items[0] = OTTO_CK_MANI;
      items[1] = OTTO_CK_VAC;
      return 2;
    case 16:                                      // SHUTDOWN FLUSH
      items[0] = "ALL RESERVOIRS SWAPPED TO WATER";
      items[1] = "MANIFOLD ON EMPTY TEST PLATE";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 17:                                      // PARK VALVES
      items[0] = "MOVES VALVES TO 4 / 1 / 1";
      return 1;
    default:                                      // FULL RUN
      items[0] = "REAGENTS LOADED, LINES TO BOTTOM";
      items[1] = "SEEDED PLATE + MANIFOLD SEATED";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
  }
}

// ----------------------------------------------------------- touch state ---
static Arduino_GigaDisplayTouch ottoTouch;   // GT911 on Wire1 (NOT the valve bus)
static bool          ottoTouchReady    = false;
static bool          ottoTouchWasDown  = false;
static int16_t       ottoTapX          = -1;  // queued press (screen coords)
static int16_t       ottoTapY          = -1;
static unsigned long ottoTouchPollMs   = 0;
static unsigned long ottoTouchMuteTil  = 0;   // swallow presses through transitions
static bool          ottoStopArmed     = false; // a run is live: STOP column hot

// ------------------------------------------------------------- UI state ----
static uint8_t       ottoUiState       = OTTO_ST_TOP;
static bool          ottoUiDrawn       = false;
static uint8_t       ottoUiCat         = 0;      // selected category
static uint8_t       ottoUiSel         = 0;      // selected action index
static bool          ottoUiCk[OTTO_CK_MAX];      // checklist ticks
static const char*   ottoUiCkItem[OTTO_CK_MAX];
static uint8_t       ottoUiCkN         = 0;
static bool          ottoUiAborted     = false;
static unsigned long ottoUiMeasuredS   = 0;      // DONE: measured duration
static unsigned long ottoUiAbortAtS    = 0;      // ABORTED: elapsed at stop
static uint8_t       ottoUiPost        = OTTO_POST_NONE;
static bool          ottoUiStep7Ran    = false;  // first nested-cycle pass done

// ---------------------------------------------------------- wizard state ---
static char          ottoWizEntry[OTTO_UI_KP_ENTRY_MAX + 1] = "";
static float         ottoWizMeasured   = 0;      // uL/well entered
static bool          ottoWizDeltaOk    = false;
static bool          ottoWizPassed     = false;  // outcome screen: SUCCESS?
static float         ottoWizPrev       = 0;      // mLPumpTime before adjust
static uint8_t       ottoWizStreak     = 0;      // consecutive in-window passes

// ------------------------------------------------------- touch functions ---

// GT911 reports native portrait coordinates (x 0..479, y 0..799). The
// dashboard runs setRotation(1); Arduino_GigaDisplay_GFX::drawPixel maps
// logical -> native as { x_n = 479 - y_l ; y_n = x_l }, so the inverse is:
//   screen_x = raw_y ;  screen_y = 479 - raw_x
// Hardware-verified 2026-08 on the instrument's shield.
static void ottoTouchMap(uint16_t rx, uint16_t ry, int16_t& sx, int16_t& sy) {
  sx = (int16_t)ry;
  sy = (int16_t)(479 - rx);
}

// Called from setup() in LowLevelFns.ino (forward-declared there).
void ottoTouchBegin() {
  ottoTouchReady = ottoTouch.begin();
  Serial.println(ottoTouchReady ? F("TOUCH INIT OK (GT911)")
                                : F("TOUCH INIT FAILED (GT911)"));
}

// ------------------------------------------------- remote serial interface -
// One firmware, two interfaces: line-based single-char commands over USB
// serial (9600) drive the SAME action pipeline as a touch launch, skipping
// the on-screen checklist/confirm (the remote operator confirms
// out-of-band). During a run only 'x' (abort) and 's' (status) act; other
// input answers BUSY. Polled from the same Wait-slice hook as touch.
static char    ottoSerLine[8];
static uint8_t ottoSerLen     = 0;
static char    ottoSerPending = 0;    // idle command awaiting dispatch

void ottoSerialHelp() {
  Serial.println(F("REMOTE COMMANDS (one per line, 9600 baud):"));
  Serial.println(F("  1..9  calibration steps 1-9     R  FULL RUN"));
  Serial.println(F("  w/c/i prime wash/clv/inc 2min   f  FULL RINSE"));
  Serial.println(F("  d DISPENSE ALL   a ASPIRATE ALL   k PARK VALVES"));
  Serial.println(F("  r CAL RESET LINE   v CAL BUBBLE TO VALVE"));
  Serial.println(F("  n CAL BUBBLE TO NEEDLE 1"));
  Serial.println(F("  t CAL SPLIT TEST   p CAL PURGE TEST"));
  Serial.println(F("  x STOP/ABORT (during run)   s STATUS   ? this help"));
}

// Command char -> action index (-1 = not an action command).
static int8_t ottoSerialAction(char c) {
  switch (c) {
    case 'v': return 19;  case 'n': return 20;
    case 't': return 21;  case 'p': return 22;
    case 'r': return 18;
    case 'w': return 10;  case 'c': return 11;  case 'i': return 12;
    case 'f': return 13;  case 'd': return 14;  case 'a': return 15;
    case 'k': return 17;  case 'R': return 9;
    default:
      if (c >= '1' && c <= '9') return (int8_t)(c - '1');  // steps 1..9
      return -1;
  }
}

static void ottoSerialStatus() {
  Serial.print(F("STATUS "));
  if (ottoStopArmed) {
    Serial.print(F("RUNNING "));
    Serial.print(ottoActStepLabel(ottoUiSel));
  } else if (ottoUiState == OTTO_ST_RESULT) {
    if (ottoUiAborted) Serial.print(F("ABORTED"));
    else               Serial.print(F("DONE"));
  } else if (ottoUiState >= OTTO_ST_WIZ_VOL) {
    Serial.print(F("WIZARD"));
  } else {
    Serial.print(F("IDLE"));
  }
  Serial.print(F(" elapsed=")); Serial.print(otto_elapsedS()); Serial.print('s');
  Serial.print(F(" mLPumpTime="));            Serial.print(mLPumpTime);
  Serial.print(F(" ReagentLineVolume="));     Serial.print(ReagentLineVolume);
  Serial.print(F(" SampleLineTotalVolume=")); Serial.print(SampleLineTotalVolume);
  Serial.print(F(" adjustSampleVolMicro=")); Serial.print(adjustSampleVolMicro);
  Serial.print(F(" vacTime="));               Serial.println(vacTime);
}

// A completed input line's first character.
static void ottoSerialCommand(char c) {
  if (c == 's') { ottoSerialStatus(); return; }
  if (c == '?') { ottoSerialHelp();   return; }
  if (c == 'x') {
    if (ottoStopArmed) {
      ottoAbortFlag = true;                 // same latch as the red button
      Serial.println(F("ABORT REQUESTED"));
    } else {
      Serial.println(F("NOT RUNNING"));
    }
    return;
  }
  if (ottoStopArmed) { Serial.println(F("BUSY (RUNNING)")); return; }
  if (ottoSerialAction(c) < 0) {
    Serial.println(F("UNKNOWN CMD - ? FOR HELP"));
    return;
  }
  ottoSerPending = c;   // dispatched by ottoPanelLoop in an idle state
}

// Byte pump: buffer until newline, act on the line's first character.
static void ottoSerialPoll() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (ottoSerLen > 0) {
        char c = ottoSerLine[0];
        ottoSerLen = 0;
        ottoSerialCommand(c);
      }
    } else if (ch != ' ' && ch != '\t') {
      if (ottoSerLen < sizeof(ottoSerLine) - 1) ottoSerLine[ottoSerLen++] = ch;
    }
  }
}

// Poll the GT911 (25 ms rate limit) and act on press edges only:
//   - run live (ottoStopArmed): a press at/right of the STOP column latches
//     ottoAbortFlag — no confirmation, that's the point;
//   - otherwise: queue the tap for the UI loop.
// Called from Wait()'s 100 ms slices (via LowLevelFns) and from the UI loop.
void ottoTouchPoll() {
  ottoSerialPoll();                 // remote serial rides the same hook
  if (!ottoTouchReady) return;
  unsigned long now = millis();
  if (now - ottoTouchPollMs < 25) return;
  ottoTouchPollMs = now;

  GDTpoint_t pts[GT911_MAX_CONTACTS];
  uint8_t n = ottoTouch.getTouchPoints(pts);
  bool down = (n > 0);

  if (down && !ottoTouchWasDown) {                 // press edge
    int16_t sx, sy;
    ottoTouchMap(pts[0].x, pts[0].y, sx, sy);
    Serial.print(F("TOUCH ")); Serial.print(sx);   // bring-up aid: verify the
    Serial.print(' '); Serial.println(sy);         // mapping against the UI
    if ((long)(now - ottoTouchMuteTil) >= 0) {
      if (ottoStopArmed) {
        // 20 px slop left of the drawn button edge; any height counts.
        if (sx >= OTTO_STOP_X - 20) {
          ottoAbortFlag = true;
          Serial.print(F("PANEL STOP PRESSED @ ")); Serial.print(sx);
          Serial.print(' '); Serial.println(sy);
        }
      } else {
        ottoTapX = sx;
        ottoTapY = sy;
      }
    }
  }
  ottoTouchWasDown = down;
}

// Consume the queued tap, if any.
static bool ottoTapConsume(int16_t& x, int16_t& y) {
  ottoTouchPoll();
  if (ottoTapX < 0) return false;
  x = ottoTapX; y = ottoTapY;
  ottoTapX = ottoTapY = -1;
  return true;
}

// Swallow presses for ms (screen transitions; lingering fingers).
static void ottoTapMute(unsigned long ms) {
  ottoTouchMuteTil = millis() + ms;
  ottoTapX = ottoTapY = -1;
}

static bool ottoUiIn(int16_t x, int16_t y,
                     int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

// ------------------------------------------------------------ park (abort) -
// Hardware to a known safe state: pump and vacuum first (instant GPIO), then
// the slow valve moves. The abort latch is cleared before the moves —
// Select*Port are no-ops while it is set. After an abort it runs once the
// aborted routine has fully unwound; it is also the PARK VALVES utility.
static void ottoPanelPark() {
  StopPump();
  CloseVacuumLine();
  ottoAbortFlag = false;
  SelectReagentPort(4);   // empty / fallback reservoir
  SelectSamplePort(1);    // port 1 = unplumbed park (required unconnected)
  SelectVacuumPort(1);    // port 1 = unplumbed park
  Serial.println(F("PANEL PARKED: pump off, vacuum closed, valves 4/1/1"));
}

// ------------------------------------------------------- drawing helpers ---
static void ottoUiButton(int16_t x, int16_t y, int16_t w, int16_t h,
                         const char* label, uint16_t fill, uint16_t txt,
                         uint16_t border, uint8_t maxSz) {
  ottoGfx.fillRoundRect(x, y, w, h, 10, fill);
  ottoGfx.drawRoundRect(x, y, w, h, 10, border);
  uint8_t sz = otto_fitSize(label, w - 24, maxSz);
  otto_centerIn(label, x, w, y + (h - 8 * sz) / 2, sz, txt);
}

static void ottoUiTitleBar(const char* right) {
  ottoGfx.fillRect(0, 0, OTTO_SCR_W, OTTO_UI_MENU_TOP, OTTO_COL_HDR);
  ottoGfx.setTextSize(3);
  ottoGfx.setTextColor(OTTO_COL_FG);
  ottoGfx.setCursor(16, 12);
  ottoGfx.print("OTTO3");
  otto_centerIn(right, 220, 560, 12, 3, OTTO_COL_LIGHT);
}

// ------------------------------------------------------------- top menu ----
static void ottoUiTopRect(uint8_t i, int16_t& x, int16_t& y) {
  x = OTTO_UI_TOP_X;
  y = OTTO_UI_TOP_Y0 + i * OTTO_UI_TOP_STEP;
}

static void ottoUiDrawTop() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  ottoUiTitleBar("SELECT CATEGORY");
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x, y;
    ottoUiTopRect(i, x, y);
    bool run = (i == 1);
    ottoUiButton(x, y, OTTO_UI_TOP_W, OTTO_UI_TOP_H, OTTO_CAT_NAME[i],
                 run ? OTTO_UI_BTN_NAVY : OTTO_UI_BTN_FILL, OTTO_COL_FG,
                 run ? OTTO_COL_BLUE : OTTO_COL_GREY, 6);
  }
}

static int8_t ottoUiTopHit(int16_t tx, int16_t ty) {
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x, y;
    ottoUiTopRect(i, x, y);
    if (ottoUiIn(tx, ty, x, y, OTTO_UI_TOP_W, OTTO_UI_TOP_H)) return (int8_t)i;
  }
  return -1;
}

// -------------------------------------------------------------- submenu ----
// Cell 0 = BACK; cells 1..n = the category's actions. The grid is 2 columns
// by however many rows the category needs (min 5, so small menus keep the
// classic 86 px rows; UTILITIES' 12 cells get 6 x 72 px rows).
static int16_t otto_uiRowH = OTTO_UI_ROW_H;
static int16_t otto_uiBtnH = OTTO_UI_BTN_H;

static void ottoUiGridFor(uint8_t nCells) {
  uint8_t rows = (nCells + 1) / 2;
  if (rows < 5) rows = 5;
  otto_uiRowH = (OTTO_SCR_H - OTTO_UI_MENU_TOP) / rows;
  otto_uiBtnH = otto_uiRowH - 8;
}

static void ottoUiMenuRect(uint8_t cell, int16_t& x, int16_t& y) {
  x = (cell % 2) ? OTTO_UI_COL1_X : OTTO_UI_COL0_X;
  y = OTTO_UI_MENU_TOP + (cell / 2) * otto_uiRowH +
      (otto_uiRowH - otto_uiBtnH) / 2;
}

static void ottoUiDrawMenu() {
  uint8_t n;
  const uint8_t* acts = ottoCatActs(ottoUiCat, n);
  ottoUiGridFor(n + 1);
  ottoGfx.fillScreen(OTTO_COL_BG);
  ottoUiTitleBar(OTTO_CAT_NAME[ottoUiCat]);

  int16_t x, y;
  ottoUiMenuRect(0, x, y);
  ottoUiButton(x, y, OTTO_UI_BTN_W, otto_uiBtnH, "< BACK",
               OTTO_UI_BTN_FILL, OTTO_COL_AMBER, OTTO_COL_AMBER, 3);

  for (uint8_t c = 0; c < n; c++) {
    uint8_t a = acts[c];
    ottoUiMenuRect(c + 1, x, y);
    bool full = (a == OTTO_ACT_FULLRUN);
    ottoUiButton(x, y, OTTO_UI_BTN_W, otto_uiBtnH, OTTO_UI_LABEL[a],
                 full ? OTTO_UI_BTN_NAVY : OTTO_UI_BTN_FILL, OTTO_COL_FG,
                 full ? OTTO_COL_BLUE : OTTO_COL_GREY, 3);
  }
}

// Returns: -2 = BACK, -1 = nothing, else action index.
static int8_t ottoUiMenuHit(int16_t tx, int16_t ty) {
  uint8_t n;
  const uint8_t* acts = ottoCatActs(ottoUiCat, n);
  ottoUiGridFor(n + 1);
  for (uint8_t cell = 0; cell <= n; cell++) {
    int16_t x, y;
    ottoUiMenuRect(cell, x, y);
    if (ottoUiIn(tx, ty, x, y, OTTO_UI_BTN_W, otto_uiBtnH)) {
      return (cell == 0) ? -2 : (int8_t)acts[cell - 1];
    }
  }
  return -1;
}

// --------------------------------------------------- confirm + checklist ---
static bool ottoUiCkAll() {
  for (uint8_t i = 0; i < ottoUiCkN; i++) {
    if (!ottoUiCk[i]) return false;
  }
  return true;
}

static void ottoUiDrawCkRow(uint8_t i) {
  int16_t y = OTTO_UI_CK_TOP + i * OTTO_UI_CK_ROW_H;
  ottoGfx.fillRect(0, y, OTTO_SCR_W, OTTO_UI_CK_ROW_H, OTTO_COL_BG);
  int16_t by = y + (OTTO_UI_CK_ROW_H - OTTO_UI_CK_BOX) / 2;
  // checkbox
  ottoGfx.drawRect(24, by, OTTO_UI_CK_BOX, OTTO_UI_CK_BOX, OTTO_COL_LIGHT);
  ottoGfx.drawRect(25, by + 1, OTTO_UI_CK_BOX - 2, OTTO_UI_CK_BOX - 2,
                   OTTO_COL_LIGHT);
  if (ottoUiCk[i]) {
    ottoGfx.fillRect(24 + 6, by + 6, OTTO_UI_CK_BOX - 12, OTTO_UI_CK_BOX - 12,
                     OTTO_COL_GREEN);
  }
  // text
  const int16_t txx = 24 + OTTO_UI_CK_BOX + 18;
  uint8_t sz = otto_fitSize(ottoUiCkItem[i], OTTO_SCR_W - txx - 12, 3);
  ottoGfx.setTextSize(sz);
  ottoGfx.setTextColor(ottoUiCk[i] ? OTTO_COL_FG : OTTO_COL_LIGHT);
  ottoGfx.setCursor(txx, y + (OTTO_UI_CK_ROW_H - 8 * sz) / 2);
  ottoGfx.print(ottoUiCkItem[i]);
}

static void ottoUiDrawGo() {
  bool en = ottoUiCkAll();
  ottoUiButton(OTTO_UI_CF_GO_X, OTTO_UI_CF_BTN_Y, OTTO_UI_CF_BTN_W,
               OTTO_UI_CF_BTN_H, "GO",
               en ? OTTO_COL_GREEN : OTTO_UI_BTN_FILL,
               en ? OTTO_COL_BG : OTTO_UI_GO_OFF_TXT,
               en ? OTTO_COL_GREEN : OTTO_COL_GREY, 6);
}

static void ottoUiDrawConfirm() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  otto_centerIn(OTTO_UI_LABEL[ottoUiSel], 0, OTTO_SCR_W, 6, 3, OTTO_COL_FG);
  otto_centerIn("CHECK EVERY BOX, THEN GO", 0, OTTO_SCR_W, 34, 2,
                OTTO_COL_LIGHT);

  for (uint8_t i = 0; i < ottoUiCkN; i++) ottoUiDrawCkRow(i);

  // expected-duration line, right above the buttons
  char buf[28];
  unsigned long exp_s = ottoActExpectedS(ottoUiSel);
  if (exp_s > 0) {
    char t[12];
    otto_fmtMMSS(exp_s, t, sizeof(t));
    snprintf(buf, sizeof(buf), "EXPECTED ~ %s", t);
  } else {
    snprintf(buf, sizeof(buf), "NO TIME ESTIMATE");
  }
  otto_centerIn(buf, 0, OTTO_SCR_W, OTTO_UI_CF_BTN_Y - 26, 2, OTTO_COL_LIGHT);

  ottoUiButton(OTTO_UI_CF_CAN_X, OTTO_UI_CF_BTN_Y, OTTO_UI_CF_BTN_W,
               OTTO_UI_CF_BTN_H, "CANCEL", OTTO_UI_BTN_FILL, OTTO_COL_FG,
               OTTO_COL_GREY, 5);
  ottoUiDrawGo();
}

// ------------------------------------------------------- execute an action -
static void ottoPanelExecute(uint8_t i) {
  Serial.print(F("RUN: ")); Serial.println(OTTO_UI_LABEL[i]);  // launch ack
  ottoAbortFlag    = false;
  ottoStopArmed    = true;    // STOP column is hot from this moment
  ottoTapMute(400);
  otto_needRebuild = true;    // leaving a UI screen: full dashboard rebuild
  ottoStepBegin(ottoActStepLabel(i), ottoActExpectedS(i));

  ottoActRun(i);              // blocking; Wait() slices poll the STOP column

  ottoStopArmed = false;
  if (ottoAbortFlag) {
    ottoUiAbortAtS = otto_elapsedS();
    ottoPanelPark();          // clears the latch itself, then parks
    ottoUiAborted = true;
    ottoUiState   = OTTO_ST_RESULT;
    Serial.println(F("ABORTED"));       // end marker for remote drivers
  } else {
    ottoStepEnd();            // freeze measured duration on the dashboard
    ottoUiMeasuredS = otto_stepFrozenS;
    ottoUiAborted   = false;
    if (i == OTTO_ACT_DISPCAL) {        // STEP 5: guided calibration wizard
      ottoWizEntry[0] = '\0';
      ottoUiState = OTTO_ST_WIZ_VOL;
    } else {
      ottoUiState = OTTO_ST_RESULT;
    }
    Serial.print(F("DONE "));           // end marker for remote drivers
    Serial.print(ottoUiMeasuredS); Serial.println('s');
  }
  ottoUiDrawn = false;
  ottoTapMute(700);           // stray presses through the transition
}

// --------------------------------------------------------- result screens --
static void ottoUiDrawResult() {
  if (ottoUiAborted) {
    ottoGfx.fillScreen(OTTO_COL_RED);
    otto_centerIn(OTTO_UI_LABEL[ottoUiSel], 0, OTTO_SCR_W, 30, 3, OTTO_COL_BG);
    ottoGfx.fillRect(0, 100, OTTO_SCR_W, 130, OTTO_COL_BG);   // contrast band
    otto_centerIn("ABORTED", 0, OTTO_SCR_W, 100 + (130 - 64) / 2, 8,
                  OTTO_COL_RED);
    char buf[24];
    char t[12];
    otto_fmtMMSS(ottoUiAbortAtS, t, sizeof(t));
    snprintf(buf, sizeof(buf), "STOPPED AT %s", t);
    otto_centerIn(buf, 0, OTTO_SCR_W, 260, 3, OTTO_COL_BG);
    otto_centerIn("PUMP OFF - VACUUM CLOSED - VALVES PARKED (4/1/1)", 0,
                  OTTO_SCR_W, 310, 2, OTTO_COL_BG);
    otto_centerIn("TOUCH SCREEN TO RETURN TO MENU", 0, OTTO_SCR_W, 440, 2,
                  OTTO_COL_BG);
  } else {
    ottoGfx.fillScreen(OTTO_COL_BG);
    otto_centerIn(OTTO_UI_LABEL[ottoUiSel], 0, OTTO_SCR_W, 40, 3,
                  OTTO_COL_LIGHT);
    otto_centerIn("DONE", 0, OTTO_SCR_W, 130, 10, OTTO_COL_GREEN);
    char buf[24];
    char t[12];
    otto_fmtMMSS(ottoUiMeasuredS, t, sizeof(t));
    snprintf(buf, sizeof(buf), "MEASURED %s", t);
    otto_centerIn(buf, 0, OTTO_SCR_W, 280, 4, OTTO_COL_FG);
    const char* note = ottoActResultNote(ottoUiSel);
    if (note) {
      uint8_t nsz = otto_fitSize(note, OTTO_SCR_W - 2 * OTTO_MARGIN, 3);
      otto_centerIn(note, 0, OTTO_SCR_W, 350, nsz, OTTO_COL_AMBER);
    }
    const char* note2 = ottoActResultNote2(ottoUiSel);
    if (note2) otto_centerIn(note2, 0, OTTO_SCR_W, 396, 2, OTTO_COL_AMBER);
    otto_centerIn("TOUCH SCREEN TO RETURN TO MENU", 0, OTTO_SCR_W, 440, 2,
                  OTTO_COL_GREY);
  }
}

// ----------------------------------------------- post-step instructions ----
static void ottoUiDrawPost() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  if (ottoUiPost == OTTO_POST_EMPTY_PLATE) {
    otto_centerIn("BEFORE THE NEXT STEP:", 0, OTTO_SCR_W, 70, 3,
                  OTTO_COL_LIGHT);
    otto_centerIn("EMPTY THE", 0, OTTO_SCR_W, 160, 6, OTTO_COL_AMBER);
    otto_centerIn("TEST PLATE", 0, OTTO_SCR_W, 240, 6, OTTO_COL_AMBER);
    otto_centerIn("STEP 5 CALIBRATES ON AN EMPTY PLATE", 0, OTTO_SCR_W, 330, 2,
                  OTTO_COL_LIGHT);
  } else {                                  // OTTO_POST_SEEDED
    otto_centerIn("NEXT:", 0, OTTO_SCR_W, 50, 3, OTTO_COL_LIGHT);
    otto_centerIn("REPEAT STEP 7 ON A", 0, OTTO_SCR_W, 120, 5, OTTO_COL_AMBER);
    otto_centerIn("CELL-SEEDED PLATE", 0, OTTO_SCR_W, 180, 5, OTTO_COL_AMBER);
    otto_centerIn("PASS: <= 20 uL PER WELL AFTER ASPIRATIONS", 0, OTTO_SCR_W,
                  280, 2, OTTO_COL_FG);
    otto_centerIn("ENDS WET: ~1 mL PER WELL = INCUBATION STATE", 0, OTTO_SCR_W,
                  320, 2, OTTO_COL_FG);
  }
  otto_centerIn("TOUCH SCREEN TO RETURN TO MENU", 0, OTTO_SCR_W, 440, 2,
                OTTO_COL_GREY);
}

// ------------------------------------------------- STEP 5 wizard screens ---
// Keypad keys: index 0..8 = digits 1..9, 9 = DEL, 10 = 0, 11 = OK.
static void ottoUiKpRect(uint8_t k, int16_t& x, int16_t& y) {
  x = OTTO_UI_KP_X0 + (k % 3) * OTTO_UI_KP_XSTEP;
  y = OTTO_UI_KP_Y0 + (k / 3) * OTTO_UI_KP_YSTEP;
}

static const char* ottoUiKpLabel(uint8_t k) {
  static const char* const lbl[12] =
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "DEL", "0", "OK"};
  return lbl[k];
}

// Entry value in uL, or -1 while invalid/incomplete.
static long ottoWizEntryVal() {
  if (!ottoWizEntry[0]) return -1;
  long v = atol(ottoWizEntry);
  if (v < OTTO_WIZ_ENTRY_LO || v > OTTO_WIZ_ENTRY_HI) return -1;
  return v;
}

// Entry readout (left half). Repainted on every keypress.
static void ottoUiDrawWizEntry() {
  ottoGfx.fillRect(20, 140, 370, 100, OTTO_COL_BG);
  ottoGfx.drawRect(20, 140, 370, 100, OTTO_COL_BORDER);
  char buf[12];
  snprintf(buf, sizeof(buf), "%s uL", ottoWizEntry[0] ? ottoWizEntry : "-");
  otto_centerIn(buf, 20, 370, 140 + (100 - 8 * 6) / 2, 6,
                ottoWizEntryVal() >= 0 ? OTTO_COL_GREEN : OTTO_COL_FG);
}

static void ottoUiDrawWizVol() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  otto_centerIn("STEP 5 CALIBRATION", 0, 410, 20, 3, OTTO_COL_LIGHT);
  otto_centerIn("AVG VOLUME PER WELL?", 0, 410, 60, 3, OTTO_COL_FG);
  otto_centerIn("(WEIGH THE PLATE, uL)", 0, 410, 96, 2, OTTO_COL_LIGHT);
  ottoUiDrawWizEntry();
  otto_centerIn("TARGET 1000 - 1080 uL", 0, 410, 260, 2, OTTO_COL_LIGHT);
  ottoUiButton(20, 380, 370, 80, "CANCEL WIZARD", OTTO_UI_BTN_FILL,
               OTTO_COL_FG, OTTO_COL_GREY, 3);
  for (uint8_t k = 0; k < 12; k++) {
    int16_t x, y;
    ottoUiKpRect(k, x, y);
    bool ok = (k == 11), del = (k == 9);
    ottoUiButton(x, y, OTTO_UI_KP_W, OTTO_UI_KP_H, ottoUiKpLabel(k),
                 ok ? OTTO_COL_GREEN : (del ? OTTO_COL_AMBER : OTTO_UI_BTN_FILL),
                 (ok || del) ? OTTO_COL_BG : OTTO_COL_FG,
                 OTTO_COL_GREY, ok || del ? 4 : 5);
  }
}

static void ottoUiDrawWizDelta() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  otto_centerIn("STEP 5 CALIBRATION", 0, OTTO_SCR_W, 30, 3, OTTO_COL_LIGHT);
  otto_centerIn("MAX WELL-TO-WELL", 0, OTTO_SCR_W, 90, 4, OTTO_COL_FG);
  otto_centerIn("DELTA UNDER 50 uL?", 0, OTTO_SCR_W, 135, 4, OTTO_COL_FG);
  ottoUiButton(60, 230, 300, 170, "YES", OTTO_COL_GREEN, OTTO_COL_BG,
               OTTO_COL_GREEN, 7);
  ottoUiButton(440, 230, 300, 170, "NO", OTTO_COL_AMBER, OTTO_COL_BG,
               OTTO_COL_AMBER, 7);
}

static void ottoUiDrawWizOutcome() {
  char buf[56];
  if (ottoWizPassed) {
    ottoGfx.fillScreen(OTTO_COL_BG);
    otto_centerIn("STEP 5 PASS", 0, OTTO_SCR_W, 30, 5, OTTO_COL_GREEN);
    snprintf(buf, sizeof(buf), "mLPumpTime = %.2f", (double)mLPumpTime);
    otto_centerIn(buf, 0, OTTO_SCR_W, 120, 6, OTTO_COL_FG);
    otto_centerIn("SAVE TO OttoFns/constants.ino", 0, OTTO_SCR_W, 210, 3,
                  OTTO_COL_AMBER);
    if (ottoWizStreak >= 2) {
      snprintf(buf, sizeof(buf), "PASS #%u - TWO CONSECUTIVE PASSES AGREE",
               (unsigned)ottoWizStreak);
      otto_centerIn(buf, 0, OTTO_SCR_W, 280, 2, OTTO_COL_GREEN);
    } else {
      otto_centerIn("PASS #1 - RE-RUN TO CONFIRM (FRESH LINE DRIFTS)", 0,
                    OTTO_SCR_W, 280, 2, OTTO_COL_AMBER);
    }
    otto_centerIn("VALUE IS IN RAM ONLY - LOST AT POWER-OFF", 0, OTTO_SCR_W,
                  320, 2, OTTO_COL_LIGHT);
    char t[12];
    otto_fmtMMSS(ottoUiMeasuredS, t, sizeof(t));
    snprintf(buf, sizeof(buf), "RUN TOOK %s", t);
    otto_centerIn(buf, 0, OTTO_SCR_W, 360, 2, OTTO_COL_LIGHT);
    otto_centerIn("TOUCH SCREEN TO RETURN TO MENU", 0, OTTO_SCR_W, 440, 2,
                  OTTO_COL_GREY);
  } else {
    ottoGfx.fillScreen(OTTO_COL_BG);
    if (ottoWizPrev != mLPumpTime) {
      otto_centerIn("ADJUSTED", 0, OTTO_SCR_W, 26, 6, OTTO_COL_AMBER);
      snprintf(buf, sizeof(buf), "mLPumpTime %.2f -> %.2f s/mL",
               (double)ottoWizPrev, (double)mLPumpTime);
    } else {
      otto_centerIn("DELTA FAIL", 0, OTTO_SCR_W, 26, 6, OTTO_COL_AMBER);
      snprintf(buf, sizeof(buf), "mLPumpTime stays %.2f s/mL",
               (double)mLPumpTime);
    }
    otto_centerIn(buf, 0, OTTO_SCR_W, 120, 3, OTTO_COL_FG);
    snprintf(buf, sizeof(buf), "MEASURED %ld uL (TARGET 1000-1080)",
             (long)ottoWizMeasured);
    otto_centerIn(buf, 0, OTTO_SCR_W, 165, 2, OTTO_COL_LIGHT);
    if (ottoWizPrev == mLPumpTime) {
      otto_centerIn("CHECK MANIFOLD SEATING AND LINES", 0, OTTO_SCR_W, 200, 2,
                    OTTO_COL_LIGHT);
    } else {
      otto_centerIn("FRESH PUMP LINE DRIFTS - PASSES MUST AGREE TWICE", 0,
                    OTTO_SCR_W, 200, 2, OTTO_COL_LIGHT);
    }
    otto_centerIn("EMPTY THE PLATE", 0, OTTO_SCR_W, 270, 6, OTTO_COL_FG);
    otto_centerIn("TOUCH SCREEN TO CONTINUE (BACK TO CHECKLIST)", 0,
                  OTTO_SCR_W, 440, 2, OTTO_COL_GREY);
  }
}

// Decide the wizard outcome once both answers are in.
static void ottoWizDecide() {
  bool inWindow = (ottoWizMeasured >= OTTO_WIZ_TARGET_LO &&
                   ottoWizMeasured <= OTTO_WIZ_TARGET_HI);
  ottoWizPrev = mLPumpTime;
  if (inWindow && ottoWizDeltaOk) {
    ottoWizPassed = true;
    if (ottoWizStreak < 255) ottoWizStreak++;
    Serial.print(F("WIZARD PASS #")); Serial.print(ottoWizStreak);
    Serial.print(F("  mLPumpTime=")); Serial.println(mLPumpTime);
  } else {
    ottoWizPassed = false;
    ottoWizStreak = 0;
    if (!inWindow) {
      // measured uL for 1 mL commanded -> true s/mL = old * 1000 / measured
      mLPumpTime = mLPumpTime * 1000.0f / ottoWizMeasured;
      Serial.print(F("WIZARD ADJUST mLPumpTime "));
      Serial.print(ottoWizPrev); Serial.print(F(" -> "));
      Serial.println(mLPumpTime);
    } else {
      Serial.println(F("WIZARD DELTA FAIL - value kept, re-run"));
    }
  }
  ottoUiState = OTTO_ST_WIZ_OUTCOME;
  ottoUiDrawn = false;
  ottoTapMute(400);
}

// ---------------------------------------------------------- state machine --
// Call forever from the sketch's loop(). Never parks the CPU: after any
// result the panel returns to the submenu it launched from.
static void ottoPanelLoop() {
  int16_t tx, ty;

  // Remote launch: dispatch a queued serial command from an idle screen.
  // Interactive flows (checklist/confirm, wizard) are never hijacked.
  if (ottoSerPending) {
    char c = ottoSerPending;
    ottoSerPending = 0;
    if (ottoUiState == OTTO_ST_TOP || ottoUiState == OTTO_ST_MENU ||
        ottoUiState == OTTO_ST_RESULT || ottoUiState == OTTO_ST_POST) {
      int8_t a = ottoSerialAction(c);
      if (a >= 0) {
        ottoUiSel = (uint8_t)a;
        ottoPanelExecute((uint8_t)a);   // no checklist/confirm: remote
      }                                 // operator confirms out-of-band
    } else {
      Serial.println(F("BUSY (ON-SCREEN FLOW ACTIVE)"));
    }
  }

  switch (ottoUiState) {

    case OTTO_ST_TOP:
      if (!ottoUiDrawn) { ottoUiDrawTop(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        int8_t hit = ottoUiTopHit(tx, ty);
        if (hit >= 0) {
          ottoUiCat   = (uint8_t)hit;
          ottoUiState = OTTO_ST_MENU;
          ottoUiDrawn = false;
          ottoTapMute(300);
        }
      }
      break;

    case OTTO_ST_MENU:
      if (!ottoUiDrawn) { ottoUiDrawMenu(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        int8_t hit = ottoUiMenuHit(tx, ty);
        if (hit == -2) {                          // BACK
          ottoUiState = OTTO_ST_TOP;
          ottoUiDrawn = false;
          ottoTapMute(300);
        } else if (hit >= 0) {
          ottoUiSel  = (uint8_t)hit;
          ottoUiCkN  = ottoActChecklist(ottoUiSel, ottoUiCkItem);
          for (uint8_t i = 0; i < OTTO_CK_MAX; i++) ottoUiCk[i] = false;
          ottoUiState = OTTO_ST_CONFIRM;
          ottoUiDrawn = false;
          ottoTapMute(300);
        }
      }
      break;

    case OTTO_ST_CONFIRM:
      if (!ottoUiDrawn) { ottoUiDrawConfirm(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        if (ottoUiIn(tx, ty, OTTO_UI_CF_CAN_X, OTTO_UI_CF_BTN_Y,
                     OTTO_UI_CF_BTN_W, OTTO_UI_CF_BTN_H)) {
          ottoUiState = OTTO_ST_MENU;
          ottoUiDrawn = false;
          ottoTapMute(300);
        } else if (ottoUiIn(tx, ty, OTTO_UI_CF_GO_X, OTTO_UI_CF_BTN_Y,
                            OTTO_UI_CF_BTN_W, OTTO_UI_CF_BTN_H)) {
          if (ottoUiCkAll()) {
            ottoPanelExecute(ottoUiSel);   // blocking run; sets next state
          }
          // GO while unchecked: ignored (button is drawn disabled)
        } else if (ty >= OTTO_UI_CK_TOP &&
                   ty < OTTO_UI_CK_TOP + (int16_t)ottoUiCkN * OTTO_UI_CK_ROW_H) {
          uint8_t row = (uint8_t)((ty - OTTO_UI_CK_TOP) / OTTO_UI_CK_ROW_H);
          if (row < ottoUiCkN) {
            ottoUiCk[row] = !ottoUiCk[row];
            ottoUiDrawCkRow(row);
            ottoUiDrawGo();
          }
        }
      }
      break;

    case OTTO_ST_RESULT:
      if (!ottoUiDrawn) { ottoUiDrawResult(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        // touch anywhere returns; DONE may route via an instruction screen
        ottoUiPost = OTTO_POST_NONE;
        if (!ottoUiAborted) {
          if (ottoUiSel == OTTO_ACT_STEP4RINSE) {   // after STEP 4 FULL RINSE
            ottoUiPost = OTTO_POST_EMPTY_PLATE;
          } else if (ottoUiSel == OTTO_ACT_NESTED && !ottoUiStep7Ran) {
            ottoUiPost     = OTTO_POST_SEEDED;      // after first STEP 7 pass
            ottoUiStep7Ran = true;
          }
        }
        ottoUiState = (ottoUiPost != OTTO_POST_NONE) ? OTTO_ST_POST
                                                     : OTTO_ST_MENU;
        ottoUiDrawn = false;
        ottoTapMute(300);
      }
      break;

    case OTTO_ST_POST:
      if (!ottoUiDrawn) { ottoUiDrawPost(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        ottoUiState = OTTO_ST_MENU;
        ottoUiDrawn = false;
        ottoTapMute(300);
      }
      break;

    case OTTO_ST_WIZ_VOL:
      if (!ottoUiDrawn) { ottoUiDrawWizVol(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        if (ottoUiIn(tx, ty, 20, 380, 370, 80)) {   // CANCEL WIZARD
          ottoUiState = OTTO_ST_MENU;
          ottoUiDrawn = false;
          ottoTapMute(300);
          break;
        }
        for (uint8_t k = 0; k < 12; k++) {
          int16_t x, y;
          ottoUiKpRect(k, x, y);
          if (!ottoUiIn(tx, ty, x, y, OTTO_UI_KP_W, OTTO_UI_KP_H)) continue;
          if (k == 9) {                             // DEL
            size_t n = strlen(ottoWizEntry);
            if (n) ottoWizEntry[n - 1] = '\0';
            ottoUiDrawWizEntry();
          } else if (k == 11) {                     // OK
            long v = ottoWizEntryVal();
            if (v >= 0) {                           // else: ignore the press
              ottoWizMeasured = (float)v;
              ottoUiState = OTTO_ST_WIZ_DELTA;
              ottoUiDrawn = false;
              ottoTapMute(300);
            }
          } else {                                  // digit
            size_t n = strlen(ottoWizEntry);
            if (n < OTTO_UI_KP_ENTRY_MAX) {
              ottoWizEntry[n]     = ottoUiKpLabel(k)[0];
              ottoWizEntry[n + 1] = '\0';
              ottoUiDrawWizEntry();
            }
          }
          break;
        }
      }
      break;

    case OTTO_ST_WIZ_DELTA:
      if (!ottoUiDrawn) { ottoUiDrawWizDelta(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        if (ottoUiIn(tx, ty, 60, 230, 300, 170)) {        // YES
          ottoWizDeltaOk = true;
          ottoWizDecide();
        } else if (ottoUiIn(tx, ty, 440, 230, 300, 170)) { // NO
          ottoWizDeltaOk = false;
          ottoWizDecide();
        }
      }
      break;

    case OTTO_ST_WIZ_OUTCOME:
      if (!ottoUiDrawn) { ottoUiDrawWizOutcome(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        if (ottoWizPassed) {
          ottoUiState = OTTO_ST_MENU;               // done; streak kept
        } else {
          // loop: back to the STEP 5 checklist ("PLATE EMPTY" is row 1)
          ottoUiCkN = ottoActChecklist(ottoUiSel, ottoUiCkItem);
          for (uint8_t i = 0; i < OTTO_CK_MAX; i++) ottoUiCk[i] = false;
          ottoUiState = OTTO_ST_CONFIRM;
        }
        ottoUiDrawn = false;
        ottoTapMute(300);
      }
      break;
  }

  delay(10);   // ~100 Hz UI loop; touch poll self-limits to 25 ms
}

#endif // OTTO_PANEL_UI_H
