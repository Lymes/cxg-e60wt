# CXG-E60WT Soldering Iron firmware (STM8S103K3)

A firmware for the CXG-E60WT soldering iron with STM8S103K3 MCU. Features sleep/wake-up modes, buzzer, error detection, per-tip ADC calibration, mains voltage compensation (110V/220V), and overtemperature protection.

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

## Overtemperature Protection

The firmware includes two independent hardware protection layers:
* **Hard limit (480°C)**: if the measured temperature exceeds 480°C the heater is switched OFF immediately, the display shows **OVH** and the fault is latched until power cycle
* **Thermal runaway**: if the heater is commanded OFF but the temperature remains more than 30°C above target for 3 seconds (indicating a stuck transistor Q1/IRF840), the same OVH fault is triggered

## Mains Voltage Compensation (110V / 220V)

The R14/R16 voltage divider feeds the DC bus voltage to ADC CH1. The firmware automatically scales the maximum heater power to maintain constant thermal output regardless of mains voltage (110V or 220V AC), with no manual configuration required.


## CXG-E60WT Schematic diagram

![CXG-E60WT Scheme](/images/scheme.gif)

## Additional hardware

The following additional hardware has been installed:
- Mercury Switch: https://www.aliexpress.com/item/32509962658.html?spm=a2g0s.9042311.0.0.274233edX3SZw4
- SMD Buzzer:   https://www.aliexpress.com/item/4000043864737.html?spm=a2g0s.9042311.0.0.274233ediyCCli


Please feel free to use, modify, add new cool features.  Good luck!
