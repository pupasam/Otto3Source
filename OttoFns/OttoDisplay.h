// OttoDisplay.h — Otto3 live instrument dashboard for the GIGA Display Shield.
// v2: 4-panel bench dashboard + step timer (v1 was header/stage/progress).
//
// Header-only, C-style API, no dynamic allocation. Requires only
// Arduino_GigaDisplay_GFX (which pulls in Adafruit GFX). Copy this file into a
// sketch's folder alongside the sketch and include it. Intended to be included
// from Otto3Source only under `#ifdef OTTO_DISPLAY_ENABLED` so the same sources
// still build for arduino:avr:mega and display-less GIGA (see INTEGRATION.md).
//
//   ottoDisplayBegin();                          // once, in setup()
//   ottoStepBegin("STEP 3 - DISPENSE", 113);     // header name + timer start
//   ottoPanelReagent(3);                         // "3 / WASH", panel flashes
//   ottoPanelSample(4);                          // "4 / WELL 3"
//   ottoPanelSolenoid(true);                     // vacuum panel: OPEN (green)
//   ottoPanelVacuumPort(4);                      // vacuum panel: PORT 4 - WELL 3
//   ottoPanelPump(true, 18.9f);                  // RUNNING + live "19s LEFT"
//   ottoTick();                                  // call at 2-10 Hz from waits
//   ottoStepEnd();                               // freeze timer, pill = DONE
//   ottoCue("EYES ON NEEDLES", OTTO_CUE_WATCH);  // observation cue banner in
//                                                // the footer (CALM = steady
//                                                // grey/blue info; WATCH =
//                                                // amber, pulsed by ottoTick)
//   ottoCueArm("RED AT NEEDLE 3", 6300);         // WATCH banner + live "T-Ns"
//                                                // countdown; at T-0: triple
//                                                // full-screen strobe, then
//                                                // "NOW - HOLD THAT IMAGE"
//   ottoCueClear();                              // banner off, footer back
//   ottoShowError("VALVE 2 NOT RESPONDING");     // red takeover; any later
//                                                // call rebuilds the dashboard
//
// Screen layout (800x480 landscape, setRotation(1)):
//
//   +------------------------------------------------+
//   | header: OTTO3 | step name          | state pill|   y   0..63
//   +-----------------------+------------------------+
//   |  REAGENT VALVE        |  SAMPLE VALVE          |   y  64..239
//   |  port # BIG + name    |  port # BIG + label    |
//   +-----------------------+------------------------+
//   |  VACUUM               |  PUMP                  |   y 240..415
//   |  OPEN/CLOSED + port   |  RUNNING/STOPPED + s   |
//   +-----------------------+------------------------+
//   | footer: elapsed / expected + progress bar      |   y 416..479
//   +------------------------------------------------+
//
// Panel rectangles (x, y, w, h):
//   REAGENT (  0,  64, 400, 176)   SAMPLE (400,  64, 400, 176)
//   VACUUM  (  0, 240, 400, 176)   PUMP   (400, 240, 400, 176)
//
// STOP zone (optional, touch builds - OttoPanel): define
// OTTO_DISPLAY_STOP_ZONE before including this header and the right column
// x >= 680 (120 x 416 px, below the header) is reserved for a big red STOP
// button: the panel grid narrows to 340 px per panel and the footer shortens
// to 680 px, so no partial repaint ever paints under the button. This header
// only DRAWS the button (on every dashboard rebuild); deciding what a press
// there means is the touch layer's job (OttoPanelUI.h). Without the define
// the classic full-width geometry above is byte-identical.
//
// No-flicker discipline: every update repaints only the rectangle it owns
// (partial fillRect + reprint). Full-screen clears happen only in
// ottoDisplayBegin(), ottoShowError(), and the one rebuild after an error.
//
// Port label maps (domain facts, Otto3 calibration plumbing):
//   Reagent valve : 1=CLEAVAGE  2=INCORPORATION  3=WASH  4=EMPTY  6=AIR
//                   (6 must stay dry - air pushes)          others = "--"
//   Sample valve  : 1=PARK  2..7=WELL 1..WELL 6  8=VENT     others = "--"
//   Vacuum valve  : 1=PARK  2..7=WELL 1..WELL 6  8=PARK     others = "--"

#ifndef OTTO_DISPLAY_H
#define OTTO_DISPLAY_H

#include <Arduino.h>
#include "Arduino_GigaDisplay_GFX.h"

// ------------------------------------------------------- colors (RGB565) ---
#define OTTO_COL_BG      0x0000  // black
#define OTTO_COL_FG      0xFFFF  // white
#define OTTO_COL_HDR     0x18E3  // very dark grey (header bar)
#define OTTO_COL_GREEN   0x07E0  // RUNNING / OPEN / bar fill
#define OTTO_COL_BLUE    0x051F  // DONE pill (azure)
#define OTTO_COL_RED     0xF800  // ERROR
#define OTTO_COL_AMBER   0xFD20  // overrun bar (past 100%)
#define OTTO_COL_GREY    0x8410  // STOPPED / CLOSED words, IDLE pill, bar frame
#define OTTO_COL_BORDER  0x39E7  // panel border rings (dark grey)
#define OTTO_COL_LIGHT   0xC618  // panel titles, secondary lines
#define OTTO_COL_FLASH   0xFFFF  // change-flash ring (white pops on black)

