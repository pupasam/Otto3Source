// Pre-run test scripts for Otto3
// all reagent slots should be loaded with PR2 reservoirs
// manifold should be loaded onto empty, unseeded test plate

#include </Users/kirbybry/Documents/Otto3/OttoFns/constants.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/LowLevelFns.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/OttoFns.ino>

void loop() {

  //stopLoop();stopLoop();

  //RunPumpLine(WASH, 8, 20); //Step 1.1, prime wash line
  //RunPumpLine(CLEAVAGE, 8, 20); //Step 1.2, prime cleavage line
  //RunPumpLine(INCORPORATION, 8, 20); //Step 1.3, prime incorporation line

  //fullRinse(WellLength, SampleWells, mLPumpTime); //Step 2, prime all lines

  //INSTRUCTION: empty out test plate for Step 3

  //DispenseLines(WellLength, SampleWells,WASH,mLPumpTime); //Step 3, assess & calibrate dispensation volume

  //INSTRUCTION: total delta between well dispensation volumes should be <50uL
  //INSTRUCTION: empty plate and re-run Step 3, altering and saving mLPumpTime in 'constants' each time, until average dispensation = 1mL
  
  //DispenseLines(WellLength, SampleWells,WASH,mLPumpTime);AspirateLines(WellLength, VacuumWells,  vacTime, SafeVacA, SafeVacB, fillTime); //Step 4, 

  //AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, WASH, 1, mLPumpTime, mLPumpTime, SafeVacA, SafeVacB, fillTime, 0, 1); //Step 5
  //INSTRUCTION: validate general vacuum performance first on unseeded plate, then re-run Step 5 with a cell-seeded test plate for precise assessment
  //INSTRUCTION: total remaining volume between well dispensation volumes on seeded plate should be <20uL/well

  AddSBSReagent(WellLength, SampleWells, VacuumWells, CLEAVAGE, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB); //Step 6
  //INSTRUCTION: validate that leading edge of air bubble dissappears into sample valve before being split into sample lines, then lagging end of bubble dissappears into needles at end of sample lines
  //INSTRUCTION: validate that ~SBSVolume has been dispensed to each well following operation
  
  stopLoop();stopLoop();

}