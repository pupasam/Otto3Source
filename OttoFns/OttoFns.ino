#define ASSERT(x) if(!(x)) { Serial.println(#x); while(1); }

void RunPumpLine(Reagent reagentName, int SamplePort, float secs) {
  // current runtime is runtime + 0.25 secs
  int ReagentPort = getReagentPort(reagentName);
  SelectReagentPort(ReagentPort);
  delay(20); //100
  SelectSamplePort(SamplePort);
  delay(30); //100
  RunPump(secs);
  delay(50); //150
}

void DispenseLines(int WellLength, int Wells[], Reagent reagentName, float pumpTime) {
  for (int i = 0; i <= WellLength-1; i++) {
    //Serial.println(i);
    RunPumpLine(reagentName, Wells[i], pumpTime);
    //delay(100);
  }
}

void AspirateLine(int VacuumPort, float VacTime) {
  SelectVacuumPort(VacuumPort);
  Wait(VacTime);
}

void AspirateLineFull(int VacuumPort, float VacTime, int VacStartPort, float FillTime) {
  OpenVacuumLine();
  SelectVacuumPort(VacStartPort);
  Wait(FillTime);
  SelectVacuumPort(VacuumPort);
  Wait(VacTime);
  CloseVacuumLine();
  SelectVacuumPort(VacStartPort);
}

void AspirateLines(int WellLength, int Wells[], float vacTime, int VacStartPort, int VacEndPort, float FillTime) {

  SelectVacuumPort(VacStartPort);

  OpenVacuumLine();

  Wait(FillTime); //for vacuum line to fill

  for (int i = 0; i < WellLength; i++) {
    int well = Wells[i];
    AspirateLine(Wells[i], vacTime);
  }
  SelectVacuumPort(VacEndPort);
  CloseVacuumLine();
}

// Assumes vacuum line is ALREADY open
void AspirateDispenseNestedWell(int SampleWells[], int VacuumWells[], int position, Reagent reagentName, float PumpTime, float VacTime) {
  SelectVacuumPort(VacuumWells[position]); //vacuum well
  if (position > 0) {
    RunPumpLine(reagentName, SampleWells[position - 1], PumpTime);
  }
  else {
    Wait(PumpTime);
  }
}

// Assumes vacuum line is ALREADY open
void AspirateDispenseNestedWellReverse(int SampleWells[], int VacuumWells[], int position, Reagent reagentName, float PumpTime, float VacTime, int WellLength) {
  SelectVacuumPort(VacuumWells[position - 1]); //vacuum well
  if (position < WellLength) {
    RunPumpLine(reagentName, SampleWells[position], PumpTime);
  }
  else {
    Wait(PumpTime);
  }
}

// if PumpAmt < VacTime, PumpAmt will be overridden and VacTime-worth will be dispensed per well
// if PumpAmt > VacTime, VacTime-worth will first be dispensed per well and then PumpAmt time - VacTime will be dispensed per well
void AspirateDispenseNestedWells(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float PumpAmt, float mLPumpTime, float VacTime, int VacStartPort, int VacEndPort, float FillTime, bool open, bool close) {

  SelectVacuumPort(VacStartPort);

  int ReagentPort = getReagentPort(reagentName);
  SelectReagentPort(ReagentPort);
  SelectSamplePort(SampleWells[0]);

  if (open) {
    OpenVacuumLine();
    Wait(FillTime); //for vacuum line to fill
  }

  float PumpTime = PumpAmt * mLPumpTime;
  float PostPumpTime = max(PumpTime - VacTime,0);

  for (int i = 0; i < WellLength; i++) {
    AspirateDispenseNestedWell(SampleWells, VacuumWells, i, reagentName, VacTime, VacTime);
  }

  SelectVacuumPort(VacEndPort);

  if (close) {
    CloseVacuumLine();
  }

  RunPumpLine(reagentName, SampleWells[WellLength-1], VacTime); //runs once for the final well

  if (PostPumpTime > 0) {
    SelectSamplePort(SampleWells[0]);
    Wait(0.5);
    for (int position = 0; position < WellLength; position++) {
      RunPumpLine(reagentName, SampleWells[position], PostPumpTime);
    }

  SelectSamplePort(SampleWells[0]);
  }
}