// ------------------------------------------------------ layout constants ---
#define OTTO_SCR_W       800
#define OTTO_SCR_H       480
#define OTTO_HDR_H        64     // header bar 0..63
#define OTTO_HDR_NAME_X  146     // step-name region 146..549 (OTTO3 ends ~136)
#define OTTO_HDR_NAME_W  404
#define OTTO_HDR_PILL_X  550     // pill reserve 550..799
#define OTTO_PAN_H       176
#define OTTO_PAN_TOP      64     // first panel row y
#define OTTO_FOOT_Y      416     // footer 416..479
#define OTTO_FOOT_H       64
#define OTTO_TIME_X       16     // footer time text "MM:SS / MM:SS", size 3
#define OTTO_TIME_Y      436
#define OTTO_TIME_W      252
#define OTTO_BAR_Y       428
#define OTTO_BAR_H        36
#ifdef OTTO_DISPLAY_STOP_ZONE    // right column reserved for the STOP button
#define OTTO_STOP_X      680     // STOP column x 680..799 (120 px wide)
#define OTTO_PAN_W       340     // panel grid narrows: 2 x 340 = 680
#define OTTO_FOOT_W      680     // footer stops short of the column
#define OTTO_BAR_X       272     // footer progress bar (shortened)
#define OTTO_BAR_W       400
#else
#define OTTO_PAN_W       400     // panel grid: 2 x 2, full width
#define OTTO_FOOT_W      800
#define OTTO_BAR_X       280     // footer progress bar
#define OTTO_BAR_W       500
#endif
#define OTTO_BAR_PAD       4     // gap between frame and fill
#define OTTO_FLASH_MS    700     // change-flash ring lifetime
#define OTTO_MARGIN       12     // min side margin for centered text
#define OTTO_CUE_PULSE_MS 500    // WATCH-cue banner pulse half-period

// Observation-cue levels (ottoCue).
#define OTTO_CUE_CALM      0     // informational: no need to watch
#define OTTO_CUE_WATCH     1     // high attention: amber, pulsing
#define OTTO_CUE_NOW       2     // internal: solid post-strobe "NOW" banner
#define OTTO_CUE_STROBE_MS 120   // per-frame strobe duration (x3 frames)

// Panel indices.
#define OTTO_PAN_REAGENT 0
#define OTTO_PAN_SAMPLE  1
#define OTTO_PAN_VACUUM  2
#define OTTO_PAN_PUMP    3

// Header pill states (internal; driven by step/error calls).
#define OTTO_PILL_IDLE    0
#define OTTO_PILL_RUNNING 1
#define OTTO_PILL_DONE    2
#define OTTO_PILL_ERROR   3

// --------------------------------------------------------- module state ----
static GigaDisplay_GFX ottoGfx;              // raw GFX handle (usable directly)

static bool          otto_inited      = false;
static bool          otto_needRebuild = false; // error screen wiped the layout
static uint8_t       otto_pill        = OTTO_PILL_IDLE;

// Step / timer.
static char          otto_stepName[40] = "";
static bool          otto_stepActive   = false; // timer counting
static bool          otto_stepDone     = false; // ended; elapsed frozen
static unsigned long otto_stepStartMs  = 0;
static unsigned long otto_stepExpected = 0;    // seconds; 0 = unknown
static unsigned long otto_stepFrozenS  = 0;    // elapsed at ottoStepEnd()

// Footer render cache.
static unsigned long otto_lastElapsedS = (unsigned long)-1;
static char          otto_lastTime[24] = "";
static bool          otto_barFrame     = false;
static bool          otto_barOver      = false; // painted amber (overrun)
static int16_t       otto_lastFillW    = -1;

// Panel value cache (what is on screen / what to rebuild after an error).
static int           otto_reagentPort  = -1;   // -1 = not yet reported
static int           otto_samplePort   = -1;
static int           otto_vacPort      = -1;
static bool          otto_solOpen      = false;
static bool          otto_pumpRunning  = false;
static bool          otto_pumpTimed    = false; // countdown known?
static unsigned long otto_pumpEndMs    = 0;
static long          otto_pumpShownS   = -1;   // countdown value on screen

// Change-flash ring deadlines (0 = ring is in its normal color).
static unsigned long otto_flashUntil[4] = {0, 0, 0, 0};

// Observation-cue banner (takes over the footer while active).
static bool          otto_cueActive    = false;
static uint8_t       otto_cueLevel     = OTTO_CUE_CALM;
static char          otto_cueMsg[44]   = "";
static bool          otto_cuePhase     = false;  // WATCH pulse phase (on/off)
static unsigned long otto_cueNextPulse = 0;

// Armed cue (ottoCueArm): live T-Ns countdown, strobe at expiry.
static bool          otto_cueArmed     = false;
static unsigned long otto_cueFireMs    = 0;      // millis() of T-0
static long          otto_cueShownT    = -1;     // countdown secs on screen
static char          otto_cueBase[36]  = "";     // armed text minus " T-Ns"

// Panel origins, indexed by OTTO_PAN_*.
static const int16_t OTTO_PAN_X[4] = {0, OTTO_PAN_W, 0, OTTO_PAN_W};
static const int16_t OTTO_PAN_Y[4] = {OTTO_PAN_TOP, OTTO_PAN_TOP,
                                      OTTO_PAN_TOP + OTTO_PAN_H,
                                      OTTO_PAN_TOP + OTTO_PAN_H};
