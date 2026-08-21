// Pump control-path test. Pulses PumpPin (33, active-LOW through BSS138 ch.3
// to the MINIPULS 3 barrier strip) 5 s ON / 5 s OFF forever. The GIGA's red
// LED mirrors the command: LED ON = pump commanded ON. No valves, no I2C.
const int PumpPin = 33;

void setup() {
  Serial.begin(9600);
  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin, HIGH);   // pump off (line released to 5 V)
  pinMode(LEDR, OUTPUT);
  digitalWrite(LEDR, HIGH);      // GIGA LEDs are active-LOW; HIGH = off
}

void loop() {
  Serial.println("PUMP ON  (pin 33 LOW)");
  digitalWrite(PumpPin, LOW);  digitalWrite(LEDR, LOW);
  delay(5000);
  Serial.println("PUMP OFF (pin 33 HIGH)");
  digitalWrite(PumpPin, HIGH); digitalWrite(LEDR, HIGH);
  delay(5000);
}
