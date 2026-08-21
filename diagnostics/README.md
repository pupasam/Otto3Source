# Bench diagnostics

Tiny single-purpose sketches for isolating one piece of hardware at a time.
Flash one, observe, flash the next. None of them touch anything they don't
name — pump-only sketches never move valves, valve sketches never start the
pump. Flashing = the sketch starts immediately after the reboot.

Compile/flash any of them exactly like the main sketches:

```
arduino-cli compile --fqbn arduino:mbed_giga:giga diagnostics/<Name>
arduino-cli upload  --fqbn arduino:mbed_giga:giga -p <port> diagnostics/<Name>
```

| Sketch | What it does | Use it when |
|---|---|---|
| `SafeOff` | Pump held STOPPED, solenoid held CLOSED, forever | Parking the instrument safe; first flash of a bench day |
| `PumpOff` | Pump line held in STOP only | Killing a runaway pump fast |
| `PumpRunOn` | Pump runs CONTINUOUSLY (red LED = commanded on) | Cassette tensioning, priming by eye, flow troubleshooting |
| `PumpTest` | Pump 5 s ON / 5 s OFF forever, LED mirrors command | Verifying the GPIO 33 → BSS138 ch.3 → barrier-strip path |
| `SolenoidTest` | Solenoid 2 s ON / 2 s OFF forever, LED mirrors command | Verifying the GPIO 32 → opto relay → 12 V loop path |
| `VacScan` | Solenoid OPEN, aspiration valve dwells 20 s on each well port 2..7 | Finding a weak/blocked vacuum line, needle height issues |
| `DisplayTest` | Title + live seconds counter on the Display Shield | Proving the shield + GFX library before blaming firmware |

Safety notes:

- **The pump runs whenever the GIGA is unpowered** (the shifter pull-up sags
  to the dead 5 V rail and grounds the start input). GIGA on first, pump
  power off first — always.
- After ANY front-panel keypad use, press the pump's STOP key once or remote
  control stays disarmed.
- `VacScan` needs the 12 V solenoid supply on and wall vacuum open, or every
  port will look "dead".