static const char* const OTTO_PAN_TITLE[4] =
    {"REAGENT VALVE", "SAMPLE VALVE", "VACUUM", "PUMP"};

// ------------------------------------------------------------ label maps ---

// Reagent valve: port -> reagent name (getReagentPort() in OttoFns/constants).
static const char* otto_reagentLabel(int port) {
  switch (port) {
    case 1:  return "CLEAVAGE";
    case 2:  return "INCORPORATION";
    case 3:  return "WASH";
    case 4:  return "EMPTY";
    case 6:  return "AIR";
    default: return "--";
  }
}

// Sample/vacuum valves: port -> well label. Port 1 = PARK (must stay
// unplumbed - required for software operation). Port 8 differs per valve:
// VENT on the sample (dispensing) valve, PARK on the vacuum valve.
static void otto_wellLabel(int port, bool port8IsVent, char* buf, size_t n) {
  if (port >= 2 && port <= 7) {
    snprintf(buf, n, "WELL %d", port - 1);
  } else if (port == 1) {
    snprintf(buf, n, "PARK");
  } else if (port == 8) {
    snprintf(buf, n, port8IsVent ? "VENT" : "PARK");
  } else {
    snprintf(buf, n, "--");
  }
}

// ---------------------------------------------------- internal helpers -----

// Largest classic-font text size (1..maxSize) whose single line fits maxW.
// Classic GFX font advance = 6 px * size per character; height = 8 px * size.
static uint8_t otto_fitSize(const char* s, int16_t maxW, uint8_t maxSize) {
  int16_t len = (int16_t)strlen(s);
  if (len < 1) len = 1;
  for (uint8_t sz = maxSize; sz > 1; sz--) {
    if ((int32_t)len * 6 * sz <= maxW) return sz;
  }
  return 1;
}

// Print one line centered inside [x, x+w), top edge at yTop.
static void otto_centerIn(const char* s, int16_t x, int16_t w, int16_t yTop,
                          uint8_t size, uint16_t color) {
  int16_t tw = (int16_t)((int32_t)strlen(s) * 6 * size);
  int16_t tx = x + (w - tw) / 2;
  if (tx < x) tx = x;
  ottoGfx.setTextSize(size);
  ottoGfx.setTextColor(color);
  ottoGfx.setCursor(tx, yTop);
  ottoGfx.print(s);
}

// Format seconds as MM:SS (minutes uncapped: 100 min prints as "100:00").
static void otto_fmtMMSS(unsigned long s, char* buf, size_t n) {
  snprintf(buf, n, "%02lu:%02lu", s / 60UL, s % 60UL);
}

// ---- header ----------------------------------------------------------------

static void otto_drawPill() {
  ottoGfx.fillRect(OTTO_HDR_PILL_X, 0, OTTO_SCR_W - OTTO_HDR_PILL_X,
                   OTTO_HDR_H, OTTO_COL_HDR);
  const char* label;
  uint16_t col;
  switch (otto_pill) {
    case OTTO_PILL_RUNNING: label = "RUNNING"; col = OTTO_COL_GREEN; break;
    case OTTO_PILL_DONE:    label = "DONE";    col = OTTO_COL_BLUE;  break;
    case OTTO_PILL_ERROR:   label = "ERROR";   col = OTTO_COL_RED;   break;
    default:                label = "IDLE";    col = OTTO_COL_GREY;  break;
  }
  int16_t tw = (int16_t)strlen(label) * 6 * 3;      // text size 3
  int16_t pw = tw + 28;                             // padding
  int16_t px = OTTO_SCR_W - 16 - pw;
  ottoGfx.fillRoundRect(px, 12, pw, 40, 8, col);
  ottoGfx.setTextSize(3);
  ottoGfx.setTextColor(OTTO_COL_BG);
  ottoGfx.setCursor(px + 14, 12 + (40 - 24) / 2 + 1);
  ottoGfx.print(label);
}

static void otto_drawStepName() {
  ottoGfx.fillRect(OTTO_HDR_NAME_X, 0, OTTO_HDR_NAME_W, OTTO_HDR_H,
                   OTTO_COL_HDR);
  if (!otto_stepName[0]) return;
  uint8_t sz = otto_fitSize(otto_stepName, OTTO_HDR_NAME_W - 8, 4);
  int16_t y  = (OTTO_HDR_H - 8 * sz) / 2;
  otto_centerIn(otto_stepName, OTTO_HDR_NAME_X, OTTO_HDR_NAME_W, y, sz,
                OTTO_COL_FG);
}

static void otto_drawHeader() {
  ottoGfx.fillRect(0, 0, OTTO_SCR_W, OTTO_HDR_H, OTTO_COL_HDR);
  ottoGfx.setTextSize(4);                            // 32 px tall
  ottoGfx.setTextColor(OTTO_COL_FG);
  ottoGfx.setCursor(16, (OTTO_HDR_H - 32) / 2);
  ottoGfx.print("OTTO3");
  otto_drawStepName();
  otto_drawPill();
}

// ---- panels ----------------------------------------------------------------

