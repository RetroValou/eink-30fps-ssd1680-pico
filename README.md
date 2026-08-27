# High-Speed E-Ink Display (SSD1680 ~30 FPS)

This is a Proof of Concept (POC) project to achieve high refresh rates (~30 FPS) on an SSD1680 E-Ink display (296x128) using a Raspberry Pi Pico (RP2040).

**Link of the screen** : https://a.aliexpress.com/_EwXZnTM

> **!!! WARNING !!! - 
> EXPERIMENTAL PROJECT — USE AT YOUR OWN RISK**
> - This project drives the E-Ink display far outside standard specifications.
> - It may **damage** or permanently **break** your screen.
> - No long-term durability or hardware lifespan tests have been conducted.
> - Tested only in room temperatures between **25°C** and **30°C**.

---

## Demonstration

![Demo](git_asset/presentation.gif)

| Videos link |
| :--- |
| **https://youtube.com/shorts/2a2peLPoG-4?si=s9Ip-TMU3TKgSbHC** |
| **https://youtu.be/mXJzX-ChPq4?si=gPxGaxDmFcfgZog-** |
| **https://youtu.be/n4vD2oG3FTA?si=KuVln5IYM-LVP9yl** |
| **https://youtu.be/qPXJoeaoD4Q?si=WtSLQF_6KufEmaOL** |
| **https://youtu.be/nNH0770l38s?si=ZnYsdGb6J3APIjYM** |


---

## Overview

Standard E-Ink displays take hundreds of milliseconds to refresh. This project uses custom waveforms (LUT), overclocked SPI, and electrical charge compensation to update the display at **29 to 30 FPS** with grayscale support.

*Note: The included video decoder and compression scripts are only used as a test bench to benchmark real-time display refresh performance.*

---

## Technical Features

### 1. Custom LUT & Temperature Sensor Trick
- Uses a **custom Look-Up Table (LUT)** designed for fast refresh cycles.
- Sends an undocumented value (`0x00`) to the temperature control register during boot initialization. This bypasses default thermal limits and unlocks maximum refresh speed.

### 2. High SPI Frequency
- SPI bus is clocked at **20.83 MHz** (slightly above the panel's official 20 MHz rating).

### 3. Differential Updates & Dual-Pass Rendering
- Frame updates send only differential pixel changes to reduce screen latency.
- Uses a 2-pass frame update mechanism (combining transition update and final image update).

### 4. Electrical Charge Compensation
- Tracks accumulated voltage drift per pixel using an internal balance buffer.
- Sends corrective electrical pulses when pixels stay purely black or white for too long.
- Applies extra charge pulses after pixel transitions to prevent washed-out contrast.

### 5. Grayscale Mode
- Implements 2-bit grayscale by fast temporal pixel alternation (blinking between black and white).
- Uses 1 update pass instead of 2 for fast motion rendering.

---

## Performance

- **Current actual speed:** 29 to 30 FPS.
- **Potential speed with shorter LUT compensation:** 31 to 32 FPS.
- **Display driver raw update limit (excluding SPI data transfer overhead):** Up to 39 FPS.

---

## Known Limitations

- **Ghosting:** Residual image ghosting is still visible, especially in grayscale mode compared to monochrome (black & white).
- **Temperature Dependence:** Image stability depends heavily on room temperature since automatic thermal adjustment is disabled.

---

## Areas for Improvement

- **Partial SPI Data Transfers:** Optimize SPI transfers by sending only altered pixel blocks instead of full frame buffers (currently sending 4 buffers: current, previous, and 2 compensation buffers).
- **Enhanced Ghosting Reduction:** Adjust the number of compensation updates dynamically to clean residual artifacts.
- **Temperature-Specific LUTs:** Build custom LUT profiles tuned for different ambient temperature ranges.
- **Power Control Safeguards:** Add checks to prevent display freeze issues when toggling high-voltage analog circuitry.

---

## Code Base Overview

- `lib/ssd1680_rv.h` / `lib/ssd1680_rv.cpp`: Core C++ driver implementing fast updates, charge balance tracking, and custom LUT management.
- `var/cmd.h` & `var/config.h`: SSD1680 command definitions, supply voltages, and charge balance thresholds.
- `lib/videodecoder.cpp`: RLE Delta video decoder for streaming compressed test frames.
- `compress_video/main.py`: Python utility to compress MP4 videos into 1-bit or 2-bit Delta-RLE C headers.

---

## Hardware Setup (Raspberry Pi Pico)

| Signal | Pico GPIO | Function |
| :--- | :--- | :--- |
| **CLK** | GPIO 10 | SPI Clock |
| **MOSI** | GPIO 11 | SPI Data Output |
| **DC** | GPIO 12 | Data / Command Select |
| **CS** | GPIO 13 | SPI Chip Select |
| **BUSY** | GPIO 14 | Panel Busy Signal |
| **RST** | GPIO 15 | Display Hardware Reset |
| **START** | GPIO 18 | Execution Trigger Pin (Input) |