void AspirateDispenseNestedWellsReverse(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float PumpAmt, float mLPumpTime, float VacTime, int VacStartPort, int VacEndPort, float FillTime, bool open, bool close) {
  
  SelectVacuumPort(VacEndPort);

  float PumpTime = PumpAmt * mLPumpTime;
  float PostPumpTime = max(PumpTime - VacTime,0);

  int ReagentPort = getReagentPort(reagentName);
  SelectReagentPort(ReagentPort);
  SelectSamplePort(SampleWells[WellLength-1]);

  Wait(1);

  if (open) {
    OpenVacuumLine();
    Wait(FillTime); //for vacuum line to fill
  }

  for (int i = WellLength; i > 0; i--) {
    AspirateDispenseNestedWellReverse(SampleWells, VacuumWells, i, reagentName, VacTime, VacTime, WellLength);
  }

  SelectVacuumPort(VacStartPort);

  if (close) {
  CloseVacuumLine();
  }

  RunPumpLine(reagentName, SampleWells[0], VacTime); //runs once for the final well

  if (PostPumpTime > 0) {
    SelectSamplePort(SampleWells[WellLength-1]);
    Wait(0.5);
    for (int position = WellLength; position >= 0; position--) {
      RunPumpLine(reagentName, SampleWells[position], PostPumpTime);
    }
  }
}

void AspirateDispenseNestedWellsIncubation(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float PumpAmt, float mLPumpTime, float VacTime, int VacStartPort, int VacEndPort, float FillTime, float PauseTime, int times) {
  
  for (int i = 1; i <= times; i++) {
    Serial.println(i);
    AspirateDispenseNestedWells(WellLength, SampleWells, VacuumWells, reagentName, PumpAmt, mLPumpTime, VacTime, VacStartPort, VacEndPort, FillTime, true, true); //always open and close vac line 
    SelectSamplePort(SampleWells[0]);
    Wait(FillTime);
    SelectVacuumPort(VacStartPort);
    if ((PauseTime-FillTime) > 0) {Wait(PauseTime-FillTime);} // wait for remaining incubation
  }
}

void AspirateDispenseNestedWellsForwardReverse(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float PumpAmt, float mLPumpTime, float VacTime, int VacStartPort, int VacEndPort, float FillTime, float PauseTimeEnd, float PauseTimeBeginning, int times) {
  for (int i = 1; i <= times; i++) {
      bool open_vacF; bool close_vacF; bool open_vacR; bool close_vacR; float fillF;
      if (i == 1 && times > 1) { // initial and times > 1
        open_vacF = true;
        close_vacF = false;
        open_vacR = false;
        close_vacR = false;
        fillF = FillTime;
      }
      else if (i == times && times == 1) { // final and times = 1
        open_vacF = true;
        close_vacF = false;
        open_vacR = false;
        close_vacR = true;
        fillF = FillTime;
      }
      else if (i == times && times > 1) { // final and times > 1
        open_vacF = false;
        close_vacF = false;
        open_vacR = false;
        close_vacR = true;
        fillF = 0;
      }
      else { // intermediate
        open_vacF = false;
        close_vacF = false;
        open_vacR = false;
        close_vacR = false;
        fillF = 0;
      }
    AspirateDispenseNestedWells(WellLength, SampleWells, VacuumWells, reagentName, PumpAmt, mLPumpTime, VacTime, VacStartPort, VacEndPort, fillF, open_vacF, close_vacF);
    Wait(PauseTimeEnd); //pause at end of forward loop to allow last well to incubate in wash
    AspirateDispenseNestedWellsReverse(WellLength, SampleWells, VacuumWells, reagentName, PumpAmt, mLPumpTime, VacTime, VacStartPort, VacEndPort, 0, open_vacR, close_vacR);
    Wait(PauseTimeBeginning); //pause in secs at end of backward loop to allow first well to incubate in wash
  }
}