// 3-px border ring inset 2 px into the panel (color = BORDER or FLASH).
static void otto_panelRing(uint8_t p, uint16_t color) {
  int16_t x = OTTO_PAN_X[p], y = OTTO_PAN_Y[p];
  ottoGfx.fillRect(x + 2, y + 2, OTTO_PAN_W - 4, 3, color);              // top
  ottoGfx.fillRect(x + 2, y + OTTO_PAN_H - 5, OTTO_PAN_W - 4, 3, color); // bot
  ottoGfx.fillRect(x + 2, y + 5, 3, OTTO_PAN_H - 10, color);             // left
  ottoGfx.fillRect(x + OTTO_PAN_W - 5, y + 5, 3, OTTO_PAN_H - 10, color);
}

// Ring + title. Content region below the title is (x+8, y+30, 384, 138).
static void otto_panelFrame(uint8_t p) {
  int16_t x = OTTO_PAN_X[p], y = OTTO_PAN_Y[p];
  otto_panelRing(p, otto_flashUntil[p] ? OTTO_COL_FLASH : OTTO_COL_BORDER);
  ottoGfx.setTextSize(2);
  ottoGfx.setTextColor(OTTO_COL_LIGHT);
  ottoGfx.setCursor(x + 16, y + 12);
  ottoGfx.print(OTTO_PAN_TITLE[p]);
}

// Clear a panel's content region (keeps ring + title).
static void otto_panelClearContent(uint8_t p) {
  ottoGfx.fillRect(OTTO_PAN_X[p] + 8, OTTO_PAN_Y[p] + 30, OTTO_PAN_W - 16,
                   OTTO_PAN_H - 38, OTTO_COL_BG);
}

// Flash the ring white now; ottoTick() restores it after OTTO_FLASH_MS.
static void otto_panelFlash(uint8_t p) {
  otto_panelRing(p, OTTO_COL_FLASH);
  unsigned long until = millis() + OTTO_FLASH_MS;
  otto_flashUntil[p] = until ? until : 1;   // 0 is the "no flash" sentinel
}

// Big port digit + label underneath (reagent + sample panels).
static void otto_drawPortPanel(uint8_t p, int port, const char* label) {
  int16_t x = OTTO_PAN_X[p], y = OTTO_PAN_Y[p];
  otto_panelClearContent(p);

  char d[8];
  if (port >= 0) snprintf(d, sizeof(d), "%d", port);
  else           snprintf(d, sizeof(d), "-");
  otto_centerIn(d, x, OTTO_PAN_W, y + 34, 10, OTTO_COL_FG);  // 80 px digits

  uint8_t sz = otto_fitSize(label, OTTO_PAN_W - 24, 4);
  otto_centerIn(label, x, OTTO_PAN_W, y + 126 + (32 - 8 * sz) / 2, sz,
                OTTO_COL_LIGHT);
}

static void otto_drawReagentPanel() {
  otto_drawPortPanel(OTTO_PAN_REAGENT, otto_reagentPort,
                     otto_reagentPort >= 0 ? otto_reagentLabel(otto_reagentPort)
                                           : "--");
}

static void otto_drawSamplePanel() {
  char lbl[12] = "--";
  if (otto_samplePort >= 0) {
    otto_wellLabel(otto_samplePort, true /*8=VENT*/, lbl, sizeof(lbl));
  }
  otto_drawPortPanel(OTTO_PAN_SAMPLE, otto_samplePort, lbl);
}

// Vacuum panel: solenoid word dominates, aspiration-valve port line beneath.
static void otto_drawVacuumPanel() {
  int16_t x = OTTO_PAN_X[OTTO_PAN_VACUUM], y = OTTO_PAN_Y[OTTO_PAN_VACUUM];
  otto_panelClearContent(OTTO_PAN_VACUUM);

  otto_centerIn(otto_solOpen ? "OPEN" : "CLOSED", x, OTTO_PAN_W, y + 36, 8,
                otto_solOpen ? OTTO_COL_GREEN : OTTO_COL_GREY);  // 64 px word

  char line[24];
  if (otto_vacPort >= 0) {
    char lbl[12];
    otto_wellLabel(otto_vacPort, false /*8=PARK*/, lbl, sizeof(lbl));
    snprintf(line, sizeof(line), "PORT %d - %s", otto_vacPort, lbl);
  } else {
    snprintf(line, sizeof(line), "PORT --");
  }
  otto_centerIn(line, x, OTTO_PAN_W, y + 124, 3, OTTO_COL_LIGHT);
}

// Pump countdown sub-region only (so ottoTick can update it alone).
static void otto_drawPumpCountdown() {
  int16_t x = OTTO_PAN_X[OTTO_PAN_PUMP], y = OTTO_PAN_Y[OTTO_PAN_PUMP];
  ottoGfx.fillRect(x + 8, y + 108, OTTO_PAN_W - 16, 56, OTTO_COL_BG);
  if (!otto_pumpRunning || !otto_pumpTimed) return;
  long remain = (long)(otto_pumpEndMs - millis());
  long secs   = remain > 0 ? (remain + 999L) / 1000L : 0;
  otto_pumpShownS = secs;
  char buf[16];
  snprintf(buf, sizeof(buf), "%lds LEFT", secs);
  otto_centerIn(buf, x, OTTO_PAN_W, y + 112, 5, OTTO_COL_FG);
}

