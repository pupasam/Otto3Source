// Vacuum diagnostic: solenoid held OPEN, aspiration valve (0x09) dwells
// 20 s on each well port 2..7, forever. Pump stays stopped. Red LED
// blinks once at every port advance. Flash anything else to stop.
#include "src/RheoLink.h"

const int SolenoidPin = 32;
const int PumpPin = 33;
RheoLink vac;

void setup() {
  pinMode(PumpPin, OUTPUT);   digitalWrite(PumpPin, HIGH); // pump stop
  pinMode(SolenoidPin, OUTPUT); digitalWrite(SolenoidPin, LOW);
  pinMode(LEDR, OUTPUT); digitalWrite(LEDR, HIGH);
  Serial.begin(9600);
  delay(1000);
  Wire.begin();
  Wire.setClock(100000);
  uint8_t e = vac.begin(Wire, 0x09 << 1, 1, 8);
  Serial.print("vac begin="); Serial.println(e);
  vac.set_position(1, true, 5000);       // safe port first
  digitalWrite(SolenoidPin, HIGH);       // vacuum open
  delay(5000);                           // line fill
}

void loop() {
  for (int p = 2; p <= 7; p++) {
    digitalWrite(LEDR, LOW); delay(200); digitalWrite(LEDR, HIGH);
    vac.set_position(p, true, 5000);
    Serial.print("port "); Serial.println(p);
    delay(20000);
  }
}