void ExchangeWash(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float exchangeAmt, float dispenseAmt, float mLPumpTime, float vacTime, int SafeVacA, int SafeVacB, float fillTime, int times) {
  // not a true exchange, but dilutes the reagent before performing a wash
  for (int i=1;i<=times;i++) {
    DispenseLines(WellLength, SampleWells,reagentName,exchangeAmt*mLPumpTime);
    AspirateDispenseNestedWellsIncubation(WellLength, SampleWells, VacuumWells, reagentName, dispenseAmt, mLPumpTime, vacTime, SafeVacA, SafeVacB, fillTime, 0, 1);
  }
}

void calibrateReagentRuntime(float ReagentLineVolume, float mLPumpTime, float AirTime, int VentPort) {

  // prime with separate button with RunPumpLine(WASH, VentPort, airTime*5);

  float reagRuntime = getPumpRuntime(ReagentLineVolume, mLPumpTime); // convert mL to seconds of runtime for reagent line
  RunPumpLine(AIR, VentPort, AirTime); // prime the line with bubble
  RunPumpLine(WASH, VentPort, max(0, (reagRuntime-AirTime))); // prime the remainder with wash
}

void testSampleRuntime(int SampleWells[], float SampleLineTotalVolume, float mLPumpTime, float AirTime, int VentPort) {

  // prime with separate button with RunPumpLine(WASH, VentPort, airTime*5);
  
  Serial.println(SampleLineTotalVolume);
  Serial.println("-");

  float reagRuntime = getPumpRuntime(ReagentLineVolume, mLPumpTime); // convert mL to seconds of runtime for reagent line

  float sampleRuntime = getPumpRuntime(SampleLineTotalVolume, mLPumpTime); // convert mL to seconds of runtime for sample line
  
  RunPumpLine(AIR, VentPort, AirTime); // prime the reagent line with bubble
  RunPumpLine(WASH, VentPort, max(0, (reagRuntime-AirTime))); // prime the remainder reagent with wash

  Wait(3);

  RunPumpLine(WASH, SampleWells[0], sampleRuntime); // run the 1st sample line with wash

}

// will dispense 4mL total into sample well plate, then aspirate; runs 2X
//conduct first with bleach water then pure water
void fullRinse(int WellLength, int SampleWells[], float mLPumpTime) {
  
  RunPumpLine(WASH, 8, mLPumpTime);
  
  DispenseLines(WellLength, SampleWells, CLEAVAGE, mLPumpTime);
  DispenseLines(WellLength, SampleWells, INCORPORATION, mLPumpTime);
  DispenseLines(WellLength, SampleWells, IMAGE, mLPumpTime);
  DispenseLines(WellLength, SampleWells, WASH, mLPumpTime);
  AspirateLines(WellLength, VacuumWells, mLPumpTime, SafeVacA, SafeVacB, fillTime);

  DispenseLines(WellLength, SampleWells, CLEAVAGE, mLPumpTime);
  DispenseLines(WellLength, SampleWells, INCORPORATION, mLPumpTime);
  DispenseLines(WellLength, SampleWells, IMAGE, mLPumpTime);
  DispenseLines(WellLength, SampleWells, WASH, mLPumpTime);
  AspirateLines(WellLength, VacuumWells, mLPumpTime, SafeVacA, SafeVacB, fillTime);

}

// the case in which the amount pumped is less than the total line dead volume
// SBS fluid will remain behind in the sample lines
void AddSBSReagentShort(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float SBSVol, float ReagentLineVentVolume, float SampleLineTotalVolume, int VentPort, float mLPumpTime, float VacTime, float AirTime, float FillTime, int VacStartPort, int VacEndPort, float ventRuntime) {
  RunPumpLine(reagentName, VentPort, ventRuntime*2);
  RunPumpLine(AIR, VentPort, airTime); // dispense bubble
  AspirateDispenseNestedWells(WellLength, SampleWells, VacuumWells, WASH, SBSVol, mLPumpTime, VacTime, VacStartPort, VacEndPort, FillTime, true, true);
  RunPumpLine(WASH, VentPort, ventRuntime); //wash out vent line
  SelectSamplePort(SampleWells[0]);
  SelectVacuumPort(VacStartPort);
}

