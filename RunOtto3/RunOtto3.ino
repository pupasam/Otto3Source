#include </Users/kirbybry/Documents/Otto3/OttoFns/constants.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/LowLevelFns.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/OttoFns.ino>

// for testing, insert this line to stop the loop at that point: stopLoop();

char msg[64];  // static buffer, no heap, reads in 63 chars max

void loop() {

    //ASSERT((vacTime/mLPumpTime) >= 0.75); // vacTime should be sufficient to pump 0.75mL into the well

    if (Serial.available() > 0) {

        size_t len = Serial.readBytesUntil('\n', msg, sizeof(msg) - 1);
        msg[len] = '\0';  // ensure null termination

        // trim end
        while (len > 0 && (msg[len - 1] == '\r' || msg[len - 1] == ' ')) {
            msg[--len] = '\0';
        }

        // compare safely
        if (len >= 16 && strncmp(msg, "BEGIN AUTOMATION", 16) == 0) {
            runAutomation();
        }
    }
}

void runAutomation() {

  Serial.println("Running Otto 3");

  Wait(3);

  //// PART 1, cleavage

  RunPumpLine(WASH, 8, mLPumpTime);

  Serial.println("2 washes");
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 2);
  
  Serial.println("Add cleavage");
  AddSBSReagent(WellLength, SampleWells, VacuumWells, CLEAVAGE, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);
  
  Serial.println("Incubate 6 min");
  Wait(360); // 6 min incubation (should end up around 7.5 mins total per well)

  Serial.println("1 exchange wash");
  ExchangeWash(WellLength, SampleWells, VacuumWells, WASH, 1, 0.75, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 1);

  Serial.println("1 front-back wash");
  AspirateDispenseNestedWellsForwardReverse(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 0, 0, 1);

  Serial.println("1 wash with increased volume");
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 3, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 0, 1); 

  Serial.println("1 front-back wash");
  AspirateDispenseNestedWellsForwardReverse(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 0, 1);

  Serial.println("4 washes");
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 4);

  //// PART 2, incorporation

  // incorporation
  Serial.println("Add incorporation");
  AddSBSReagent(WellLength, SampleWells, VacuumWells, INCORPORATION, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB);
  
  Serial.println("Incubate 6.5 min");
  Wait(390); // 6.5 min incubation (should end up around 7.5-8 mins per well)

  Serial.println("2 exchange washes");
  ExchangeWash(WellLength, SampleWells, VacuumWells, WASH, 1, 0.75, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 2); 

  Serial.println("2 front-back washes");
  AspirateDispenseNestedWellsForwardReverse(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 0, 0, 2);

  Serial.println("1 wash with 5 min incubation and increased volume");
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 3, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 300, 1);

  Serial.println("3 washes with 7 min incubation"); //pass mL pump time for vacTime to frontload dispensation, to include pump duty cycle in incubation time
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 420, 3);

  Serial.println("1 wash with 12 min incubation"); //pass mL pump time for vacTime to frontload dispensation, to include pump duty cycle in incubation time
  AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 720, 1);

  RunPumpLine(WASH, 8, mLPumpTime);
  
  Serial.println("AUTOMATION COMPLETE");
}
