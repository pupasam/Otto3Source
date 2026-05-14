// Post-run shutdown script for Otto3
// all reagent slots should be loaded with water reservoirs

#include </Users/kirbybry/Documents/Otto3/OttoFns/constants.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/LowLevelFns.ino>
#include </Users/kirbybry/Documents/Otto3/OttoFns/OttoFns.ino>

void loop() {

  //fullRinse(WellLength, SampleWells, mLPumpTime); //wash all lines

  stopLoop();stopLoop();

  //INSTRUCTION: close vacuum line
  //INSTRUCTION: turn off cold block, pump, and valves
  //INSTRUCTION: turn off heating box system

  //INSTRUCTION: load manifold into clean, empty well plate and put away

}
