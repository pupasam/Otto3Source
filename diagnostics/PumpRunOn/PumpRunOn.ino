// Runs the pump CONTINUOUSLY (pin 33 LOW) for cassette tensioning / flow
// troubleshooting. Red LED on = pump commanded on. Flash PumpOff or press
// the pump's front-panel stop to halt.
const int PumpPin = 33;
void setup() {
  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin, LOW);   // pump on
  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDR, LOW);      // LED on
}
void loop() {}
