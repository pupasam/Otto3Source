# Known issues & unresolved calibration findings

Status snapshot at the end of the 2026-08-21 bench session. Calibration
Steps 1–7 are verified good (constants committed); **Step 8 is where the
open problems live**, and Step 9 has not been run yet because of them.
Read this before resuming calibration — several days went into the
evidence below, and none of it should be rediscovered.

## 1. The split distributes air unevenly, and port 2 can get none (ROOT CAUSE FOUND, NOT YET FIXED)

**Symptom** (production `AddSBSReagentMulti`, and reproduced in isolation):
after the air bubble is split across the six sample lines, the air-liquid
interface is highest in line 7 and descends monotonically — line 7 gets the
least air, each later line more — and **line 2 can receive no air at all**
(a liquid-to-liquid transition where its slice should be).

**What it is NOT** (each eliminated by direct experiment, 2026-08-21):

- *Not the park constant.* We ran a decisive experiment: park the bubble,
  titrate it forward in ~13 µL nudges until the operator saw the first air
  tip exit into line 7 (park error eliminated **by construction**), then run
  the verbatim production split loop. The gradient appeared anyway.
- *Not tubing-length variance.* The lines were recut to even lengths on
  2026-08-21; the gradient persisted and tracks the split ORDER (7→2), not
  any fixed line identity.
- *Not `adjustSampleVolMicro` or any downstream constant* — those only shift
  the later purge, which happens after the split is already uneven.

**Root cause — compressible-cushion theft, compounding down the sequence:**
during the first slice the pump pushes against the full ~0.32 mL air column;
part of the slice's displacement compresses the cushion instead of injecting
air into line 7. When the pump stops and the valve moves on, the un-relaxed
remainder discharges into the *next* line's window. Each line inherits the
previous line's stolen volume while the cushion shrinks, so the transfer
compounds: 7 least ... 3 most, and the bubble is fully spent before port 2's
turn — its slice pushes pure liquid. Note the functional consequence: **line
2 ends with no air barrier between wash and reagent** — the two liquids sit
in direct contact in that line, which is likely the real chemistry risk.

**Candidate fixes (decision + bench test needed):**

1. **Hold-after-slice** — pause 2–3 s after each split slice BEFORE the
   sample valve switches, so each line keeps its own relaxation instead of
   donating it to the next. Attacks the mechanism directly; preserves all
   volumes; a few-line change in the split loop.
2. **Per-line slice times** — lengthen early slices, shorten late ones.
   Tunable, but chases a dynamic effect with static constants.
3. **Accept it** — if the chemistry tolerates uneven air and a barrier-less
   line 2 (question for Bryce; did Otto2 show the same signature?).

**The yardstick for any fix** (all serial commands, OttoPanel):
`d a r` (clean) → `b` (park) → `u` until the tip exits into 7 → `M`
(production split, one step) → compare the six interfaces. The 2026-08-21
baseline to beat: 7 shortest, monotone growth, 2 = zero.

## 2. `ReagentLineVolume` is bracketed, not settled

Three measurements disagree in an informative way:

| Method | Value | Caveat |
|---|---|---|
| Flowing calibration via full split test (2026-08-20) | 0.60 | includes ~50–70 µL of in-flight bubble compression |
| Frozen park at 0.575 (2026-08-21) | front just inside the valve | freeze-at-pump-stop reads short of any settled state |
| Stepwise titration to the port-7 exit (2026-08-21) | ≤ 0.655 nominal | 0.25 s pump pulses under-deliver, so this is a ceiling |

The board currently carries **0.575** (mid-investigation value). Since
issue #1 shows the split's evenness is NOT controlled by this constant,
the remaining requirement on it is looser than we thought: it must deliver
the front to the valve without pushing air into line 7 before the split.
Re-settle it (one `b` run, front at the entrance) after the split fix is
chosen, then confirm with a full `t`.

## 3. Short pump pulses do not deliver nominal volume

`mLPumpTime = 18.8 s/mL` was calibrated on long (≥15 s) runs. Evidence from
2026-08-21: seven 0.25 s nudges (+93 µL nominal) were needed to advance a
front the operator had judged "just inside the valve" — consistent with the
MINIPULS losing most of a quarter-second command to spin-up. Conversely the
1 s split slices appear to over-transmit (~20 %) once inherited
decompression is included (issue #1's line-2 starvation requires it).
Consequence: **don't trust nominal volume math on sub-second pump windows**;
calibration procedures should use continuous pushes for measurement, short
pulses only for approach.

## 4. Sample line constants may be stale after the recut

The lines were recut on 2026-08-21 to even their lengths.
`SampleLineVolume = 0.110` still assumes **35 in** of 1/64" ETFE at
3.14 µL/in. **Measure the new cut length and update the constant** before
tuning `adjustSampleVolMicro` (whose final purge-edge reading was never
completed — it sits at the mid-tuning checkpoint value 0).

## 5. Open questions for Bryce (design intent, not bugs)

- The bubble well ends ~airTime-worth (~0.3 mL) light on liquid during
  addReagent. Acceptable for the chemistry? (A 3 s airTime halves it —
  trialed 2026-08-20, reverted.)
- What does the `airTime` comment "should be >= pump time" actually
  constrain?
- Did Otto2 show the same split gradient (issue #1) and the chemistry
  simply tolerated it — or did its geometry hide it?

## 6. Hardware items (known quirks, not firmware bugs)

- **The pump runs whenever the GIGA is unpowered** (shifter pull-up sags to
  the dead 5 V rail). Operational rule: GIGA on first, pump power off first.
  Hardware cure when convenient: remove the shifter module's ch.3 pull-up.
- A flyback diode (1N4001, banded end to +12 V) across the vacuum solenoid
  is still worth fitting as standard practice for an inductive load — not
  currently gating anything (45 moves with interleaved solenoid firing ran
  clean without it).

## Resume-here checklist (in order)

1. Measure the recut sample-line length → update `SampleLineVolume` (#4).
2. Decide the split fix with Bryce (#1) → implement behind the `M` test
   first, production only after the yardstick shows flat interfaces.
3. Re-settle `ReagentLineVolume` with one `b` run (#2).
4. Tune `adjustSampleVolMicro` via `t` → `p` purge-edge readings (#4).
5. Run Step 9 (cleavage validation) and close calibration.
