#include </Users/kirbybry/Documents/Arduino/OttoFns/constants.ino>
#include </Users/kirbybry/Documents/Arduino/OttoFns/LowLevelFns.ino>
#include </Users/kirbybry/Documents/Arduino/OttoFns/OttoFns.ino>

void ReagentLoop() {
  for (int i=1; i<9; i++) {
    SelectReagentPort(i);
    delay(1000);
  }
}

void SampleLoop() {
  for (int i=1; i<9; i++) {
    SelectSamplePort(i);
    delay(1000);
  }
}

void VacuumLoop() {
  for (int i=1; i<9; i++) {
    SelectVacuumPort(i);
    delay(1000);
  }
}

void SolenoidLoop() {
  for (int i=0; i<3; i++) {
    OpenVacuumLine();
    delay(2000);
    CloseVacuumLine();
    delay(2000);
  }
}

void loop() {

  ReagentLoop();
  delay(3000);
  SampleLoop();
  delay(3000);
  VacuumLoop();
  delay(3000);
  SolenoidLoop();
  delay(3000);
  RunPumpLine(WASH, 8, 5);
  delay(3000);
  stopLoop();
  stopLoop();

}