static void otto_drawPumpPanel() {
  int16_t x = OTTO_PAN_X[OTTO_PAN_PUMP], y = OTTO_PAN_Y[OTTO_PAN_PUMP];
  otto_panelClearContent(OTTO_PAN_PUMP);
  otto_centerIn(otto_pumpRunning ? "RUNNING" : "STOPPED", x, OTTO_PAN_W,
                y + 38, 7,
                otto_pumpRunning ? OTTO_COL_GREEN : OTTO_COL_GREY);  // 56 px
  otto_drawPumpCountdown();
}

// ---- footer ----------------------------------------------------------------

static unsigned long otto_elapsedS() {
  if (otto_stepActive) return (millis() - otto_stepStartMs) / 1000UL;
  if (otto_stepDone)   return otto_stepFrozenS;
  return 0;
}

static void otto_drawBarFrame() {
  ottoGfx.fillRect(OTTO_BAR_X, OTTO_BAR_Y, OTTO_BAR_W, OTTO_BAR_H, OTTO_COL_BG);
  ottoGfx.drawRect(OTTO_BAR_X, OTTO_BAR_Y, OTTO_BAR_W, OTTO_BAR_H,
                   OTTO_COL_GREY);
  ottoGfx.drawRect(OTTO_BAR_X + 1, OTTO_BAR_Y + 1, OTTO_BAR_W - 2,
                   OTTO_BAR_H - 2, OTTO_COL_GREY);
  otto_barFrame  = true;
  otto_barOver   = false;
  otto_lastFillW = 0;
}

// Repaint elapsed/expected text + bar. Cheap: skips everything while the
// whole-second elapsed value is unchanged (unless force). Fully suppressed
// while an observation cue owns the footer region (see ottoCue below).
static void otto_footerUpdate(bool force) {
  if (otto_cueActive) return;
  unsigned long e = otto_elapsedS();
  if (!force && e == otto_lastElapsedS) return;
  otto_lastElapsedS = e;

  if (!otto_barFrame) otto_drawBarFrame();

  const bool over = (otto_stepExpected > 0 && e > otto_stepExpected);

  // Time text: "MM:SS / MM:SS", or elapsed only when expected is unknown.
  char eb[12], xb[12], buf[24];
  otto_fmtMMSS(e, eb, sizeof(eb));
  if (otto_stepExpected > 0) {
    otto_fmtMMSS(otto_stepExpected, xb, sizeof(xb));
    snprintf(buf, sizeof(buf), "%s / %s", eb, xb);
  } else {
    snprintf(buf, sizeof(buf), "%s", eb);
  }
  if (force || strncmp(buf, otto_lastTime, sizeof(otto_lastTime)) != 0) {
    strncpy(otto_lastTime, buf, sizeof(otto_lastTime) - 1);
    otto_lastTime[sizeof(otto_lastTime) - 1] = '\0';
    ottoGfx.fillRect(OTTO_TIME_X, OTTO_TIME_Y - 4, OTTO_TIME_W, 32,
                     OTTO_COL_BG);
    ottoGfx.setTextSize(3);
    ottoGfx.setTextColor(over ? OTTO_COL_AMBER : OTTO_COL_FG);
    ottoGfx.setCursor(OTTO_TIME_X, OTTO_TIME_Y);
    ottoGfx.print(buf);
  }

  // Bar fill. Clamped at 100%; the whole fill turns amber on overrun instead
  // of clipping silently. expected == 0 leaves the bar empty (elapsed only).
  const int16_t innerW = OTTO_BAR_W - 2 * OTTO_BAR_PAD;
  const int16_t innerH = OTTO_BAR_H - 2 * OTTO_BAR_PAD;
  const int16_t ix     = OTTO_BAR_X + OTTO_BAR_PAD;
  const int16_t iy     = OTTO_BAR_Y + OTTO_BAR_PAD;
  if (otto_stepExpected > 0) {
    if (over) {
      if (!otto_barOver) {                       // recolor full bar once
        ottoGfx.fillRect(ix, iy, innerW, innerH, OTTO_COL_AMBER);
        otto_barOver   = true;
        otto_lastFillW = innerW;
      }
    } else {
      int16_t fillW = (int16_t)(((uint64_t)e * (uint64_t)innerW) /
                                (uint64_t)otto_stepExpected);
      if (fillW < otto_lastFillW) {              // restarted: clear the track
        ottoGfx.fillRect(ix, iy, innerW, innerH, OTTO_COL_BG);
        otto_lastFillW = 0;
      }
      if (fillW > otto_lastFillW) {              // paint only the new slice
        ottoGfx.fillRect(ix + otto_lastFillW, iy, fillW - otto_lastFillW,
                         innerH, OTTO_COL_GREEN);
        otto_lastFillW = fillW;
      }
    }
  }
}

static void otto_drawFooter() {
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_FOOT_W, OTTO_FOOT_H, OTTO_COL_BG);
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_FOOT_W, 1, OTTO_COL_BORDER);
  otto_barFrame     = false;
  otto_lastTime[0]  = '\0';
  otto_lastElapsedS = (unsigned long)-1;
  otto_footerUpdate(true);
}

