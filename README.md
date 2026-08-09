# CXG-E60WT Soldering Iron firmware (STM8S103K3)

A firmware for the CXG-E60WT soldering iron with STM8S103K3 MCU. Features sleep/wake-up modes, buzzer, error detection, per-tip ADC calibration, mains voltage compensation (110V/220V), overtemperature protection, and a **proportional-derivative (PD) controller with power taper** for accurate, overshoot-free temperature regulation.

For those who want to re-build the firmware you'll need two tools:

- **stm8flash** — ST-Link flashing utility: [vdudouyt/stm8flash](https://github.com/vdudouyt/stm8flash)
- **SDCC** — Small Device C Compiler: [sdcc.sourceforge.net](https://sdcc.sourceforge.net/)

## On macOS (Apple Silicon / M1):

### To install SDCC
```
brew install sdcc
```

### To install stm8flash
```
git clone https://github.com/vdudouyt/stm8flash.git
cd stm8flash
make
cp stm8flash /opt/homebrew/bin/
```

### To build the firmware
```
git clone https://github.com/Lymes/cxg-e60wt.git
cd cxg-e60wt
make
make flash
```

## On Linux:

### To install SDCC
```
sudo add-apt-repository ppa:laczik/ppa
sudo apt-get update
sudo apt-get install sdcc
```

### To install stm8flash
```
git clone https://github.com/vdudouyt/stm8flash.git
cd stm8flash
make
sudo make install
```

### To build the firmware
```
git clone https://github.com/Lymes/cxg-e60wt.git
cd cxg-e60wt
make
make flash
```

## Debug Build (UART printf on PD5)

A debug build activates a serial trace output on **PD5 (UART1 TX, 115200 8N1, 5V)** with zero impact on the release firmware — all debug code is removed by the preprocessor when `DEBUG` is not defined.

### Hardware

Connect a USB-UART dongle (**5V-compatible** RX, e.g. CH340G with jumper on 5V):

```
Soldering iron        USB-UART dongle (CH340N SOP-8)
──────────────────────────────────────────────────────
CON1 GND   ────────── Pin 1 (GND)
CON1 VDD+  ────────── Pin 5 (VCC, 5V)
MCU PD5    ────────── Pin 7 (RXD) ← confirmed on CH340N SOP-8
```

> **Note:** on the CH340N SOP-8 package, RXD is pin 7 and TXD is pin 6
> (opposite of many breakout boards). Verify with your specific module.

`CON1` is the ST-Link programming header already on the board. `VDD+` is produced by the on-board **IC3 L05** 5V LDO — safe secondary-side power, isolated from mains.

### Build & flash

```bash
# Always run 'make clean' when switching between release and debug
make clean && make debug       # build only
make clean && make flash-debug # build + flash via ST-Link
```

### Serial output

Monitor with `minicom -o -D /dev/cu.usbserial-* -b 115200` (or any serial terminal at 115200 8N1).

| Line | When | Fields |
|---|---|---|
| `B hp=… c=… al=… ah=… sl=… ds=…` | Boot | heatPoint, calibration, adcMin, adcMax, sleep1/2 timeouts |
| `T=… S=… e=… a=… v=… p=…` | Every 200 ms | temp°C, setpoint, error, ADC_sensor_raw, ADC_vin_raw, PWM duty (100=off, minPwm=full) |
| `Sp<n>` | Button press | new setpoint |
| `E!<n> a=…` | Sensor error confirmed | heater OFF, raw ADC shown |
| `OV<temp>` | Over-temperature | hard fault latched |
| `Rw T=…` | Thermal runaway | transistor Q1 stuck |

### Build sizes

| Build | Flash used | Free (of 8192 B) |
|---|---|---|
| Release | ~6902 B | ~1290 B |
| Debug | ~7535 B | ~657 B |

## Service Menu
You can enter the Service Menu pressing "+" key and Power ON.

Double-click on any key will cyclically change the following menu items:
* **SOU**: enable/disable sound, values 0..1 (default 1)
* **CAL**: calibration offset in degrees, range -99..99 (default 0)
* **SL1**: sleep timeout in minutes, range 1..30 (default 3) — keeps 100°C
* **SL2**: deep sleep timeout in minutes, range 1..60 (default 10) — heater OFF
* **FRC**: forced mode temperature increment in degrees, range 0..100 (default 0)
* **ADL**: ADC cold point calibration (sensor at ambient temperature) — per-tip
* **ADH**: ADC hot point calibration (sensor at maximum temperature) — per-tip

To exit the Service Menu just switch OFF/ON the soldering iron.

**NOTES:**
* When in SL1 mode the soldering iron keeps 100°C
* When in SL2 mode the heater is fully OFF and the display goes blank
* To reset all values to DEFAULT press "-" key and power ON the device
* Pressing "+" and "-" simultaneously toggles FORCED mode (display shows °F symbol)

## Per-tip ADC Calibration (ADL / ADH)

Different tips have different thermal contact with the ceramic heater, resulting in a shifted ADC response curve. Use **ADL** and **ADH** to calibrate each tip:

1. Enter Service Menu (press "+" at power ON)
2. Navigate to **ADL** — with the iron at room temperature, adjust until the displayed temperature matches ambient (~25°C)
3. Navigate to **ADH** — bring the iron to maximum temperature, adjust until the display matches a known reference (e.g. measured with an external thermometer at ~450°C)
4. Fine-tune with **CAL** for a small offset correction if needed

## Temperature Control (PD + Power Taper)

The firmware uses a **proportional-derivative controller** with a **power taper** instead of a simple bang-bang or linear ramp:

- **P term**: reduces heater power proportionally as temperature approaches the setpoint
- **D term**: detects the rate of temperature rise and brakes early — adapts automatically to the thermal inertia of different tip sizes
- **Power taper**: caps maximum heater power to `(diff + 15)%` of available range. At 50°C away → 65% max; at 10°C away → 25% max; prevents energy accumulation in the heater element that causes overshoot
- **Hard cutoff**: heater is switched OFF immediately when temperature meets or exceeds the setpoint

**Measured results (with tip installed):**

| Setpoint | Overshoot | Steady-state accuracy |
|---|---|---|
| 120°C | +2°C | ±3°C |
| 250°C | 0°C | ±4°C |

The ±3-4°C steady-state band is the resolution limit of the 10-bit ADC with the NTC sensor.

## Overtemperature Protection

The firmware includes two independent hardware protection layers:
* **Hard limit (480°C)**: if the measured temperature exceeds 480°C the heater is switched OFF immediately, the display shows **OVH** and the fault is latched until power cycle
* **Thermal runaway**: if the heater is commanded OFF but the temperature **rises** more than 60°C above target for 8 consecutive seconds (indicating a stuck transistor Q1/IRF840), the same OVH fault is triggered. Natural cooling (temperature stable or falling) never trips this check.

## Mains Voltage Compensation (110V / 220V)

The R14/R16 voltage divider feeds the DC bus voltage to ADC CH1. The firmware automatically scales the maximum heater power to maintain constant thermal output regardless of mains voltage (110V or 220V AC), with no manual configuration required.


## Photos

### CXG-E60WT soldering iron
> Before opening, remove the rubber buttons from the handle.

![CXG-E60WT soldering iron](/images/screen1.jpeg)

### PCB — display side
![PCB display side](/images/screen2.jpeg)

### PCB — component side
ST-Link programming header (CON1), mercury tilt switch, and buzzer are visible with labels.

![PCB component side](/images/screen3.jpeg)

### ST-Link V2 programmer
Use the **left row** of pins for STM8 (SWIM + RST + GND + VDC 5V).

![ST-Link V2](/images/screen4.jpeg)

### CH340 USB-UART module (for debug UART)
Reference photo of a CH340N SOP-8 breakout used to monitor the debug serial output on PD5.

![CH340 USB-UART module](/images/screen5.jpeg)

### Work in progress
Board open on the bench with ST-Link and UART adapter connected, running firmware during development.

![Work in progress](/images/screen6.jpeg)

## CXG-E60WT Schematic diagram

![CXG-E60WT Scheme](/images/scheme.gif)

## Additional hardware

The following additional hardware has been installed:
- Mercury Switch: https://www.aliexpress.com/item/32509962658.html?spm=a2g0s.9042311.0.0.274233edX3SZw4
- SMD Buzzer:   https://www.aliexpress.com/item/4000043864737.html?spm=a2g0s.9042311.0.0.274233ediyCCli


Please feel free to use, modify, add new cool features.  Good luck!
