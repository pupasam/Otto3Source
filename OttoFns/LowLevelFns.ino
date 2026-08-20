//constants are referenced in script 'constants'

// I2C selector-valve driver (IDEX MX Series II on TitanEX / RheoLink boards).
// This replaces the old 4-bit BCD parallel GPIO valve control.
#include "RheoLink.h"

// Optional live dashboard on the GIGA Display Shield. The main sketch opts in
// with `#define OTTO_DISPLAY_ENABLED` before including this file; without it
// the hooks below compile to no-ops, so avr:mega and display-less GIGA builds
// are unaffected. OttoDisplay.h master copy lives in ValveControl/firmware/
// OttoDisplay/ -- keep the OttoFns/ copy in step with it.
#ifdef OTTO_DISPLAY_ENABLED
#include "OttoDisplay.h"
#else
inline void ottoDisplayBegin() {}
inline void ottoStepBegin(const char*, unsigned long) {}
inline void ottoStepEnd() {}
inline void ottoPanelReagent(int) {}
inline void ottoPanelSample(int) {}
inline void ottoPanelVacuumPort(int) {}
inline void ottoPanelSolenoid(bool) {}
inline void ottoPanelPump(bool, float) {}
inline void ottoTick() {}
inline void ottoShowError(const char*) {}
#define OTTO_CUE_CALM  0
#define OTTO_CUE_WATCH 1
inline void ottoCue(const char*, uint8_t) {}
inline void ottoCueArm(const char*, unsigned long) {}
inline void ottoCueClear() {}
#endif

// ---- Standalone-panel hooks (touchscreen STOP / abort) ----------------------
// The OttoPanel sketch defines OTTO_PANEL_ENABLED and implements the two touch
// functions in OttoPanelUI.h; every other sketch gets no-op stubs, so serial-
// driven builds (GIGA or Mega) behave exactly as before.
//
// ottoAbortFlag is the emergency-stop latch. It is defined here unconditionally
// (so the guards below compile everywhere) but only the OttoPanel touch poll
// ever sets it. While it is set:
//   - Wait() returns immediately,
//   - StartPump() / RunPump() / Select*Port() / OpenVacuumLine() are no-ops,
// so whatever OttoFns routine is in flight unwinds in milliseconds without
// touching the hardware again. StopPump() and CloseVacuumLine() are never
// guarded — stopping is always allowed. The panel clears the flag itself
// before running its park sequence (see ottoPanelPark in OttoPanelUI.h).
volatile bool ottoAbortFlag = false;

#ifdef OTTO_PANEL_ENABLED
void ottoTouchBegin();   // implemented in OttoPanelUI.h (OttoPanel sketch)
void ottoTouchPoll();
#else
inline void ottoTouchBegin() {}
inline void ottoTouchPoll() {}
#endif

// One RheoLink object per selector valve. Addresses come from constants.ino.
RheoLink reagentValve;
RheoLink sampleValve;
RheoLink vacuumValve;

// Bring up the I2C bus and initialize all three valves. Must be called once
// from setup() before any Select*Port() call. begin() wants the 8-bit write
// address, so we shift the 7-bit constant up by one (it shifts back internally).
void initValves() {
  Wire.begin();
  Wire.setClock(VALVE_I2C_HZ);   // 100 kHz — do NOT raise, 1 MHz hangs the bus.

  uint8_t eR = reagentValve.begin(Wire, ReagentValveAddr7 << 1, VALVE_POS_MIN, VALVE_POS_MAX);
  uint8_t eS = sampleValve.begin(Wire,  SampleValveAddr7  << 1, VALVE_POS_MIN, VALVE_POS_MAX);
  uint8_t eV = vacuumValve.begin(Wire,  VacuumValveAddr7  << 1, VALVE_POS_MIN, VALVE_POS_MAX);

  Serial.print(F("VALVE INIT reagent@0x")); Serial.print(ReagentValveAddr7, HEX);
  Serial.print(F(" begin=")); Serial.print(eR);
  Serial.print(F("  sample@0x"));  Serial.print(SampleValveAddr7, HEX);
  Serial.print(F(" begin=")); Serial.print(eS);
  Serial.print(F("  vacuum@0x"));  Serial.print(VacuumValveAddr7, HEX);
  Serial.print(F(" begin=")); Serial.println(eV);
  // begin() returns 0 on ACK. Non-zero => wiring/address/pull-up problem.
}

