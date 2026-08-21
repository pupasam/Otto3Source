// Solenoid control-path test: pulses SolenoidPin (32, active-HIGH -> opto
// relay IN, relay switches the 12 V loop) 2 s ON / 2 s OFF forever.
// Red LED mirrors the command. Pump pin left in STOP.
const int SolenoidPin = 32;
const int PumpPin = 33;
void setup() {
  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin, HIGH);   // pump stop
  pinMode(SolenoidPin, OUTPUT);
  digitalWrite(SolenoidPin, LOW);
  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDR, HIGH);
}
void loop() {
  digitalWrite(SolenoidPin, HIGH); digitalWrite(LEDR, LOW);   // open + LED on
  delay(2000);
  digitalWrite(SolenoidPin, LOW);  digitalWrite(LEDR, HIGH);  // close + LED off
  delay(2000);
}
