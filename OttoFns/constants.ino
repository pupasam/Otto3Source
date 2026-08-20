

/////////////////////////// RUNTIME CONSTANTS YOU PROBABLY DO WANT TO EDIT/CHECK

float mLPumpTime = 18.8; // amount of time (secs) required to pump 1 mL

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
float vacTime = 15;//10.5; // amount of time (secs) required to fully aspirate a well of maximum input volume

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

// Pump and solenoid stay on plain GPIO (they are NOT I2C valves).
int SolenoidPin = 32;
int PumpPin = 33;

// ---- I2C selector-valve addressing (RheoLink / IDEX MX Series II) ----------
// The three selector valves are now driven over I2C instead of 4-bit BCD GPIO.
// Each value below is the 7-bit I2C address as printed by the i2c_scanner sketch.
//
// IMPORTANT: every valve must be given its own UNIQUE even 8-bit write address
// BEFORE it goes on the shared bus, using the address_change sketch, one valve
// at a time, followed by a power-cycle:
//     factory 0x0E  -> 7-bit 0x07   (reagent, as shipped)
//            0x10   -> 7-bit 0x08   (sample)
//            0x12   -> 7-bit 0x09   (vacuum)
// If you only have one addressed valve on the bench, temporarily point all three
// constants at the same address to exercise the code path.
uint8_t ReagentValveAddr7 = 0x07;
uint8_t SampleValveAddr7  = 0x08;
uint8_t VacuumValveAddr7  = 0x09;

// MX Series II is a 10-position selector; ports are 1..10.
const uint8_t VALVE_POS_MIN = 1;
const uint8_t VALVE_POS_MAX = 10;
// I2C bus speed. MUST be 100 kHz — 1 MHz hangs the RheoLink bus.
const uint32_t VALVE_I2C_HZ = 100000;