void setup() {

  delay(1000); // pause 1 sec before anything else

  Serial.begin(9600); // Initialize serial communication

  ottoDisplayBegin(); // dashboard up before any valve/pump action
  ottoTouchBegin();   // GT911 touch (OttoPanel builds; no-op elsewhere)

  // Selector valves are now on I2C (RheoLink) rather than BCD GPIO.
  initValves();

  pinMode(SolenoidPin, OUTPUT);

  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin,HIGH); // confirm initial state of pump is off
  delay(50);

  
  Serial.println("SAMPLE WELLS");
  for (int i=0;i<WellLength;i++){
    Serial.println(SampleWells[i]);
  }
  Serial.println("VACUUM WELLS");
  for (int i=0;i<WellLength;i++){
    Serial.println(VacuumWells[i]);
  }
  Serial.println("SETUP COMPLETE");
}

void Wait(float secs) {
  // 100 ms slices instead of one long delay, so the dashboard clock,
  // progress bar and pump countdown stay live during holds — and so the
  // touch STOP button is polled ~10x per second (OttoPanel builds).
  if (ottoAbortFlag) return;             // aborted: unwind immediately
  unsigned long total = (unsigned long)(secs * 1000.0f);
  unsigned long t0 = millis();
  while (millis() - t0 < total) {
    unsigned long left = total - (millis() - t0);
    delay(left < 100 ? left : 100);
    ottoTick();
    ottoTouchPoll();                     // may set ottoAbortFlag
    if (ottoAbortFlag) return;
  }
}

// -------- Selector-valve port selection (I2C / RheoLink) --------------------
// These three wrappers keep the exact same signatures as the old BCD versions,
// so every higher-level routine (addReagent, aspirate, calibration, run, ...)
// calls them unchanged. Each now commands the matching I2C valve to `port`.
// set_position() blocks until the valve confirms the position (or times out),
// replacing the old instantaneous digitalWrite of the 4 BCD lines.
void SelectVacuumPort(int port) {
  if (ottoAbortFlag) return;             // aborted: no new hardware action
  vacuumValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
  ottoPanelVacuumPort(port);
}

void SelectReagentPort(int port) {
  if (ottoAbortFlag) return;             // aborted: no new hardware action
  reagentValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
  ottoPanelReagent(port);
}

void SelectSamplePort(int port) {
  if (ottoAbortFlag) return;             // aborted: no new hardware action
  sampleValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
  ottoPanelSample(port);
}

void StartPump() {
  if (ottoAbortFlag) return;             // aborted: pump must not start
  digitalWrite(PumpPin,0); //pump has pull-up transistor
  ottoPanelPump(true, -1);
}

void StopPump() {                        // never guarded: stopping is safe
  digitalWrite(PumpPin,1);
  ottoPanelPump(false, 0);
}

void RunPump(float secs) {
  if (ottoAbortFlag) return;             // aborted: no new hardware action
  StartPump();
  ottoPanelPump(true, secs); // arm the on-screen countdown
  Wait(secs);
  StopPump();
}

void OpenVacuumLine() {
  if (ottoAbortFlag) return;             // aborted: vacuum must not open
  digitalWrite(SolenoidPin,1);
  ottoPanelSolenoid(true);
}

void CloseVacuumLine() {
  digitalWrite(SolenoidPin,0);
  ottoPanelSolenoid(false);
}

// new function name
void stopLoop(){
  delay(1000);
  CloseVacuumLine();
  StopPump();
  ottoStepEnd(); // freeze the step timer at the measured duration
  //RunPumpLine(WASH, 1, .001);
  while (true) {};
  }

float getPumpRuntime(float mL, float mLPumpTime) {
  float pumpRuntime = mLPumpTime * mL;
  return pumpRuntime;
}

void stop() {
  CloseVacuumLine();
  StopPump();
  }
