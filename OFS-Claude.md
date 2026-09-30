# Optical Ground-Speed / Slip-Angle Sensor (OFS) — Key Takeaways

**Project:** CP27E custom optical ground-speed sensor, built from a PixArt PAW3395DM-T6QU
gaming-mouse sensor re-imaged through relay optics, with a local IMU for lever-arm
correction and CAN output to the VCU.

**Source tags used below:**
- `[Datasheet: X]` — read directly from the named manufacturer datasheet
- `[Bench]` — measured from actual firmware/UART logs during bring-up
- `[Calc]` — derived from thin-lens/radiometric physics, not directly measured
- `[Lit]` — published FSAE/academic literature (Tarlinton/Wollongong, Chalmers)
- `[Decision]` — a choice made in-conversation, not an external fact
- `[Unverified]` — flagged gap; treat with caution until confirmed

---

## 1. Sensor Core

| Item | Value | Source |
|---|---|---|
| Sensor | PixArt PAW3395DM-T6QU | `[Decision]` |
| Native spec | 26,000 CPI, 650 IPS | `[Datasheet: PAW3395]` |
| VDD | 1.9 V (range 1.8–2.1 V) | `[Datasheet: PAW3395]` |
| VDDIO | 3.3 V | `[Datasheet: PAW3395]` |
| Run-mode current | ~1.7 mA typical | `[Datasheet: PAW3395]` |
| LDO | TLV70019DDCT, 1.9 V fixed, 200 mA, 175 mV dropout | `[Datasheet: TI TLV700]` |
| SPI mode | Mode 3 (CPOL High, CPHA 2 Edge) | `[Datasheet: PAW3395]` required |
| **SPI clock (current)** | **2.625 MHz** (PCLK2 84 MHz ÷ 32) | `[Bench]` confirmed via UART: `SCLK=2625000 Hz` |
| Init sequence | 142-register table, ported from community reference driver | `[Decision]`, register values are NDA-gated by PixArt |

**Confirmed bring-up milestone:** Product ID reads `0x51` reliably `[Bench]`.

**Critical early bug, resolved:** internal PAW3395 LED reflecting off the M12 lens
barrel caused false SQUAL≈95 / zero-motion readings after the stock lens was removed.
Fixed by physically blocking the internal LED. `[Bench]`

**Outstanding firmware issue:** sensor still downshifts into rest states
(`M=0x18`/`0x1C`) during idle; forced run/HP mode not yet implemented. This matters
specifically for launch control — the car is stationary at the exact moment
wheel-slip is largest, so the sensor must not be asleep at that instant. `[Bench]` + `[Decision]`

---

## 2. Optics

| Parameter | Value | Source |
|---|---|---|
| Lens | Commonlands CIL061-F1.9-M12BNIR, 6.0 mm, F/1.9, no IR-cut | `[Decision]`, confirmed via invoice `[Bench]` |
| Holder | CLA018-LCP, 18 mm spacing, 8.4 mm height | `[Decision]` — height is a guess pending die-recess measurement |
| Standoff (lens face → ground) | **91.4 mm (3.60″)** | `[Calc]` — sized to hit ±0.5″ DoF at f/1.9 |
| Magnification m | 0.0703 | `[Calc]` |
| Ground FOV | 14.2 mm | `[Calc]` |
| Ground per pixel | 0.470 mm | `[Calc]`, assumes 33 µm pixel pitch `[Unverified]` |
| Ground CPI (at 26k native) | ~1828–1830 | `[Calc]` |
| **Ride-height tolerance** | **±0.50″ at f/1.9** (current build, no aperture stop) | `[Calc]` — corrects an earlier ±0.24″ error in this same conversation |
| Ride-height tolerance w/ 1.5mm stop | ±1.05″ at f/4 | `[Calc]` — stop is not currently installed |
| Speed ceiling | 511 mph | `[Calc]`, far exceeds any FSAE requirement |
| Focus setting | 0.42 mm past ∞ ≈ 0.85 turns out on M12×0.5 | `[Calc]` |

**Why 6 mm over 8 mm:** at fixed standoff, shorter focal length → smaller
magnification → depth of field scales as 1/m². 6 mm hits ±0.5″ tolerance; 8 mm would
need ~4.7″ standoff to match. `[Calc]`

**Aperture stop (1.5 mm, → F/4):** designed and tested on bench, **not currently
installed** — current build runs native F/1.9. Trade: F/4 doubles DoF tolerance but
needs ~4.4× more illumination. `[Decision]`, `[Bench]`

