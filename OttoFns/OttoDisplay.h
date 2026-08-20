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
#define OTTO_PAN_W       400     // panel grid: 2 x 2
#define OTTO_PAN_H       176
#define OTTO_PAN_TOP      64     // first panel row y
#define OTTO_FOOT_Y      416     // footer 416..479
#define OTTO_FOOT_H       64
#define OTTO_TIME_X       16     // footer time text "MM:SS / MM:SS", size 3
#define OTTO_TIME_Y      436
#define OTTO_TIME_W      252
#define OTTO_BAR_X       280     // footer progress bar
#define OTTO_BAR_Y       428
#define OTTO_BAR_W       500
#define OTTO_BAR_H        36
#define OTTO_BAR_PAD       4     // gap between frame and fill
#define OTTO_FLASH_MS    700     // change-flash ring lifetime
#define OTTO_MARGIN       12     // min side margin for centered text

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

// Panel origins, indexed by OTTO_PAN_*.
static const int16_t OTTO_PAN_X[4] = {0, 400, 0, 400};
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
// whole-second elapsed value is unchanged (unless force).
static void otto_footerUpdate(bool force) {
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
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_SCR_W, OTTO_FOOT_H, OTTO_COL_BG);
  ottoGfx.fillRect(0, OTTO_FOOT_Y, OTTO_SCR_W, 1, OTTO_COL_BORDER);
  otto_barFrame     = false;
  otto_lastTime[0]  = '\0';
  otto_lastElapsedS = (unsigned long)-1;
  otto_footerUpdate(true);
}

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