void AddSBSReagentMulti(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float SBSVol, float ReagentLineVentVolume, float SampleLineTotalVolume, int VentPort, float mLPumpTime, float VacTime, float AirTime, float FillTime, int VacStartPort, int VacEndPort, float ventRuntime, float SBSWellRuntime, float SamplePrimeRuntime, float totalPumpTime, float pumpTimeBeforeBubble) {
  
  float BubbleTimePerSampleLine = AirTime/WellLength; // dividing up the bubble between the sample lines
  int bubbleIdx = (int)floor(max(0,pumpTimeBeforeBubble)/SBSWellRuntime);
  float AirTimeBubbleIdx = AirTime;
  float pumpTimeBeforeBubbleIdx = ((pumpTimeBeforeBubble/SBSWellRuntime) - bubbleIdx) * SBSWellRuntime; // remainder of pump time
  float pumpTimeAfterBubbleIdx = SBSWellRuntime - AirTime - pumpTimeBeforeBubbleIdx;

  float bubbleTimeNextIdx;
  float pumpTimeNextIdx;

  float addlVacTime = max(VacTime-SBSWellRuntime,0); //we are pumping our initial reagent prime into existing fluid so need to vac for the whole time

  if (pumpTimeAfterBubbleIdx <= 0) { //we run the bubble spanned across 2 idx's
    bubbleTimeNextIdx = pumpTimeAfterBubbleIdx * -1; 
    pumpTimeNextIdx = SBSWellRuntime - bubbleTimeNextIdx;
    AirTimeBubbleIdx = AirTime - bubbleTimeNextIdx;
  }

  // set up vacuum line
  SelectVacuumPort(VacStartPort);
  OpenVacuumLine();
  Wait(max(0, FillTime-ventRuntime)); // wait for any necessary fill

  // we want to purge the old stuck reagent all the way through the line
  RunPumpLine(reagentName, VentPort, ventRuntime*2); //prime reagent line with > ventRuntime to ensure completion
  
  for (int w = 0; w<WellLength; w++) {
    RunPumpLine(reagentName, SampleWells[w], SamplePrimeRuntime * 1.2); //prime sample lines with a little extra to make sure
  }

  SelectSamplePort(SampleWells[0]); //select first well
  SelectVacuumPort(VacuumWells[0]); //vacuum first well
  Wait(VacTime);

  for (int i = 0; i<bubbleIdx; i++) {
    SelectVacuumPort(VacuumWells[i + 1]); //vacuum next well
    RunPumpLine(reagentName, SampleWells[i], SBSWellRuntime); //dispense to current well
    Wait(addlVacTime); //finish vacuuming
  }

  if (bubbleIdx + 1 <= WellLength-1) {SelectVacuumPort(VacuumWells[bubbleIdx + 1]);} //vacuum next well
  else {SelectVacuumPort(VacEndPort);}

  ottoCue("AIR BUBBLE ENTERING LINE NEXT", OTTO_CUE_CALM); // cue: air draw starts at the end of this segment

  RunPumpLine(reagentName, SampleWells[bubbleIdx], pumpTimeBeforeBubbleIdx);

  // cue: the bubble's LEADING edge forms at the reagent valve now and
  // disappears into the sample (dispensing) valve after ~ventRuntime seconds
  // of pumping (ReagentLineVolume worth) - Step 8(a) watch window
  static char otto_cueBuf[44];
  snprintf(otto_cueBuf, sizeof(otto_cueBuf),
           "EYES ON DISP VALVE - LEADING EDGE ~%ds", (int)(ventRuntime + 0.5f));
  ottoCue(otto_cueBuf, OTTO_CUE_WATCH);

  RunPumpLine(AIR, SampleWells[bubbleIdx], AirTimeBubbleIdx);// + pumpTimeBeforeBubble); //dispense bubble to bubble well

  // if the bubble concludes vs extends over multiple wells
  int continueIdx;
  if (pumpTimeAfterBubbleIdx > 0) {
    continueIdx = 0;
      RunPumpLine(WASH, SampleWells[bubbleIdx], pumpTimeAfterBubbleIdx); //dispense post-bubble wash to bubble well
      Wait(addlVacTime); //finish vacuuming bubble well
    }
  else {
    continueIdx = 1;
    Wait(addlVacTime); //finish vacuuming bubble well
    if (bubbleIdx + 2 <= WellLength-1) {SelectVacuumPort(VacuumWells[bubbleIdx + 2]);} //vacuum next well
    else {SelectVacuumPort(VacEndPort);}
    RunPumpLine(AIR, SampleWells[bubbleIdx + 1], bubbleTimeNextIdx); // dispense remaining bubble
    RunPumpLine(WASH, SampleWells[bubbleIdx + 1], pumpTimeNextIdx); // dispense rest of wash well
    if (bubbleIdx + 2 <= WellLength-1) {Wait(addlVacTime);} //finish vacuuming after bubble well
  }

  // wells after bubble well excepting last well
  for (int ii = bubbleIdx + 1 + continueIdx; ii<WellLength-1; ii++) {
    Serial.print("simple wells after complex well ");Serial.println(ii);
    SelectVacuumPort(VacuumWells[ii + 1]); //vacuum next well
    RunPumpLine(WASH, SampleWells[ii], SBSWellRuntime); //dispense to current well
    Wait(addlVacTime); //finish vacuuming
  }

  SelectVacuumPort(VacEndPort);
  CloseVacuumLine();
  if ((bubbleIdx + 1) < WellLength-1) {
    RunPumpLine(WASH, SampleWells[WellLength-1], SBSWellRuntime);} //dispense to final well with wash
  
  Wait(5); // let bubble settle

  for (int w=WellLength-1; w>=0; w--) {
    RunPumpLine(WASH, SampleWells[w], BubbleTimePerSampleLine); //split bubble evenly between sample lines
  }

  // cue: each purge segment below consumes ~SampleLineTotalVolume, pushing
  // that line's bubble tail out through its needle - Step 8(b) watch window
  ottoCue("EYES ON NEEDLES - TRAILING EDGE", OTTO_CUE_WATCH);

  for (int ww=0; ww<WellLength; ww++) {
    RunPumpLine(WASH, SampleWells[ww], SamplePrimeRuntime); //purge remainder of sample line
  }

  RunPumpLine(WASH, VentPort, ventRuntime); //wash out vent line
  SelectSamplePort(SampleWells[0]);
  SelectVacuumPort(VacStartPort); 
}