// ---- observation-cue banner -------------------------------------------------
// Paints the footer region (0..OTTO_FOOT_W x OTTO_FOOT_Y..479) as a cue
// banner. Panels and the STOP zone own disjoint rectangles, so nothing
// fights it; the elapsed/bar repaint is suppressed while a cue is active.
static void otto_drawCue() {
  uint16_t bg, fg;
  if (otto_cueLevel == OTTO_CUE_WATCH) {         // pulsing amber
    bg = otto_cuePhase ? OTTO_COL_AMBER : OTTO_COL_BG;
    fg = otto_cuePhase ? OTTO_COL_BG    : OTTO_COL_AMBER;
  } else if (otto_cueLevel == OTTO_CUE_NOW) {    // solid amber, no pulse
    bg = OTTO_COL_AMBER;
    fg = OTTO_COL_BG;
  } else {                                       // steady grey/blue info
    bg = OTTO_COL_HDR;
    fg = OTTO_COL_BLUE;
  }
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_FOOT_W, OTTO_FOOT_H, bg);
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_FOOT_W, 1, OTTO_COL_BORDER);
  uint8_t sz = otto_fitSize(otto_cueMsg, OTTO_FOOT_W - 2 * OTTO_MARGIN, 4);
  otto_centerIn(otto_cueMsg, 0, OTTO_FOOT_W,
                OTTO_FOOT_Y + (OTTO_FOOT_H - 8 * sz) / 2 + 1, sz, fg);
}

// ---- STOP zone (touch builds only) ------------------------------------------

#ifdef OTTO_DISPLAY_STOP_ZONE
// Big red STOP button filling the reserved right column (below the header).
// Redrawn on every dashboard rebuild; the touch layer owns the hit-testing.
static void otto_drawStopZone() {
  const int16_t x = OTTO_STOP_X, w = OTTO_SCR_W - OTTO_STOP_X;   // 120 px
  const int16_t y = OTTO_HDR_H,  h = OTTO_SCR_H - OTTO_HDR_H;    // 416 px
  ottoGfx.fillRect(x, y, w, h, OTTO_COL_BG);
  ottoGfx.fillRoundRect(x + 4, y + 4, w - 8, h - 8, 12, OTTO_COL_RED);
  // "STOP" stacked vertically: size-8 letters (48 x 64 px cells).
  static const char letters[5] = "STOP";
  const int16_t lh = 64, gap = 20;
  int16_t ly = y + (h - (4 * lh + 3 * gap)) / 2;
  for (uint8_t i = 0; i < 4; i++) {
    char s[2] = {letters[i], '\0'};
    otto_centerIn(s, x, w, ly + i * (lh + gap), 8, OTTO_COL_FG);
  }
}
#endif

// ---- full dashboard (begin + post-error rebuild) ---------------------------

static void otto_drawDashboard() {
  ottoGfx.fillScreen(OTTO_COL_BG);
  otto_drawHeader();
  for (uint8_t p = 0; p < 4; p++) {
    otto_flashUntil[p] = 0;                     // rings come back calm
    otto_panelFrame(p);
  }
  otto_drawReagentPanel();
  otto_drawSamplePanel();
  otto_drawVacuumPanel();
  otto_drawPumpPanel();
  otto_drawFooter();
  if (otto_cueActive) otto_drawCue();           // cue survives a rebuild
#ifdef OTTO_DISPLAY_STOP_ZONE
  otto_drawStopZone();
#endif
}

// Rebuild the dashboard from cached state after the error screen wiped it.
static void otto_rebuildIfNeeded() {
  if (!otto_needRebuild) return;
  otto_needRebuild = false;
  // Restore the pill from the step state (the run has moved past the error).
  otto_pill = otto_stepActive ? OTTO_PILL_RUNNING
            : otto_stepDone   ? OTTO_PILL_DONE
                              : OTTO_PILL_IDLE;
  otto_drawDashboard();
}

// ------------------------------------------------------------ public API ---

// Initialize the display (landscape 800x480) and draw the empty dashboard.
static void ottoDisplayBegin() {
  ottoGfx.begin();
  ottoGfx.setRotation(1);           // 800x480 landscape
  ottoGfx.setTextWrap(false);
  otto_inited = true;
  otto_drawDashboard();
}

// Start a protocol step: name goes in the header, the elapsed clock restarts
// at 00:00, and expected_s (seconds) sets the footer progress-bar target.
// expected_s == 0 means unknown: the footer shows elapsed only, no bar fill.
// The caller computes expected_s from firmware constants at the call site
// (e.g. 6 wells x mLPumpTime seconds); this just takes seconds.
static void ottoStepBegin(const char* name, unsigned long expected_s) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  strncpy(otto_stepName, (name && name[0]) ? name : "--",
          sizeof(otto_stepName) - 1);
  otto_stepName[sizeof(otto_stepName) - 1] = '\0';
  otto_stepActive   = true;
  otto_stepDone     = false;
  otto_stepStartMs  = millis();
  otto_stepExpected = expected_s;
  otto_pill         = OTTO_PILL_RUNNING;
  otto_cueActive    = false;        // a new step retires any leftover cue
  otto_cueArmed     = false;        // ...including a pending countdown
  otto_drawStepName();
  otto_drawPill();
  otto_drawFooter();                // fresh 00:00 + empty bar
}

