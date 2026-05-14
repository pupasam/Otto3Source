//constants are referenced in script 'constants'

void setup() {

  delay(1000); // pause 1 sec before anything else

  Serial.begin(9600); // Initialize serial communication

  //set digital GPIO pins to output

  for (int pinInd = 0; pinInd < 4; pinInd++) {
    pinMode(ReagentPins[pinInd], OUTPUT);
  }
  for (int pinInd = 0; pinInd < 4; pinInd++) {
    pinMode(VacuumPins[pinInd], OUTPUT);
  }
  for (int pinInd = 0; pinInd < 4; pinInd++) {
    pinMode(SamplePins[pinInd], OUTPUT);
  }

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

// Use small unsigned type to save RAM. Index 0 is unused (or all-zero).
const uint8_t binaryTable[12][4] = {
  {0,0,0,0},  // 0 (unused)
  {1,0,0,0},  // 1
  {0,1,0,0},  // 2
  {1,1,0,0},  // 3
  {0,0,1,0},  // 4
  {1,0,1,0},  // 5
  {0,1,1,0},  // 6
  {1,1,1,0},  // 7
  {0,0,0,1},  // 8
  {1,0,0,1},  // 9
  {0,1,0,1},  // 10
  {0,1,1,1}   // 11 (default)
};

void SelectPort(int PinArray[], int port) {
  int idx = port;
  if (idx < 1 || idx > 11) {
    idx = 11; // clamp to default mapping
  }
  for (int pinInd = 0; pinInd < 4; pinInd++) {
    digitalWrite(PinArray[pinInd], binaryTable[idx][pinInd]);
  }
}

void SelectVacuumPort(int port) {
  SelectPort(VacuumPins, port);
}

void SelectReagentPort(int port) {
  SelectPort(ReagentPins, port);
}

void SelectSamplePort(int port) {
  SelectPort(SamplePins, port);
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
