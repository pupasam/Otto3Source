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
//     sample 1, vacuum 1)
//   - ottoPanelLoop(): the whole UI state machine —
//       MENU -> CONFIRM (checklist gates GO) -> run (dashboard + STOP)
//            -> RESULT (DONE/ABORTED) -> optional POST instruction -> MENU
//     plus the STEP 3 guided calibration wizard:
//       run -> keypad "avg uL/well" -> "delta < 50 uL?" -> SUCCESS (save
//       value) or ADJUST (retune mLPumpTime in RAM, empty plate, loop)
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

// ------------------------------------------------------------- UI colors ---
#define OTTO_UI_BTN_FILL   0x18E3    // dark grey button
#define OTTO_UI_BTN_NAVY   0x0210    // FULL RUN accent
#define OTTO_UI_GO_OFF_TXT OTTO_COL_GREY

// -------------------------------------------------------- menu geometry ----
// 2 cols x 5 rows of 384x78 buttons under a 48 px title bar (all >= 78 px
// tall, ~13 mm on the 4" panel).
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

// STEP 3 pass window (uL/well): "average = 1 mL, aim a hair over".
#define OTTO_WIZ_TARGET_LO 1000.0f
#define OTTO_WIZ_TARGET_HI 1080.0f
// Sanity window for keypad entry (uL) — OK is ignored outside it.
#define OTTO_WIZ_ENTRY_LO   200
#define OTTO_WIZ_ENTRY_HI  3000

// ------------------------------------------------------------ UI states ----
#define OTTO_ST_MENU        0
#define OTTO_ST_CONFIRM     1
#define OTTO_ST_RESULT      2   // DONE or ABORTED
#define OTTO_ST_POST        3   // post-step operator instruction
#define OTTO_ST_WIZ_VOL     4   // keypad: avg uL per well
#define OTTO_ST_WIZ_DELTA   5   // yes/no: delta < 50 uL
#define OTTO_ST_WIZ_OUTCOME 6   // SUCCESS or ADJUST screen

// Post-instruction ids.
#define OTTO_POST_NONE        0
#define OTTO_POST_EMPTY_PLATE 1
#define OTTO_POST_SEEDED      2

// --------------------------------------------------------------- actions ---
// Labels, expected-duration formulas and function calls are EXACTLY the ones
// in PreRunCalibrationScript.ino (steps) and RunProtocol.ino (full run).
#define OTTO_ACT_STEP3     4    // index of the wizard-driven action
#define OTTO_ACT_STEP5     6
#define OTTO_ACT_FULLRUN   9

static const char* const OTTO_UI_LABEL[] = {
  "STEP 1.1 PRIME WASH",
  "STEP 1.2 PRIME CLV",
  "STEP 1.3 PRIME INC",
  "STEP 2 FULL RINSE",
  "STEP 3 DISPENSE",
  "STEP 4 DISP+ASP",
  "STEP 5 NESTED CYCLE",
  "STEP 7 ADDREAGENT INC",
  "STEP 8 ADDREAGENT CLV",
  "FULL RUN",
};
#define OTTO_UI_NACT 10

// Expected duration (s) — same formulas as the calibration script call
// sites; 0 = no estimate (footer shows elapsed only). Computed at confirm
// time so a wizard-adjusted mLPumpTime is reflected immediately.
static unsigned long ottoActExpectedS(uint8_t i) {
  switch (i) {
    case 0: case 1: case 2:
      return 20;
    case 3: return (unsigned long)(61 * mLPumpTime + 2 * fillTime);
    case 4: return (unsigned long)(WellLength * mLPumpTime);
    case 5: return (unsigned long)(WellLength * (mLPumpTime + vacTime) + fillTime);
    case 6: return (unsigned long)(WellLength * 2 * mLPumpTime + fillTime);
    default: return 0;   // steps 7, 8 and the full run have no formula
  }
}

// The same function calls as the calibration script / run protocol.
static void ottoActRun(uint8_t i) {
  switch (i) {
    case 0: RunPumpLine(WASH, 8, 20); break;
    case 1: RunPumpLine(CLEAVAGE, 8, 20); break;
    case 2: RunPumpLine(INCORPORATION, 8, 20); break;
    case 3: fullRinse(WellLength, SampleWells, mLPumpTime); break;
    case 4: DispenseLines(WellLength, SampleWells, WASH, mLPumpTime); break;
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
    case 9: runAutomation(); break;
  }
}