---

## 3. Bench Validation Results

Measured after blocking the internal LED and removing the stock LM19 lens, in forced
run-mode segments at 26,000 CPI: `[Bench]`

```
M=84 X= 1 Y= 0 SQ=119      (stationary)
M=84 X= 0 Y=-1 SQ=116
M=04 X= 0 Y= 0 SQ=113
M=80 X=-61 Y=-155 SQ=164   (under motion)
M=80 X=109 Y=-33 SQ=169
```

- **Stationary jitter: ±2 counts** → ±0.028 mm apparent ground motion at ~1830 CPI. Tight lock.
- **SQUAL peaks at 169** under motion with lens focused — confirms optics functional.
- Both axes respond independently and proportionally to motion — prerequisite for slip angle.
- Diagnosed and fixed a persistent overexposure/clipping issue (`RAW[0..127]`, shutter
  railed at 4608 floor) via a 1.5 mm aperture stop test, then reverted to F/1.9 for the
  current build. `[Bench]`

---

## 4. Illumination System

| Parameter | Value | Source |
|---|---|---|
| LED | OSRAM SFH 4715B, 850 nm centroid, 30 nm FWHM, ±40° half-angle | `[Datasheet: OSRAM SFH 4715B]` |
| Max radiant flux | 1050 mW @ 1A, 2000 mW @ 2A (100µs pulse) | `[Datasheet: OSRAM SFH 4715B]` — confirms non-linear droop (~5% at 2A) |
| Max continuous current | 2 A; no duty-cycle restriction below that (confirmed from Permissible Pulse Handling chart) | `[Datasheet: OSRAM SFH 4715B]` |
| **Layout (current plan)** | 3 arms × 120° azimuth, 2–3 LEDs/arm (see open item below) | `[Decision]` |
| Geometry | 40 mm height, 42° grazing angle, 44.4 mm radial offset, 59.8 mm slant, 76.9 mm arm-to-arm | `[Calc]` |
| Required ground irradiance (worst case, f/1.9, dark asphalt ρ=0.06) | 42 mW/cm² | `[Calc]`, ρ assumed `[Unverified]` |
| Design target (with SF 1.5) | 63 mW/cm² | `[Decision]` |
| **Drive current — under revision** | 0.65 A (9-LED plan) vs. 2.0 A (6-LED plan) | `[Decision]` — open, see §9 |
| Driver IC | Analog Devices LT3518, 2.3 A / 45 V switch, buck/boost/buck-boost | `[Datasheet: LT3518]` |
| Driver config (9-LED case) | R_SENSE=154 mΩ, R_T=90.9k (250 kHz — required for 95% duty margin at 18V battery floor), L1=100 µH, D1=MBRS260T3 (60V/2A) | `[Calc]` from `[Datasheet: LT3518]` |
| **Critical wiring note** | LT3518 **VIN** pin must run off a separate 3.3V rail, NOT the battery — 30V is its continuous max with zero margin | `[Datasheet: LT3518]`, Note 3 |
| Battery range | 18–30 V | `[Decision]`, user-stated |
| Connector | Molex Micro-Fit 3.0: 43650-0200 (header), 43645-0200 (housing), 43030-0007 (terminal, 20–24AWG, 5A) | `[Datasheet: Molex Micro-Fit]` |
| Filter | 850 ± 20 nm bandpass (40 nm FWHM, not 20 nm) | `[Calc]` — 20 nm clips 43% of LED's own 30 nm-wide emission plus can't track thermal centroid drift (~0.19 nm/°C, confirmed `[Datasheet: OSRAM SFH 4715B]`) |

**Why 3 arms at 120°, not a ring:** a full ring illuminator fills in its own shadows
(bright-field, no texture contrast). A single source creates parallel shadows only,
blinding one axis of motion — unacceptable for slip-angle measurement, which is a
ratio of two perpendicular axes. Three sources at 120° give shadow coverage in every
direction. `[Calc]` / physical reasoning, this chat.

**Strobing rationale:** required energy is fixed (E ∝ 1/t), so a short bright pulse
and a long dim one draw the same average power — but only the short pulse freezes
motion. Blur at 80 mph: 24 µs pulse → 1.8 px blur (too much); 5.4 µs → 0.41 px (good).
This is the argument driving the 6-LED-at-2A reconsideration below. `[Calc]`

---

## 5. IMU