// End the current step: freeze the timer (the final elapsed time stays on
// screen - that's the measured step duration to write down) and set the
// header pill to DONE. The bar keeps its final fill.
static void ottoStepEnd() {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  if (otto_stepActive) {
    otto_stepFrozenS = (millis() - otto_stepStartMs) / 1000UL;
    otto_stepActive  = false;
  }
  otto_stepDone = true;
  otto_pill     = OTTO_PILL_DONE;
  otto_drawPill();
  otto_footerUpdate(true);          // land exactly on the final time
}                                   // (no-op while a cue holds the footer)

// Observation cue: banner over the footer telling the operator whether the
// instrument needs eyes right now. msg <= ~40 chars (auto-sized, truncated
// beyond 43). Levels:
//   OTTO_CUE_CALM  - steady dark banner, blue text: informational
//   OTTO_CUE_WATCH - amber banner, pulsed every OTTO_CUE_PULSE_MS by
//                    ottoTick() so peripheral vision catches it
// The elapsed/progress footer is suppressed while a cue is up and comes
// back on ottoCueClear(). ottoStepBegin() also retires any leftover cue.
static void ottoCue(const char* msg, uint8_t level) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  strncpy(otto_cueMsg, (msg && msg[0]) ? msg : "--", sizeof(otto_cueMsg) - 1);
  otto_cueMsg[sizeof(otto_cueMsg) - 1] = '\0';
  otto_cueLevel     = level;
  otto_cueActive    = true;
  otto_cueArmed     = false;                     // replaces any countdown
  otto_cuePhase     = true;                      // start on the loud phase
  otto_cueNextPulse = millis() + OTTO_CUE_PULSE_MS;
  otto_drawCue();
}

// Retire the cue banner and restore the elapsed/progress footer.
static void ottoCueClear() {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  otto_cueArmed = false;
  if (!otto_cueActive) return;
  otto_cueActive = false;
  otto_drawFooter();
}

// ---- armed cue: live countdown + strobe at T-0 ------------------------------

// Rebuild otto_cueMsg as "<base> T-Ns" from the time left to T-0.
static void otto_cueCompose() {
  long remain = (long)(otto_cueFireMs - millis());
  long secs   = remain > 0 ? (remain + 999L) / 1000L : 0;
  otto_cueShownT = secs;
  snprintf(otto_cueMsg, sizeof(otto_cueMsg), "%s T-%lds", otto_cueBase, secs);
}

// T-0: triple full-screen strobe (white/amber/white, OTTO_CUE_STROBE_MS
// each), then a full dashboard rebuild from cached state (the post-error
// rebuild path), with the banner left on the static "NOW" text until the
// next ottoCue*/ottoStep* call replaces it.
static void otto_cueFire() {
  otto_cueArmed = false;
  ottoGfx.fillScreen(OTTO_COL_FG);    delay(OTTO_CUE_STROBE_MS);
  ottoGfx.fillScreen(OTTO_COL_AMBER); delay(OTTO_CUE_STROBE_MS);
  ottoGfx.fillScreen(OTTO_COL_FG);    delay(OTTO_CUE_STROBE_MS);
  strncpy(otto_cueMsg, "NOW - HOLD THAT IMAGE", sizeof(otto_cueMsg) - 1);
  otto_cueMsg[sizeof(otto_cueMsg) - 1] = '\0';
  otto_cueLevel    = OTTO_CUE_NOW;    // solid amber, not pulsed
  otto_cueActive   = true;
  otto_needRebuild = true;            // strobe wiped the layout
  otto_rebuildIfNeeded();             // redraw dashboard + cue banner
}

// Precision cue: WATCH-level banner with a live " T-Ns" countdown appended
// (updated each second by ottoTick), firing in fire_in_ms. At expiry the
// screen strobes 3x and the banner reads "NOW - HOLD THAT IMAGE" until the
// next cue/step call. Driven by ottoTick() from Wait()'s 100 ms slices, so
// T-0 carries up to ~100 ms of jitter. msg <= ~28 chars (room for " T-Ns").
static void ottoCueArm(const char* msg, unsigned long fire_in_ms) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  strncpy(otto_cueBase, (msg && msg[0]) ? msg : "--", sizeof(otto_cueBase) - 1);
  otto_cueBase[sizeof(otto_cueBase) - 1] = '\0';
  otto_cueFireMs    = millis() + fire_in_ms;
  otto_cueArmed     = true;
  otto_cueLevel     = OTTO_CUE_WATCH;
  otto_cueActive    = true;
  otto_cuePhase     = true;
  otto_cueNextPulse = millis() + OTTO_CUE_PULSE_MS;
  otto_cueCompose();
  otto_drawCue();
}

// Reagent valve panel: big port digit + reagent name
// (1=CLEAVAGE 2=INCORPORATION 3=WASH 4=EMPTY 6=AIR, others "--").
static void ottoPanelReagent(int port) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  if (port == otto_reagentPort) return;        // nothing changed
  otto_reagentPort = port;
  otto_drawReagentPanel();
  otto_panelFlash(OTTO_PAN_REAGENT);
}

// Sample (dispensing) valve panel: big port digit + label
// (1=PARK, 2..7=WELL 1..6, 8=VENT, others "--").
static void ottoPanelSample(int port) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  if (port == otto_samplePort) return;
  otto_samplePort = port;
  otto_drawSamplePanel();
  otto_panelFlash(OTTO_PAN_SAMPLE);
}