// Pre-run checklists, from the calibration script's SETUP banner + README
// operational rules. Short texts, drawn big; the GO button stays disabled
// until every row is checked.
#define OTTO_CK_RES   "RESERVOIRS FULL, LINES TO BOTTOM"
#define OTTO_CK_PUMP  "PUMP ARMED (STOP AFTER KEYPAD USE)"
#define OTTO_CK_VAC   "VACUUM OPEN, TRAP IN LINE, 12V ON"

static uint8_t ottoActChecklist(uint8_t i, const char** items) {
  switch (i) {
    case 0: case 1: case 2:                       // STEP 1.x — prime to vent
      items[0] = OTTO_CK_RES;
      items[1] = "VENT LINE (PORT 8) TO WASTE";
      items[2] = OTTO_CK_PUMP;
      return 3;
    case 3:                                       // STEP 2 — full rinse
      items[0] = OTTO_CK_RES;
      items[1] = "MANIFOLD ON EMPTY TEST PLATE";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 4:                                       // STEP 3 — dispense cal
      items[0] = "PLATE EMPTY";
      items[1] = OTTO_CK_RES;
      items[2] = "MANIFOLD SEATED ON PLATE";
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 5:                                       // STEP 4 — disp + asp
      items[0] = "PLATE EMPTY";
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 6:                                       // STEP 5 — nested cycle
      items[0] = "UNSEEDED FIRST, THEN SEEDED PLATE";
      items[1] = OTTO_CK_RES;
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      return 4;
    case 7: case 8:                               // STEP 7/8 — addReagent
      items[0] = "SBS REAGENTS LOADED (INC + CLV)";
      items[1] = "PORT 6 (AIR) DRY - NO TUBING";
      items[2] = OTTO_CK_VAC;
      items[3] = OTTO_CK_PUMP;
      items[4] = "MANIFOLD SEATED ON PLATE";
      return 5;
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
static uint8_t       ottoUiState       = OTTO_ST_MENU;
static bool          ottoUiDrawn       = false;
static uint8_t       ottoUiSel         = 0;      // selected action index
static bool          ottoUiCk[OTTO_CK_MAX];      // checklist ticks
static const char*   ottoUiCkItem[OTTO_CK_MAX];
static uint8_t       ottoUiCkN         = 0;
static bool          ottoUiAborted     = false;
static unsigned long ottoUiMeasuredS   = 0;      // DONE: measured duration
static unsigned long ottoUiAbortAtS    = 0;      // ABORTED: elapsed at stop
static uint8_t       ottoUiPost        = OTTO_POST_NONE;
static bool          ottoUiStep5Ran    = false;  // first nested-cycle pass done

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
// If a future shield revision mirrors this, taps land flipped: fix it HERE
// (one place), verified in seconds with the serial "TOUCH x y" log below.
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

// Poll the GT911 (25 ms rate limit) and act on press edges only:
//   - run live (ottoStopArmed): a press at/right of the STOP column latches
//     ottoAbortFlag — no confirmation, that's the point;
//   - otherwise: queue the tap for the UI loop.
// Called from Wait()'s 100 ms slices (via LowLevelFns) and from the UI loop.
void ottoTouchPoll() {
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
// Post-abort park: hardware to a known safe state. Pump and vacuum first
// (instant GPIO), then the slow valve moves. The abort latch is cleared
// before the moves — Select*Port are no-ops while it is set. Runs after the
// aborted routine has fully unwound (its call has returned).
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

// ----------------------------------------------------------------- menu ----
static void ottoUiMenuRect(uint8_t i, int16_t& x, int16_t& y) {
  x = (i % 2) ? OTTO_UI_COL1_X : OTTO_UI_COL0_X;
  y = OTTO_UI_MENU_TOP + (i / 2) * OTTO_UI_ROW_H +
      (OTTO_UI_ROW_H - OTTO_UI_BTN_H) / 2;
}

static void ottoUiDrawMenu() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  ottoGfx.fillRect(0, 0, OTTO_SCR_W, OTTO_UI_MENU_TOP, OTTO_COL_HDR);
  ottoGfx.setTextSize(3);
  ottoGfx.setTextColor(OTTO_COL_FG);
  ottoGfx.setCursor(16, 12);
  ottoGfx.print("OTTO3");
  otto_centerIn("SELECT ACTION", 220, 560, 12, 3, OTTO_COL_LIGHT);

  for (uint8_t i = 0; i < OTTO_UI_NACT; i++) {
    int16_t x, y;
    ottoUiMenuRect(i, x, y);
    bool full = (i == OTTO_ACT_FULLRUN);
    ottoUiButton(x, y, OTTO_UI_BTN_W, OTTO_UI_BTN_H, OTTO_UI_LABEL[i],
                 full ? OTTO_UI_BTN_NAVY : OTTO_UI_BTN_FILL, OTTO_COL_FG,
                 full ? OTTO_COL_BLUE : OTTO_COL_GREY, 3);
  }
}

static int8_t ottoUiMenuHit(int16_t tx, int16_t ty) {
  for (uint8_t i = 0; i < OTTO_UI_NACT; i++) {
    int16_t x, y;
    ottoUiMenuRect(i, x, y);
    if (ottoUiIn(tx, ty, x, y, OTTO_UI_BTN_W, OTTO_UI_BTN_H)) return (int8_t)i;
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
  Serial.print(F("PANEL RUN: ")); Serial.println(OTTO_UI_LABEL[i]);
  ottoAbortFlag    = false;
  ottoStopArmed    = true;    // STOP column is hot from this moment
  ottoTapMute(400);
  otto_needRebuild = true;    // leaving a UI screen: full dashboard rebuild
  ottoStepBegin(OTTO_UI_LABEL[i], ottoActExpectedS(i));

  ottoActRun(i);              // blocking; Wait() slices poll the STOP column

  ottoStopArmed = false;
  if (ottoAbortFlag) {
    ottoUiAbortAtS = otto_elapsedS();
    ottoPanelPark();          // clears the latch itself, then parks
    ottoUiAborted = true;
    ottoUiState   = OTTO_ST_RESULT;
    Serial.println(F("PANEL RESULT: ABORTED"));
  } else {
    ottoStepEnd();            // freeze measured duration on the dashboard
    ottoUiMeasuredS = otto_stepFrozenS;
    ottoUiAborted   = false;
    if (i == OTTO_ACT_STEP3) {          // guided calibration wizard
      ottoWizEntry[0] = '\0';
      ottoUiState = OTTO_ST_WIZ_VOL;
    } else {
      ottoUiState = OTTO_ST_RESULT;
    }
    Serial.print(F("PANEL RESULT: DONE in "));
    Serial.print(ottoUiMeasuredS); Serial.println(F(" s"));
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
    otto_centerIn("STEP 3 CALIBRATES ON AN EMPTY PLATE", 0, OTTO_SCR_W, 330, 2,
                  OTTO_COL_LIGHT);
  } else {                                  // OTTO_POST_SEEDED
    otto_centerIn("NEXT:", 0, OTTO_SCR_W, 50, 3, OTTO_COL_LIGHT);
    otto_centerIn("REPEAT STEP 5 ON A", 0, OTTO_SCR_W, 120, 5, OTTO_COL_AMBER);
    otto_centerIn("CELL-SEEDED PLATE", 0, OTTO_SCR_W, 180, 5, OTTO_COL_AMBER);
    otto_centerIn("PASS: <= 20 uL PER WELL AFTER ASPIRATIONS", 0, OTTO_SCR_W,
                  280, 2, OTTO_COL_FG);
    otto_centerIn("ENDS WET: ~1 mL PER WELL = INCUBATION STATE", 0, OTTO_SCR_W,
                  320, 2, OTTO_COL_FG);
  }
  otto_centerIn("TOUCH SCREEN TO RETURN TO MENU", 0, OTTO_SCR_W, 440, 2,
                OTTO_COL_GREY);
}

// ------------------------------------------------- STEP 3 wizard screens ---
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
  otto_centerIn("STEP 3 CALIBRATION", 0, 410, 20, 3, OTTO_COL_LIGHT);
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
  otto_centerIn("STEP 3 CALIBRATION", 0, OTTO_SCR_W, 30, 3, OTTO_COL_LIGHT);
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
    otto_centerIn("STEP 3 PASS", 0, OTTO_SCR_W, 30, 5, OTTO_COL_GREEN);
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
// result the panel returns to the menu.
static void ottoPanelLoop() {
  int16_t tx, ty;

  switch (ottoUiState) {

    case OTTO_ST_MENU:
      if (!ottoUiDrawn) { ottoUiDrawMenu(); ottoUiDrawn = true; }
      if (ottoTapConsume(tx, ty)) {
        int8_t hit = ottoUiMenuHit(tx, ty);
        if (hit >= 0) {
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
          if (ottoUiSel == 3) {                     // after STEP 2
            ottoUiPost = OTTO_POST_EMPTY_PLATE;
          } else if (ottoUiSel == OTTO_ACT_STEP5 && !ottoUiStep5Ran) {
            ottoUiPost    = OTTO_POST_SEEDED;       // after first STEP 5 pass
            ottoUiStep5Ran = true;
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
          // loop: back to the STEP 3 checklist ("PLATE EMPTY" is row 1)
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