| Parameter | Value | Source |
|---|---|---|
| **Selected part** | **ASM330LHB** (ST) | `[Decision]` |
| Why not ICM-42688-P | Better gyro noise (2.8 mdps/√Hz) and has hardware FSYNC, but **out of stock until 2027** — structural wafer shortage from AR/wearables demand, no genuine pin-compatible substitute exists | `[Web: supply-chain reporting]` |
| ASM330LHB gyro noise | 5 typ / 12 max mdps/√Hz | `[Datasheet: ASM330LHB]`, confirmed via full-text extraction |
| ASM330LHB bias instability | 3°/hour | `[Datasheet: ASM330LHB]` |
| Qualification | AEC-Q100, −40 to 105°C | `[Datasheet: ASM330LHB]` |
| Sync capability | **No external hardware sync pin** — "synchronized output" is internal only (accel+gyro sampled together); has a 25 µs internal timestamp counter instead | `[Datasheet: ASM330LHB]`, confirmed via full pin-table extraction |
| Why the noise gap doesn't matter | 5 mdps/√Hz × √100Hz × 1.2m lever arm ≈ 1 mm/s error, vs. optical quantization floor of ~14 mm/s at 1kHz — 14× margin | `[Calc]` |
| Interface | SPI (own bus, separate from PAW3395's) | `[Decision]` |
| Config target | ±1000 dps gyro, ±16g accel, ~8kHz oversample / ~250Hz LPF / decimate to 1kHz | `[Decision]` |

**Alternative noted:** ASM330LHHG1 (same family, 125°C rating) if thermal margin
becomes a measured problem — not adopted by default. `[Decision]`

**Lever-arm correction (why the IMU is needed at all):** sensor is not at CG.

```
V_CG = V_sensor − ω × r
```

Local IMU measures ω correctly regardless of mounting location (angular velocity is
uniform on a rigid body); r is a fixed config constant from CG measurement. Co-location
on the same board/clock as the optical sensor is what makes the correction trustworthy
during fast transients. `[Calc]`

---

## 6. MCU Pin Assignments (STM32H573RIT6)

### Optical Sensor (SPI3)
| Signal | MCU Pin | Function |
|---|---|---|
| OFS_SCLK | PC10 | SPI3_SCK |
| OFS_MISO | PC11 | SPI3_MISO |
| OFS_MOSI | PC12 | SPI3_MOSI |
| OFS_NCS | PA15 | GPIO output (software CS) |
| OFS_MOTION | PB0 | GPIO input, EXTI0 |
| OFS_NRESET | PB1 | GPIO output |
| OFS_LDO_EN | PB2 | GPIO output |
| OFS_LED_P | PC6 | TIM8_CH1 input capture (exposure edge) |
| OFS_STROBE | PC7 | TIM8_CH2 output (drives LT3518 PWM) |

### IMU (SPI2)
| Signal | MCU Pin | Function |
|---|---|---|
| IMU_SCLK | PB10 | SPI2_SCK |
| IMU_MISO | PB14 | SPI2_MISO |
| IMU_MOSI | PB15 | SPI2_MOSI |
| IMU_NCS | PB4 | GPIO output (software CS) |
| IMU_INT1 | PB3 | GPIO input, EXTI3 (data ready) |
| IMU_FSYNC | PC8 | TIM8_CH3 output — phase-locks IMU sample to LED_P/strobe on the same timer |

`[Decision]`, cross-checked against schematic screenshots this chat. **Not yet
verified:** SPI2/SPI3/TIM8 alternate-function mapping in CubeMX for the H573
specifically — assignments assume standard STM32 AF tables. `[Unverified]`

**FSYNC vs INT2 tradeoff:** using pin 9 as FSYNC (input, phase-locked to the optical
strobe timer) sacrifices a second interrupt line but buys hardware time-alignment
between IMU and optical samples — the deciding reason this IMU was chosen despite its
worse gyro noise. Using it as INT2 instead would require software timestamp
reconciliation, erasing that advantage. `[Decision]`

---

## 7. CAN Output Spec (to VCU)

| Frame | Contents | Rate |
|---|---|---|
| 0x100 | Vx_s, Vy_s (int16, 0.01 m/s/LSB), β_s (int16, 0.01°/LSB), SQUAL (uint8), status (uint8) | 1 kHz |
| 0x101 | Vx_CG, Vy_CG (int16), β_CG (int16), ωz (int16, 0.02°/s/LSB) | 1 kHz |
| 0x102 | ωx, ωy, ωz (int16 ea), temp (int8), seq (uint8) | 500 Hz |
| 0x103 | ax, ay, az (int16, 0.01 m/s²/LSB), shutter (uint16) | 500 Hz |
| 0x104 | diagnostics (raw sum/min/max, mode, faults) | 10 Hz |

`[Calc]` / `[Decision]`. int16 chosen over int8 (insufficient range/resolution
tradeoff) and over int32 (no signal here carries >16 bits of real information — sensor
quantization and gyro noise floor both sit well inside int16 range). Bus load ~390
kbit/s on classic CAN (40% utilization) or a single 64-byte FDCAN frame at 1kHz if
available.

**Key insight:** slip angle (β) needs no ride-height correction — magnification
cancels in the Vy/Vx ratio — but speed magnitude does. This is why β is the more
inherently trustworthy output. `[Calc]`

---

## 8. Validation Plan

Tow-hitch mount on a BMW 340i + VectorNav VN-300 (dual-antenna GNSS compass, INS mode)
as ground truth. `[Decision]`

Key corrections identified as necessary:
- **Lever-arm correction, both sensors** — hitch-to-VN300 distance (~2m) means yaw
  rate induces apparent sideslip at the hitch that isn't real motion at CG; must
  apply ω×r to both the optical sensor's reading and reconcile with the VN-300's own
  mount point. `[Calc]`
- **Frame alignment** — steady straight-line run should read β=0; any offset is
  mounting misalignment, calibrated out before dynamic testing.
- **Ride-height margin** — recommend reinstalling the aperture stop (→ ±1.05″
  tolerance) for this specific test, since a road surface's irregularity likely
  exceeds the bare ±0.50″ f/1.9 budget. `[Decision]`
- **Timing sync** — pipe VN-300 UART into the same STM32 logging optical data, single
  clock domain.

Protocol order: static → straight-line (speed cal) → steady circles (β cal) → slalom
(transient response) → drift (extreme case, last, once everything else is validated).

---

## 9. Open Items / Unresolved Decisions

1. **LED count/current tradeoff — not finalized.** 9 LEDs @ 0.65 A (irradiance
   margin, but 24 µs min pulse → 1.8 px blur at 80 mph) vs. 6 LEDs @ 2.0 A (shorter
   string voltage, 2 MHz switching possible, 3 µs min pulse → 0.41 px blur, same
   average power). Blur analysis favors fewer LEDs at higher current; final call
   pending. `[Decision — open]`
2. Force run/HP mode in firmware — sensor still downshifts to rest states.
3. Scope LED_P directly to get real exposure/frame timing — PixArt does not publish
   this number (NDA); all strobe-sync design currently assumes it exists and behaves
   as expected. `[Unverified]`
4. **Measure actual die recess depth** on the PAW3395 package → confirms or corrects
   the 8.4mm holder height guess.
5. **Measure actual pixel pitch** (currently assumed 33 µm) — this single number
   scales the entire ride-height tolerance and radiometric budget linearly. Highest-
   priority unmeasured quantity in the whole project.
6. **Measure real asphalt reflectance** on the team's actual test surface (currently
   assumed ρ=0.10 nominal, 0.06–0.18 range).
