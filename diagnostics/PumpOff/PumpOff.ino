// Holds the pump line in its STOP state (pin 33 HIGH / released to 5 V) forever.
const int PumpPin = 33;
void setup() {
  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin, HIGH);  // stop
}
void loop() {}
