//constants are referenced in script 'constants'

// I2C selector-valve driver (IDEX MX Series II on TitanEX / RheoLink boards).
// This replaces the old 4-bit BCD parallel GPIO valve control.
#include "RheoLink.h"

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
  delay(secs * 1000);
}

// -------- Selector-valve port selection (I2C / RheoLink) --------------------
// These three wrappers keep the exact same signatures as the old BCD versions,
// so every higher-level routine (addReagent, aspirate, calibration, run, ...)
// calls them unchanged. Each now commands the matching I2C valve to `port`.
// set_position() blocks until the valve confirms the position (or times out),
// replacing the old instantaneous digitalWrite of the 4 BCD lines.
void SelectVacuumPort(int port) {
  vacuumValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
}

void SelectReagentPort(int port) {
  reagentValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
}

void SelectSamplePort(int port) {
  sampleValve.set_position((uint8_t)port, true, RheoLink_TIMEOUT);
}

void StartPump() {
  digitalWrite(PumpPin,0); //pump has pull-up transistor
}

void StopPump() {
  digitalWrite(PumpPin,1);
}

void RunPump(float secs) {
  StartPump();
  Wait(secs);
  StopPump();
}

void OpenVacuumLine() {
  digitalWrite(SolenoidPin,1);
}

void CloseVacuumLine() {
  digitalWrite(SolenoidPin,0);
}

// new function name
void stopLoop(){
  delay(1000);
  CloseVacuumLine();
  StopPump();
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