// Vacuum panel, aspiration-valve line: "PORT n - WELL m"
// (1=PARK, 2..7=WELL 1..6, 8=PARK, others "--").
static void ottoPanelVacuumPort(int port) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  if (port == otto_vacPort) return;
  otto_vacPort = port;
  otto_drawVacuumPanel();
  otto_panelFlash(OTTO_PAN_VACUUM);
}

// Vacuum panel, solenoid word: OPEN (green) / CLOSED (grey).
static void ottoPanelSolenoid(bool open) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  if (open == otto_solOpen) return;
  otto_solOpen = open;
  otto_drawVacuumPanel();
  otto_panelFlash(OTTO_PAN_VACUUM);
}

// Pump panel: RUNNING (green) / STOPPED (grey). run_secs > 0 starts a live
// countdown of seconds remaining (updated by ottoTick); run_secs <= 0 means
// unknown duration - just the RUNNING word. Re-calling while running (e.g.
// RunPump after StartPump) rearms the countdown without re-flashing.
static void ottoPanelPump(bool running, float run_secs) {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  bool flipped  = (running != otto_pumpRunning);
  bool newTimed = (running && run_secs > 0.0f);
  if (!flipped && !newTimed && otto_pumpTimed == newTimed) {
    return;   // repeated StartPump/StopPump with nothing to change: no-op
  }
  otto_pumpRunning = running;
  otto_pumpTimed   = newTimed;
  if (newTimed) {
    otto_pumpEndMs = millis() + (unsigned long)(run_secs * 1000.0f);
  }
  otto_pumpShownS = -1;
  otto_drawPumpPanel();
  if (flipped) otto_panelFlash(OTTO_PAN_PUMP);
}

// Heartbeat - call at ~2-10 Hz from wait loops (see INTEGRATION.md: Wait()
// should sleep in ~100 ms slices and call this each slice). Updates the
// elapsed clock / progress bar, the pump countdown, and expiring
// change-flashes; costs almost nothing when none of those changed.
// Safe to call before/after any other call (including right after
// ottoShowError, which it answers by rebuilding the dashboard).
static void ottoTick() {
  if (!otto_inited) return;
  otto_rebuildIfNeeded();
  unsigned long now = millis();

  otto_footerUpdate(false);                    // skips unless a second rolled

  if (otto_pumpRunning && otto_pumpTimed) {    // countdown: redraw on change
    long remain = (long)(otto_pumpEndMs - now);
    long secs   = remain > 0 ? (remain + 999L) / 1000L : 0;
    if (secs != otto_pumpShownS) otto_drawPumpCountdown();
  }

  for (uint8_t p = 0; p < 4; p++) {            // expire change-flash rings
    if (otto_flashUntil[p] && (long)(now - otto_flashUntil[p]) >= 0) {
      otto_flashUntil[p] = 0;
      otto_panelRing(p, OTTO_COL_BORDER);
    }
  }

  // Armed cue: refresh the T-Ns countdown each second; strobe at T-0.
  if (otto_cueActive && otto_cueArmed) {
    long remain = (long)(otto_cueFireMs - now);
    if (remain <= 0) {
      otto_cueFire();               // strobe + full rebuild + NOW banner
      return;                       // screen is fresh; done this tick
    }
    long secs = (remain + 999L) / 1000L;
    if (secs != otto_cueShownT) {
      otto_cueCompose();
      otto_drawCue();
    }
  }

  // Pulse the WATCH cue banner (alternate every OTTO_CUE_PULSE_MS).
  if (otto_cueActive && otto_cueLevel == OTTO_CUE_WATCH &&
      (long)(now - otto_cueNextPulse) >= 0) {
    otto_cuePhase     = !otto_cuePhase;
    otto_cueNextPulse = now + OTTO_CUE_PULSE_MS;
    otto_drawCue();
  }
}

// Unmissable full-screen error page (red background, huge banner, message).
// The next ottoTick() / ottoStep*() / ottoPanel*() call rebuilds the whole
// dashboard from the cached panel + step state.
static void ottoShowError(const char* msg) {
  if (!otto_inited) return;
  otto_pill        = OTTO_PILL_ERROR;
  otto_needRebuild = true;

  ottoGfx.fillScreen(OTTO_COL_RED);
  ottoGfx.fillRect(0, 70, OTTO_SCR_W, 110, OTTO_COL_BG);   // contrast banner
  otto_centerIn("! ERROR !", 0, OTTO_SCR_W, 70 + (110 - 8 * 8) / 2, 8,
                OTTO_COL_RED);

  if (msg && msg[0]) {
    uint8_t sz = otto_fitSize(msg, OTTO_SCR_W - 2 * OTTO_MARGIN, 4);
    if (sz >= 2) {
      otto_centerIn(msg, 0, OTTO_SCR_W, 280, sz, OTTO_COL_FG);
    } else {                        // very long message: wrap at size 2
      ottoGfx.setTextWrap(true);
      ottoGfx.setTextSize(2);
      ottoGfx.setTextColor(OTTO_COL_FG);
      ottoGfx.setCursor(OTTO_MARGIN, 280);
      ottoGfx.print(msg);
      ottoGfx.setTextWrap(false);
    }
  }
  otto_centerIn("check serial log", 0, OTTO_SCR_W, 420, 2, OTTO_COL_BG);
}

#endif // OTTO_DISPLAY_H
