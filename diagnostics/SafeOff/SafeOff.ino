// Bench-safe idle: pump line held in STOP, solenoid held CLOSED, forever.
void setup() {
  pinMode(33, OUTPUT); digitalWrite(33, HIGH); // pump stop
  pinMode(32, OUTPUT); digitalWrite(32, LOW);  // solenoid closed
}
void loop() {}