7. Decide aperture stop in/out for the on-car build — trades DoF tolerance against
   illumination budget.
8. Finalize LT3518 board layout, open-LED protection network (Fig. 4 topology,
   R1/R2/R3/Q1), bring-up.
9. IMU lever-arm correction firmware — not yet written, pending ASM330LHB hardware.
10. Focus-transfer method between test webcam (Arducam B0332, OV9281 global shutter,
    M12, no IR-cut) and the actual PAW3395 — requires matching optical (not just
    mechanical) distance from holder face to each die, accounting for cover-glass
    thickness on each part. Check whether PAW3395 supports a raw-frame-dump SPI mode
    (some PixArt parts do) — would allow direct focus verification on the actual die,
    skipping plane-transfer entirely. `[Decision]`, `[Unverified]`

---

## 10. Sourcing Legend Recap

Datasheets consulted directly (full or partial text extracted this chat): PixArt
PAW3395 (partial/community), OSRAM SFH 4715B, Analog Devices LT3518, ST ASM330LHB, TI
TLV70019, TDK ICM-42688-P (rejected on availability). All other technical figures are
either bench-measured from live firmware logs, or calculated from stated thin-lens/
radiometric/electrical relationships — flagged individually throughout. Anything
tagged `[Unverified]` is a live risk to the numbers built on top of it and should be
resolved before finalizing the on-car BOM.