void AddSBSReagent(int WellLength, int SampleWells[], int VacuumWells[], Reagent reagentName, float SBSVol, float ReagentLineVentVolume, float SampleLineTotalVolume, int VentPort, float mLPumpTime, float VacTime, float AirTime, float FillTime, int VacStartPort, int VacEndPort) {

  ottoCue("PRIMING - NO NEED TO WATCH YET", OTTO_CUE_CALM); // cue: run start

  float ventRuntime = getPumpRuntime(ReagentLineVentVolume, mLPumpTime); // convert mL to seconds of runtime for vent
  float SBSWellRuntime = getPumpRuntime(SBSVol - SampleLineTotalVolume, mLPumpTime); // convert mL to seconds of pre-purge reagent runtime per well
  float SamplePrimeRuntime = getPumpRuntime(SampleLineTotalVolume, mLPumpTime); // convert mL to seconds of runtime for sample line
  float totalPumpTime = SBSWellRuntime * WellLength; // the total time the pump will be running before purging reagent lines
  float pumpTimeBeforeBubble = totalPumpTime - ventRuntime;

  if (pumpTimeBeforeBubble>0) {
    AddSBSReagentMulti(WellLength, SampleWells, VacuumWells, reagentName, SBSVol, ReagentLineVentVolume, SampleLineTotalVolume, VentPort, mLPumpTime, VacTime, AirTime, FillTime, VacStartPort, VacEndPort, ventRuntime, SBSWellRuntime, SamplePrimeRuntime, totalPumpTime, pumpTimeBeforeBubble);
  }
  else {
    AddSBSReagentShort(WellLength, SampleWells, VacuumWells, reagentName, SBSVol, ReagentLineVentVolume, SampleLineTotalVolume, VentPort, mLPumpTime, VacTime, AirTime, FillTime, VacStartPort, VacEndPort, ventRuntime);
  }

  // cue: left up on purpose (no ottoCueClear) - it stays through stopLoop /
  // the panel RESULT screen; the next ottoStepBegin retires it
  ottoCue("DONE - CHECK WELLS: ~1 mL EACH, NO AIR", OTTO_CUE_CALM);
}