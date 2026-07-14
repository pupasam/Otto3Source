// One-time setup calibration script for Otto3
// all reagent slots should be loaded with PR2 reservoirs

#include "src/constants.h"
#include "src/LowLevelFns.h"
#include "src/OttoFns.h"

void loop() {
  
  //DispenseLines(WellLength, SampleWells,WASH,mLPumpTime); //Step 1, assess & calibrate dispensation volume

  //INSTRUCTION: total delta between well dispensation volumes should be <50uL
  //INSTRUCTION: empty plate and re-run Step 1, altering and saving mLPumpTime in 'constants' each time, until average dispensation = 1mL
  
  //INSTRUCTION: Step 2.1, use tubing ID and length to computationally set SampleLineVolume in constants
  //INSTRUCTION: Step 2.2, use needle guauge chart like below to computationally set SampleNeedleVolume in constants
  //https://www.hamiltoncompany.com/knowledge-base/article/needle-gauge-chart

  //AddSBSReagent(WellLength, SampleWells, VacuumWells, INCORPORATION, SBSVolume, ReagentLineVolume, SampleLineTotalVolume, VentPort, mLPumpTime, vacTime, airTime, fillTime, SafeVacA, SafeVacB); //Step 3
  
  //INSTRUCTION: run and re-run step 3 first to calibrate reagent line volume, editing and saving 'ReagentLineVolume' in constants until leading edge of air bubble dissappears into sample valve before being split into sample lines
  //INSTRUCTION: run and re-run step 3 then to calibrate sample line volume, editing and saving 'adjustSampleVolMicro' in constants until lagging edge of air bubble dissappears into sample needles at end of function
  
  stopLoop();stopLoop();

}
