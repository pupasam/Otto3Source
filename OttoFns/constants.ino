

/////////////////////////// RUNTIME CONSTANTS YOU PROBABLY DO WANT TO EDIT/CHECK

float mLPumpTime = 15.4; // amount of time (secs) required to pump 1 mL

float ReagentLineVolume = 0.64; // mL required to move an air bubble from the reagent valve to the sample valve

float SampleLineVolume = 0.25; // sample line volume (mL), calculated based on length (nearest inch) and ID of line
float SampleNeedleVolume = 0.0427; // dispensation needle dead volume based on needle guage
float adjustSampleVolMicro = 40; //in MICROliters -- can be +/-, final adjustment to calculated sample line volume such that addReagent has expected function

// SampleWells and VacuumWells reference the PORTS on the valve switchers corresponding to each well
//length of SampleWells and VacuumWells MUST be the same
// Ports are 2-indexed (port 1 is reserved)
int SampleWells[] = {2,3,4,5,6,7} ;//,2,3,4,5,6}; //each position in SampleWells and VacuumWells correspond to PORTS for the same well
int VacuumWells[] = {2,3,4,5,6,7}; //each position in SampleWells and VacuumWells correspond to PORTS for the same well

/////////////////////////// RUNTIME CONSTANTS YOU PROBABLY DON'T WANT TO EDIT

//LIMITATION -- THIS HAS TO BE LESS THAN SBS VOLUME!
float SampleLineTotalVolume = SampleLineVolume + SampleNeedleVolume + (adjustSampleVolMicro/1000);

int WellLength = sizeof(SampleWells) / sizeof(SampleWells[0]); //length of SampleWells and VacuumWells must be the same

int SafeVacA = 1; // a port that is not currently used before the vacuum array
int SafeVacB = 8; // a port that is not currently used after the vacuum array
int VentPort = 8; // the port that corresponds to the vent port of the sample array 

float airTime = 6; // amount of time (secs) for air bubble gap between sensitive reagents // should be >= pump time for proper addReagent functionality

float fillTime = 5; // amount of time (secs) needed to fully fill vacuum line at sample valve switch following line opening

//vacTime should be between 7-15 seconds
float vacTime = 12;//10.5; // amount of time (secs) required to fully aspirate a well of maximum input volume

float SBSVolume = 0.99; // mL to dispense per well for SBS reagents // it will have SBSVolume - SampleNeedleVolume sitting in it for ~1min


/////////////////////////// GPIO PIN AND REAGENT PORT CONSTANTS

enum Reagent {
  INCORPORATION,
  CLEAVAGE,
  NUCLEAR,
  WASH,
  IMAGE, //deprecated, could be 2XSSC
  AIR
};

int getReagentPort(Reagent r) {
  switch (r) {
    case CLEAVAGE: return 1;
    case INCORPORATION: return 2;
    case WASH: return 3;
    // for OTTO2 wash is 4 and imaging buffer is 3
    case AIR: return 6; // empty port for air
    default: return 4;  // fallback if needed, empty
  }
}

int SolenoidPin = 32;
int PumpPin = 33;
int ReagentPins[4] = {36, 37, 38, 39};
int VacuumPins[4] = {42, 43, 44, 45};
int SamplePins[4] = {48, 49, 50, 51};


