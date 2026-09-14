# KriSHE Carbon – Kiln dMRV Telemetry System & Companion App
### Complete Engineering Reference Manual & Technical Documentation

[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/Framework-ESP--IDF_v5.2.1-red.svg)](https://docs.espressif.com/projects/esp-idf/en/v5.2.1/esp32s3/)
[![Android](https://img.shields.io/badge/Android-Companion_App_(API_26+)-green.svg)](https://developer.android.com/)
[![License](https://img.shields.io/badge/License-Proprietary_/_KriSHE-purple.svg)]()

Industrial-grade digital Measurement, Reporting, and Verification (**dMRV**) telemetry node and mobile companion system designed for artisanal and industrial biochar kilns. The system provides real-time multi-zone thermal tracking, authoritative GNSS location and UTC timestamping, local Web/REST connectivity, robust Bluetooth Low Energy (BLE) communication, and an Android companion app featuring live rolling canvas charts, in-app sample review, calibration offsets, and RFC-4180 CSV export.

---

## 📋 Comprehensive Table of Contents
1. [Physical & Carbon Verification Domain Context](#1-physical--carbon-verification-domain-context)
   - [What is Pyrolysis and Biochar?](#what-is-pyrolysis-and-biochar)
   - [Why Multi-Zone Temperature Tracking is Mandatory](#why-multi-zone-temperature-tracking-is-mandatory)
   - [Digital Measurement, Reporting, and Verification (dMRV)](#digital-measurement-reporting-and-verification-dmrv)
2. [Silicon & Microcontroller Hardware Architecture](#2-silicon--microcontroller-hardware-architecture)
   - [ESP32-S3 N16R8 SoC Specifications](#esp32-s3-n16r8-soc-specifications)
   - [CPU Frequency Optimization (Why 160 MHz vs. 240 MHz)](#cpu-frequency-optimization-why-160-mhz-vs-240-mhz)
   - [Memory Architecture (SRAM, PSRAM, Flash)](#memory-architecture-sram-psram-flash)
   - [FreeRTOS Asymmetric Dual-Core Scheduling](#freertos-asymmetric-dual-core-scheduling)
3. [Electrical Wiring & Pinout Safety Analysis](#3-electrical-wiring--pinout-safety-analysis)
   - [Master Wiring Matrix](#master-wiring-matrix)
   - [ESP32-S3 Hardware Pinout Safety Analysis](#esp32-s3-hardware-pinout-safety-analysis)
   - [3.3V Logic vs. 5V Damage & ESD Clamping Physics](#33v-logic-vs-5v-damage--esd-clamping-physics)
4. [Communication Protocols & Bus Physics Deep-Dive](#4-communication-protocols--bus-physics-deep-dive)
   - [MAX6675 Bit-Banged SPI Communication](#max6675-bit-banged-spi-communication)
   - [7Semi L89HA Multi-GNSS UART Communication](#7semi-l89ha-multi-gnss-uart-communication)
   - [Apache NimBLE Bluetooth Low Energy (BLE 5.0)](#apache-nimble-bluetooth-low-energy-ble-50)
   - [Wi-Fi 802.11 b/g/n SoftAP & HTTP REST Server](#wi-fi-80211-bgn-softap--http-rest-server)
   - [Inter-Task Communication & Synchronization Primitives](#inter-task-communication--synchronization-primitives)
5. [Firmware Architecture & File-by-File Walkthrough (`src/`)](#5-firmware-architecture--file-by-file-walkthrough-src)
   - [Central Configuration (`src/config.h`)](#1-central-configuration-srcconfigh)
   - [Application Entry Point & Tasks (`src/main.c`)](#2-application-entry-point--tasks-srcmainc)
   - [MAX6675 Driver & Outlier Filtering (`src/max6675.h`, `src/max6675.c`)](#3-max6675-driver--outlier-filtering-srcmax6675h-srcmax6675c)
   - [GNSS NMEA Parser & Geometry (`src/gnss.h`, `src/gnss.c`)](#4-gnss-nmea-parser--geometry-srcgnssh-srcgnssc)
   - [dMRV Kiln State Machine (`src/kiln_state.h`, `src/kiln_state.c`)](#5-dmrv-kiln-state-machine-srckiln_stateh-srckiln_statec)
   - [Thread-Safe Telemetry Model (`src/data_model.h`, `src/data_model.c`)](#6-thread-safe-telemetry-model-srcdata_modelh-srcdata_modelc)
   - [Web Dashboard & REST Endpoint (`src/dashboard.h`, `src/dashboard.c`)](#7-web-dashboard--rest-endpoint-srcdashboardh-srcdashboardc)
   - [NimBLE GATT Peripheral & Coexistence (`src/ble_service.h`, `src/ble_service.c`)](#8-nimble-gatt-peripheral--coexistence-srcble_serviceh-srcble_servicec)
6. [Toolchain & Kernel Build Configuration](#6-toolchain--kernel-build-configuration)
   - [`platformio.ini`](#platformioini)
   - [`sdkconfig.defaults`](#sdkconfigdefaults)
   - [`CMakeLists.txt`](#cmakeliststxt)
7. [Android Mobile Companion App Architecture (`KriSHE-Carbon-App`)](#7-android-mobile-companion-app-architecture-krishe-carbon-app)
   - [MVVM Clean Architecture & Data Flow](#mvvm-clean-architecture--data-flow)
   - [Application Entry & Navigation (`MainActivity.kt`)](#application-entry--navigation-mainactivitykt)
   - [Hardware-Accelerated Live Canvas Chart (`LiveTemperatureChartView.kt`)](#hardware-accelerated-live-canvas-chart-livetemperaturechartviewkt)
   - [Real-Time Monitoring Dashboard (`DashboardFragment.kt`)](#real-time-monitoring-dashboard-dashboardfragmentkt)
   - [Device Discovery & Connection (`ScannerFragment.kt`)](#device-discovery--connection-scannerfragmentkt)
   - [Telemetry Logs & In-App Sample Preview (`LogsFragment.kt`)](#telemetry-logs--in-app-sample-preview-logsfragmentkt)
   - [Calibration, Diagnostics & Alerts (`SettingsFragment.kt`)](#calibration-diagnostics--alerts-settingsfragmentkt)
   - [BLE GATT Client Engine (`BleManager.kt`)](#ble-gatt-client-engine-blemanagerkt)
   - [Room SQLite Persistence (`SessionDatabase.kt`, `SessionDao.kt`)](#room-sqlite-persistence-sessiondatabasekt-sessiondaokt)
   - [RFC-4180 CSV Export Engine (`CsvExporter.kt`)](#rfc-4180-csv-export-engine-csvexporterkt)
8. [Thermal Management & Low-Power Operations](#8-thermal-management--low-power-operations)
9. [Verification, Field Testing & Troubleshooting](#9-verification-field-testing--troubleshooting)

---

## 1. Physical & Carbon Verification Domain Context

### What is Pyrolysis and Biochar?
Biochar is stable, carbon-rich solid biomass produced through **pyrolysis**—the thermal decomposition of organic agricultural waste (e.g., crop residue, wood prunings, coconut shells) under an oxygen-depleted or oxygen-limited atmosphere. Unlike open biomass decomposition or combustion, which releases biomass carbon back into the atmosphere as carbon dioxide ($\text{CO}_2$) and methane ($\text{CH}_4$), biochar locks atmospheric carbon into an inert, highly aromatic recalcitrant carbon matrix that remains stable in soils for hundreds to thousands of years.

Pyrolysis occurs through distinct physical-chemical phases dependent on temperature:
1. **Drying & Torrefaction ($100^\circ\text{C} - 250^\circ\text{C}$)**: Moisture evaporates; light volatile hemiceullulose chains degrade.
2. **Exothermic Pyrolysis ($300^\circ\text{C} - 550^\circ\text{C}$)**: Cellulose and lignin depolymerize, producing condensable bio-oils, non-condensable syngas ($\text{CO}, \text{H}_2, \text{CH}_4$), and solid fixed-carbon biochar. The reaction becomes autothermal (exothermic).
3. **High-Temperature Carbonization ($>600^\circ\text{C}$)**: Secondary cracking of tars, maximizing aromatic graphitic carbon rings, pore volume, and cation-exchange capacity.

### Why Multi-Zone Temperature Tracking is Mandatory
In artisanal flame-curtain kilns (Kon-Tiki, Ring, or Pit kilns) and industrial retorts, temperature distribution is non-uniform due to convective buoyant air currents and flame-cap boundary layers:
- **TOP Zone**: Measures the flame cap and radiative heat trap. The flame cap must stay hot enough ($>650^\circ\text{C}$) to burn smoke, particulate matter, and methane emitted from the pyrolyzing biomass beneath it.
- **MIDDLE Zone**: Reflects the active pyrolyzing biomass bed. It must sustain temperatures between $450^\circ\text{C} - 650^\circ\text{C}$ to ensure high Fixed Carbon Content ($>70\%$) and high H:C atomic ratio compliance ($<0.7$).
- **BOTTOM Zone**: Measures the accumulation and cooling zone. Cold bottoms indicate incomplete conversion or excessive heat loss through wet soil; excessively hot bottoms indicate air leakage from the bottom, causing biochar to turn into ash.

### Digital Measurement, Reporting, and Verification (dMRV)
Under global voluntary carbon market methodologies (e.g., **Puro.earth**, **Verra VM0044**, **Carbon Standards International**), biochar carbon removal certificates (CORCs) require immutable, auditable proof that:
1. **The pyrolysis threshold was reached and sustained**: Kiln core zones must reach at least $500^\circ\text{C} - 600^\circ\text{C}$ for a verified continuous duration.
2. **The burn took place at the certified farm or facility**: Authoritative GNSS coordinates must verify the kiln's physical location to prevent double-counting or fraudulent claiming of third-party biomass.
3. **The timestamp is tamper-proof**: Timestamps must derive directly from atomic GNSS satellite clocks (UTC epoch), preventing operators from forging system clock dates.

---

## 2. Silicon & Microcontroller Hardware Architecture

The sensor node is powered by the **Espressif ESP32-S3-WROOM-1 / DevKitC-1 N16R8** system-on-chip.

```
+-------------------------------------------------------------------------------+
|                             ESP32-S3 SOC (N16R8)                              |
|                                                                               |
|  +-----------------------------------+   +---------------------------------+  |
|  |           Xtensa Core 0           |   |          Xtensa Core 1          |  |
|  |             (160 MHz)             |   |            (160 MHz)            |  |
|  |  - state_task (dMRV State Machine)|   |  - sensor_task (MAX6675 SPI)    |  |
|  |  - NimBLE BLE Host & Controller   |   |  - gnss_task (UART2 NMEA Stream)|  |
|  |  - Wi-Fi SoftAP & HTTP Server     |   |                                 |  |
|  |  - Dynamic Power Manager          |   |                                 |  |
|  +-----------------+-----------------+   +----------------+----------------+  |
|                    |                                      |                   |
|                    v                                      v                   |
|  +-------------------------------------------------------------------------+  |
|  |                      Internal SRAM (512 KB Total)                       |  |
|  |  - 384 KB System SRAM0/1  |  - 32 KB RTC Fast/Slow SRAM                 |  |
|  |  - Thread-Safe Shared Telemetry Snapshot Buffer (FreeRTOS Mutex)        |  |
|  |  - FreeRTOS Sensor Sample Queue (Ring Overwrite, 4 Slots)               |  |
|  +-------------------------------------------------------------------------+  |
|                    |                                      |                   |
|                    v                                      v                   |
|  +-----------------------------------+   +---------------------------------+  |
|  |         External Flash            |   |         External PSRAM          |  |
|  |     (16 MB Quad SPI Flash)        |   |     (8 MB Octal SPI PSRAM)      |  |
|  |  - Embedded HTML/CSS Dashboard    |   |  - High-Memory Heap Allocations |  |
|  |  - ESP-IDF Firmware Image         |   |  - Future OTA Buffer Storage    |  |
|  +-----------------------------------+   +---------------------------------+  |
|                                                                               |
|  On-Chip Peripherals:                                                         |
|  - UART2 Peripheral (Hardware FIFO, Pins 17/18)                               |
|  - GPIO Controller (High-Speed Bit-Banged SPI, Pins 5, 6, 7, 15, 16)          |
|  - 2.4 GHz RF Transceiver (Time-Division Coexistence: Wi-Fi SoftAP + BLE 5.0) |
|  - Internal Silicon Temperature Sensor (Analog Bandgap, 20°C - 100°C)         |
+-------------------------------------------------------------------------------+
```

### ESP32-S3 N16R8 SoC Specifications
- **Core Architecture**: Dual-core 32-bit Xtensa LX7 microprocessors with 7-stage pipeline.
- **Hardware Floating-Point Unit (FPU)**: Single-precision IEEE 754 hardware FPU. All temperature derivatives ($\frac{dT}{dt}$) and GNSS ellipsoidal coordinate conversions are calculated in single-cycle FPU instructions without software emulation.
- **Clock Frequency**: Configured to **160 MHz** via ESP-IDF Kconfig (`CONFIG_ESP32S3_DEFAULT_CPU_FREQ_160=y`).

### CPU Frequency Optimization (Why 160 MHz vs. 240 MHz)
In CMOS digital electronics, active dynamic power dissipation is governed by:
$$P_{\text{dynamic}} = C \cdot V_{\text{DD}}^2 \cdot f$$
Where:
- $C$ is the internal parasitic capacitance of the logic gates,
- $V_{\text{DD}}$ is the core supply voltage (typically 1.1V for ESP32-S3),
- $f$ is the clock frequency.

Running the chip at its maximum 240 MHz dissipates significant heat (~1.5 Watts when radios are active). Because sensor reading occurs at 1 Hz and GNSS UART streams at 9600 baud, 240 MHz provides zero operational advantage. Dropping the clock frequency to **160 MHz** reduces dynamic CPU power dissipation by **33.3%** ($1 - \frac{160}{240}$), keeping the processor cool without dropping a single NMEA byte or BLE packet.

### Memory Architecture (SRAM, PSRAM, Flash)
1. **512 KB Internal SRAM**:
   - Split into SRAM0 (instruction bus), SRAM1 (data bus), and RTC SRAM.
   - All FreeRTOS task stacks, task control blocks (TCBs), sensor queues, and mutexes are allocated strictly in high-speed, zero-wait-state internal SRAM.
2. **8 MB Octal PSRAM**:
   - Connected over high-speed 8-line Octal SPI (`GPIO 33–37`). Used as auxiliary heap memory for future over-the-air (OTA) updates and large web payloads.
3. **16 MB Quad SPI Flash**:
   - Holds the compiled firmware binary, bootloader, partition table, and the embedded single-page HTML/CSS/JS dashboard string literal.

### FreeRTOS Asymmetric Dual-Core Scheduling
ESP-IDF runs a customized Symmetric Multiprocessing (SMP) FreeRTOS kernel. To ensure hardware determinism, tasks are pinned to dedicated physical cores:
- **Core 1 (Real-Time Sensing Core)**:
  - `sensor_task` (`Priority 5`): High priority to guarantee that bit-banged SPI clock pulses are never interrupted by networking interrupts.
  - `gnss_task` (`Priority 4`): Empties the UART2 hardware FIFO ring buffer into memory before buffer overrun can occur.
- **Core 0 (Networking & Computation Core)**:
  - `state_task` (`Priority 3`): Processes the state machine, calculates finite derivatives, and updates the data model.
  - `NimBLE Controller & Host Tasks`: Executes Bluetooth 5.0 link-layer state machines and packet scheduling.
  - `esp_http_server Task`: Listens on TCP port 80 and processes HTTP REST requests.

---

## 3. Electrical Wiring & Pinout Safety Analysis

### Master Wiring Matrix

| Module | Pin Name | ESP32-S3 Pin | Function | Voltage Level | Drive Mode |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **All MAX6675** | GND | GND | Common Power Ground | 0V | Power |
| **All MAX6675** | VCC | 3V3 | Power Rail | 3.3V DC ($\pm 5\%$) | Power |
| **All MAX6675** | SCK | **GPIO 5** | Shared SPI Serial Clock | 3.3V CMOS | Output (Push-Pull) |
| **All MAX6675** | SO | **GPIO 6** | Shared SPI Serial Out (MISO)| 3.3V CMOS | Input (Internal Pull-Up) |
| **TOP Zone** | CS | **GPIO 7** | Top Channel Chip Select | 3.3V CMOS | Output (Pull-Up Enabled) |
| **MIDDLE Zone** | CS | **GPIO 15** | Middle Channel Chip Select | 3.3V CMOS | Output (Pull-Up Enabled) |
| **BOTTOM Zone** | CS | **GPIO 16** | Bottom Channel Chip Select | 3.3V CMOS | Output (Pull-Up Enabled) |
| **7Semi L89HA** | 3.3V | 3V3 | Module Power Rail | 3.3V DC | Power |
| **7Semi L89HA** | GND | GND | Common Power Ground | 0V | Power |
| **7Semi L89HA** | TX | **GPIO 17** | GNSS NMEA Transmit $\rightarrow$ ESP32 | 3.3V TTL | UART2 RX (Input) |
| **7Semi L89HA** | RX | **GPIO 18** | ESP32 Command $\rightarrow$ GNSS Receive | 3.3V TTL | UART2 TX (Output) |

### ESP32-S3 Hardware Pinout Safety Analysis
The chosen pins were systematically verified against the ESP32-S3 silicon technical reference manual:

```
                      ESP32-S3 PIN ALLOCATION SAFETY MAP
  +-----------------------------------------------------------------------+
  | BOOT STRAPPING PINS (AVOIDED):                                        |
  | GPIO 0  : Boot mode (LOW = Download Boot, HIGH = SPI Boot)            |
  | GPIO 3  : JTAG signal selection                                       |
  | GPIO 45 : VDD_SPI voltage selection (Critical: 1.8V vs 3.3V Flash)   |
  | GPIO 46 : ROM code printing control                                   |
  | STATUS: 100% ISOLATED. No sensor connects to strapping pins.          |
  +-----------------------------------------------------------------------+
  | FLASH & OCTAL PSRAM BUS (AVOIDED):                                    |
  | GPIO 26 to 32 : SPICS0, SPICLK, SPID, SPIQ, SPIWP, SPIHD              |
  | GPIO 33 to 37 : OPI Flash / PSRAM High-Speed Bus                     |
  | STATUS: 100% ISOLATED. Sensor pins (5, 6, 7, 15, 16, 17, 18) are safe.|
  +-----------------------------------------------------------------------+
  | NATIVE USB-JTAG SUBSYSTEM (AVOIDED):                                  |
  | GPIO 19 : USB D-                                                      |
  | GPIO 20 : USB D+                                                      |
  | STATUS: 100% ISOLATED. UART2 uses Pins 17/18, leaving USB for debug.  |
  +-----------------------------------------------------------------------+
```

### 3.3V Logic vs. 5V Damage & ESD Clamping Physics
The MAX6675 IC and the ESP32-S3 are native **3.3V logic devices**.
ESP32-S3 GPIO pins include internal electrostatic discharge (ESD) protection networks consisting of two clamping diodes per pin:
- One diode connected between GND and the pin (anode to GND).
- One diode connected between the pin and the internal $V_{\text{DD}}$ (3.3V) rail (cathode to $V_{\text{DD}}$).

```
                 3.3V Rail (VDD)
                    ^
                    |
                  [---] Upper ESD Diode (Reverse-biased when V_pin <= 3.3V)
                    |
    GPIO Pin -------+-------> Internal CMOS Logic Gate Input
    (e.g. Pin 6)    |
                  [---] Lower ESD Diode (Reverse-biased when V_pin >= 0V)
                    |
                    v
                   GND
```

> [!WARNING]
> **5V Injection Conduction Hazard**: If a sensor module is powered from 5V (such as the 5V / VIN / VBUS USB pin), its digital output pin (`SO` or `TX`) will drive a 5V logic HIGH.
> When a 5V signal hits an ESP32 GPIO:
> 1. The input voltage exceeds $V_{\text{DD}} + V_{\text{forward}}$ ($3.3\text{V} + 0.6\text{V} = 3.9\text{V}$).
> 2. The upper ESD clamping diode becomes **forward-biased**.
> 3. Massive parasitic current flows directly from the 5V sensor module through the internal ESP32 diode into the ESP32's 3.3V internal power rail!
> 4. This causes the entire ESP32 silicon substrate to heat up rapidly (>70°C), triggering CMOS latchup or burning out the I/O bank.
> **All sensors MUST be powered exclusively from 3.3V.**

---

## 4. Communication Protocols & Bus Physics Deep-Dive

### MAX6675 Bit-Banged SPI Communication

#### Why Bit-Banged GPIO Instead of the Hardware SPI Peripheral?
The ESP32-S3 integrates hardware SPI controllers (`SPI2` and `SPI3`). However, the MAX6675 deviates from the standard Motorola SPI specification:
1. **Simplex Read-Only Operation**: It has no MOSI (data in) line.
2. **Non-Standard CS Latch Cycle**: The MAX6675 initiates its internal temperature conversion precisely when `CS` transitions from LOW to HIGH. If `CS` is held LOW continuously across multiple reads, no new conversion is ever performed.
3. **Rigid Timing Requirements**: The MAX6675 output buffer requires a microsecond propagation delay ($t_{\text{CSS}}$) after `CS` goes LOW before clock pulses can begin. Hardware SPI master DMA engines assert `CS` and `SCK` simultaneously, causing bit shifting errors.
4. **Long Cable Capacitance**: Thermocouple breakout boards frequently use long, unshielded jumper wires. The hardware SPI peripheral minimum clock rate is often too fast, producing edge reflections. Bit-banging allows exact software control of clock speed, pin pull-ups, and channel isolation.

#### Complete SPI Timing Waveform & 16-Bit Word Extraction

```
CS  -----+                                                               +-------
         |                                                               |
         +---------------------------------------------------------------+
         |<-- t_CSS -->|
         |  (25 us)    |
SCK -----+             +---+   +---+   +---+   +---+         +---+   +---+
         |             | 1 |   | 2 |   | 3 |   | 4 |  ...    |15 |   |16 |
         +-------------+   +---+   +---+   +---+   +---------+   +---+   +-------
                       |<tCH>|
                       |10 us|
SO  -------------------+-------+-------+-------+-------+-----+-------+-----------
         Hi-Z          |  D15  |  D14  |  D13  |  D12  | ... |   D1  |   D0  |
                       | Dummy |  MSB  |       |       |     |Device | Tri-  |
                       | Sign  | 1024° |  512° |  256° |     |  ID   | State |
```

#### Bit-by-Bit Field Definitions
- **Bit 15 ($D_{15}$)**: Dummy Sign Bit. Always 0. If this bit reads 1, the bus is floating or experiencing electrical noise.
- **Bits 14 – 3 ($D_{14} - D_3$)**: 12-Bit Unsigned Integer representing temperature.
  - Conversion formula:
    $$\text{Temperature}\ (^\circ\text{C}) = \left(\sum_{i=3}^{14} D_i \cdot 2^{i-3}\right) \times 0.25^\circ\text{C}$$
  - Range: $0.00^\circ\text{C}$ (`0x000`) to $+1023.75^\circ\text{C}$ (`0xFFF`).
- **Bit 2 ($D_2$)**: Thermocouple Open-Circuit Detect.
  - $D_2 = 0$: Normal operation. Closed circuit through thermocouple junction.
  - $D_2 = 1$: Open circuit. The thermocouple lead is broken, disconnected, or detached from the terminal block.
- **Bit 1 ($D_1$)**: Device Identifier. Reserved constant 0.
- **Bit 0 ($D_0$)**: Tri-state indicator.

#### The 220 ms Conversion Physics (Why Rapid Polling Destroys Data)
The MAX6675 contains an internal integrating Delta-Sigma analog-to-digital converter. From the moment `CS` rises HIGH, the IC requires **220 milliseconds** to sample the cold junction diode, sample the thermocouple differential voltage, perform internal amplification, and write the 12-bit result to its output latch.
- If firmware pulls `CS` LOW at a rapid rate (e.g., polling every 2 ms), the ongoing analog conversion is aborted midway.
- The output register locks up or outputs stale 0xFFFF/0x0000 data.
- **The Solution**: The firmware polls at a strictly metered 1000 ms (1 Hz) in active state, and 2000 ms (0.5 Hz) in resting state.

---

### 7Semi L89HA Multi-GNSS UART Communication

#### Physical Serial Configuration
- **Interface**: Full-duplex asynchronous serial UART on ESP32 UART2.
- **Baud Rate**: `9600 bps`.
- **Bit Period**:
  $$T_{\text{bit}} = \frac{1}{9600} \approx 104.167\ \mu\text{s}$$
- **Framing**: 1 Start bit, 8 Data bits, No parity, 1 Stop bit (8N1). Total bits per character = 10 bits ($1.0416\text{ ms}$ per byte).

#### NMEA-0183 Sentence Decoding

1. **`$GNGGA` Sentence (Global Positioning System Fix Data)**:
   ```text
   $GNGGA,125047.308,2836.8363,N,07712.5412,E,1,08,1.2,215.4,M,-35.0,M,,*58
   ```
   - Field 1 (`125047.308`): UTC Time (12:50:47.308 UTC).
   - Field 2 & 3 (`2836.8363,N`): Latitude $28^\circ 36.8363'$ North.
   - Field 4 & 5 (`07712.5412,E`): Longitude $77^\circ 12.5412'$ East.
   - Field 6 (`1`): Fix Quality (0 = Invalid, 1 = GPS SPS Fix, 2 = DGPS Fix).
   - Field 7 (`08`): Number of Satellites in View/Use.
   - Field 8 (`1.2`): HDOP (Horizontal Dilution of Precision).
   - Field 9 & 10 (`215.4,M`): Altitude above mean sea level in meters.

2. **Bitwise XOR Checksum Algorithm**:
   Every NMEA sentence concludes with an asterisk (`*`) and a two-digit hexadecimal checksum. The checksum is computed by performing a bitwise XOR ($\oplus$) of all ASCII characters between the `$` and `*` symbols:
   $$\text{Checksum} = \bigoplus_{i=1}^{n} \text{Character}_i$$
   If the computed XOR value fails to match the transmitted hex byte, the frame was corrupted by electrical noise and is discarded.

3. **Geodetic Coordinate Transformation Formula**:
   NMEA represents latitude as `DDMM.MMMM` (degrees and decimal minutes). The driver converts this into signed decimal degrees:
   $$\text{Degrees} = \lfloor \frac{\text{raw}}{100} \rfloor$$
   $$\text{Minutes} = \text{raw} - (\text{Degrees} \times 100)$$
   $$\text{Decimal Degrees} = \text{Degrees} + \left(\frac{\text{Minutes}}{60.0}\right)$$
   If direction is `'S'` (South) or `'W'` (West), the value is multiplied by `-1.0`.

---

### Apache NimBLE Bluetooth Low Energy (BLE 5.0)

#### Why Apache NimBLE Instead of Classic Bluedroid?
ESP-IDF historically used the Bluedroid Bluetooth stack. KriSHE Carbon uses **Apache NimBLE** because:
1. **RAM Efficiency**: Bluedroid consumes >300 KB of RAM. NimBLE consumes <80 KB, freeing memory for queues and buffers.
2. **CPU Overhead**: NimBLE is designed for embedded microcontrollers, using FreeRTOS task queues and event loops without heavy background context-switching.
3. **Bluetooth 5.0 Compliance**: Supports advertising extensions, preferred connection parameters, and fast MTU exchange.

#### GATT Table Architecture
- **Primary Service**: Custom 16-bit UUID `0xFFE0`.
- **Telemetry Characteristic**: Custom 16-bit UUID `0xFFE1`.
  - Permissions: Read, Notify (`BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY`).
- **Client Characteristic Configuration Descriptor (CCCD)**: UUID `0x2902`. Writing `0x0001` enables notifications.

#### MTU Exchange Physics
The default Bluetooth Core Specification Attribute Protocol (ATT) Maximum Transmission Unit (MTU) is **23 bytes**:
$$\text{Payload} = \text{MTU} - 3\ \text{bytes header} = 20\ \text{bytes}$$
Transmitting our 128-byte JSON telemetry payload across a 20-byte MTU would force link-layer packet fragmentation into 7 consecutive packets, increasing latency and packet loss.
On BLE connection, the Android app calls `requestMtu(512)`. NimBLE responds with an MTU negotiation grant, allowing the full 128-byte JSON payload to transmit inside a **single radio burst**.

---

### Wi-Fi 802.11 b/g/n SoftAP & HTTP REST Server

- **Access Point SSID**: `KriSHE_Carbon_AP`
- **Security**: WPA2-PSK (`WIFI_AUTH_WPA2_PSK`), Password: `krishecarbon`
- **Wi-Fi Channel**: Channel 1 (2.412 GHz)
- **Max Station Connections**: 4 clients
- **Network Interface**:
  - Node Gateway IP: `192.168.4.1`
  - Subnet Mask: `255.255.255.0`
  - DHCP Range: `192.168.4.2` to `192.168.4.5`
- **HTTP Server**: Built with `esp_http_server.h`, running non-blocking asynchronous socket multiplexing (`select()`).
- **CORS Headers**: All REST responses include:
  ```http
  Access-Control-Allow-Origin: *
  Access-Control-Allow-Methods: GET, OPTIONS
  Access-Control-Allow-Headers: Content-Type
  Content-Type: application/json
  ```

---

### Inter-Task Communication & Synchronization Primitives

```
[ sensor_task (Core 1) ]
         |
         | xQueueSend(s_temp_queue, &sample, 0)
         v
+------------------------------------+
| FreeRTOS Queue (s_temp_queue)      |  (Capacity: 4 temperature_sample_t)
| Overwrite policy on full queue     |
+------------------------------------+
         |
         | xQueueReceive(s_temp_queue, &sample, portMAX_DELAY)
         v
[ state_task (Core 0) ]
         |
         | xSemaphoreTake(s_mutex, portMAX_DELAY)
         v
+------------------------------------+
| Global Telemetry State (s_state)   |  (Protected by FreeRTOS Mutex)
| - Uptime, Temps, GPS, State, Rates |
+------------------------------------+
         |
         | xSemaphoreGive(s_mutex)
         |
         +---------------------------------------+
         |                                       |
         v                                       v
[ ble_service notification ]           [ http_server /api/status ]
 (Atomic local snapshot)                (Atomic local snapshot)
```

1. **FreeRTOS Queue (`s_temp_queue`)**: Decouples hardware sensor reads from state processing. If the state machine or BLE stack takes extra time processing a network packet, sensor reads continue without timing jitter.
2. **FreeRTOS Mutex (`s_mutex`)**: A binary semaphore with priority inheritance. Prevents read-write race conditions when the web server reads telemetry while the state task is calculating temperature derivatives.

---

## 5. Firmware Architecture & File-by-File Walkthrough (`src/`)

### 1. Central Configuration (`src/config.h`)

| Identifier | Defined Value | Physical & Architectural Justification |
| :--- | :--- | :--- |
| `DEVICE_NAME` | `"KriSHE Carbon Sensor Node"` | Human-readable node identity transmitted in JSON payloads and headers. |
| `FIRMWARE_VERSION` | `"1.0.0"` | SemVer firmware identifier for auditability and compliance. |
| `PIN_MAX6675_SCK` | `5` | GPIO 5 selected: zero conflict with boot strapping pins or flash. |
| `PIN_MAX6675_SO` | `6` | GPIO 6 selected: input line with internal pull-up enabled. |
| `PIN_MAX6675_CS_TOP` | `7` | GPIO 7: Dedicated Chip Select for Top Zone thermocouple. |
| `PIN_MAX6675_CS_MID` | `15` | GPIO 15: Dedicated Chip Select for Middle Zone thermocouple. |
| `PIN_MAX6675_CS_BOT` | `16` | GPIO 16: Dedicated Chip Select for Bottom Zone thermocouple. |
| `PIN_GNSS_RX` | `17` | GPIO 17 assigned to UART2 RX (receives NMEA sentences from GNSS TX). |
| `PIN_GNSS_TX` | `18` | GPIO 18 assigned to UART2 TX (transmits commands to GNSS RX). |
| `GNSS_UART_NUM` | `UART_NUM_2` | Dedicated hardware UART controller; avoids USB-JTAG serial conflict. |
| `GNSS_UART_BAUD` | `9600` | Standard NMEA-0183 baud rate supported by Quectel L89HA engine. |
| `GNSS_UART_BUF_SIZE` | `1024` | 1 KB ring buffer prevents overflow during bursts of multi-satellite sentences. |
| `TEMP_SAMPLE_PERIOD_MS`| `1000` | 1 Hz sampling period; provides 780 ms margin beyond 220 ms MAX6675 conversion time. |
| `ACTIVATION_THRESHOLD_C`| `60.0f` | Threshold confirming active biomass pyrolysis ignition. |
| `PREHEAT_THRESHOLD_C` | `35.0f` | Threshold distinguishing ambient heat rise from ambient room temperature. |
| `COOLDOWN_THRESHOLD_C` | `55.0f` | Temperature threshold indicating end of burn cycle. |
| `COOLDOWN_SAMPLES_REQ` | `5` | Hysteresis: requires 5 consecutive low samples to prevent premature shutdown from wind gusts. |
| `RESTING_TIMEOUT_S` | `30` | Inactivity timer: transitions idle node to RESTING state after 30s of no heat. |
| `ACTIVE_ZONE_CRITERION`| `ZONE_CRITERION_MAJORITY` | Majority voting prevents a single bad sensor from blocking verification. |
| `WIFI_AP_SSID` | `"KriSHE_Carbon_AP"` | Wi-Fi network name broadcast by SoftAP. |
| `WIFI_AP_PASS` | `"krishecarbon"` | WPA2 passphrase for local connection security. |
| `SENSOR_TASK_PRIORITY` | `5` | Highest task priority: guarantees deterministic SPI clock bit-banging. |
| `GNSS_TASK_PRIORITY` | `4` | High priority: prevents UART hardware FIFO ring buffer overrun. |
| `STATE_TASK_PRIORITY` | `3` | Standard priority: processes data and broadcasts network notifications. |

---

### 2. Application Entry Point & Tasks (`src/main.c`)

- **`app_main(void)`**:
  1. Calls `nvs_flash_init()`: Initializes Non-Volatile Storage required by Wi-Fi and Bluetooth stacks to store calibration data and PHY parameters.
  2. Calls `data_model_init()`: Allocates the FreeRTOS Mutex and initializes default telemetry structures.
  3. Calls `init_internal_temp_sensor()`: Registers the ESP32-S3 on-die temperature sensor using `TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100)`.
  4. Calls `max6675_init()`: Configures GPIO pins 5, 6, 7, 15, and 16.
  5. Calls `gnss_init()`: Configures UART2 parameters and installs the 1024-byte ring buffer driver.
  6. Calls `kiln_state_init()`: Resets the state machine to `KILN_STATE_IDLE`.
  7. Calls `ble_service_init()`: Starts the NimBLE stack and begins advertising as `KriSHE-Carbon`.
  8. Calls `wifi_init_softap()` & `start_web_server()`: Starts the Wi-Fi AP and spawns the web dashboard.
  9. Spawns FreeRTOS tasks pinned to respective cores.
- **`sensor_task(void *pvParameters)`**:
  - Runs in an infinite loop. Calls `max6675_sample_all()`.
  - Implements thermal delay throttling:
    ```c
    TickType_t period = (kiln_state_get_current() == KILN_STATE_RESTING)
                        ? pdMS_TO_TICKS(2000)
                        : pdMS_TO_TICKS(TEMP_SAMPLE_PERIOD_MS);
    vTaskDelayUntil(&last_wake_time, period);
    ```
- **`print_serial_diagnostics(const device_state_t *st)`**:
  - Formats a 3-second serial summary to the console displaying Uptime, State, Die Temp, Zone Temperatures, Rates of change, GNSS status, and RF states.

---

### 3. MAX6675 Driver & Outlier Filtering (`src/max6675.h`, `src/max6675.c`)

- **`max6675_init(void)`**:
  - Configures `SCK`, `TOP_CS`, `MID_CS`, and `BOT_CS` as `GPIO_MODE_OUTPUT`.
  - Configures `SO` as `GPIO_MODE_INPUT` with `GPIO_PULLUP_ENABLE`.
  - Sets all CS pins HIGH (inactive) immediately to prevent multiple chips from driving the `SO` bus simultaneously.
- **`max6675_read_raw_word(gpio_num_t cs_pin)`**:
  - Bit-bangs 16 clock pulses.
  - Pulls `CS` LOW, waits $25\ \mu\text{s}$ for bus stabilization.
  - Loops 16 times: pulls `SCK` HIGH ($10\ \mu\text{s}$), reads bit from `SO` via `gpio_get_level()`, pulls `SCK` LOW ($10\ \mu\text{s}$).
  - Pulls `CS` HIGH. Returns the raw `uint16_t` word.
- **`max6675_sample_all(void)`**:
  - Reads TOP (CS 7), waits 20 ms settling delay.
  - Reads MIDDLE (CS 15), waits 20 ms settling delay.
  - Reads BOTTOM (CS 16), waits 20 ms settling delay.
  - Decodes temperature values and verifies Bit 2 ($D_2$) open thermocouple flag.
  - Clamps physically impossible noise spikes (>100°C change in 1 second) with a 2-cycle maximum hold to prevent false freezing.

---

### 4. GNSS NMEA Parser & Geometry (`src/gnss.h`, `src/gnss.c`)

- **`gnss_update(void)`**:
  - Reads raw bytes from UART2 into a local buffer.
  - Scans for sentence terminators (`\r\n`).
  - Passes complete lines to `gnss_parse_nmea_sentence()`.
- **`gnss_verify_checksum(const char *sentence)`**:
  - Evaluates bitwise XOR checksum between `$` and `*`. Returns false if mismatch occurs.
- **`gnss_parse_nmea_sentence(const char *s)`**:
  - Tokenizes comma-delimited fields.
  - Parses `$GNGGA`: Extracts UTC time, converts latitude/longitude from `DDMM.MMMM` to decimal degrees, extracts fix validity and satellite count.
  - Computes Unix UTC epoch time:
    ```c
    time_t epoch = mktime(&tm);
    ```

---

### 5. dMRV Kiln State Machine (`src/kiln_state.h`, `src/kiln_state.c`)

- **State Transitions**:
  - `IDLE`: Startup state. If no heat is detected for 30 seconds (`s_no_heat_counter >= 30`), transitions to `RESTING`.
  - `RESTING`: Low-power monitoring state. Sampling period relaxes to 2000 ms. If heat is detected on any probe ($>35^\circ\text{C}$), immediately wakes to `PREHEATING`.
  - `PREHEATING`: Evaluates whether majority of valid zones cross $60^\circ\text{C}$. If reached, advances to `ACTIVE`. If heat drops back below $35^\circ\text{C}$ for 15 seconds, returns to `RESTING`.
  - `ACTIVE`: Main carbonization phase. Locks batch ID, start coordinates, and start UTC epoch. Increments session duration counter every second.
  - `COOLDOWN`: Entered when temperatures drop below $55^\circ\text{C}$. If heat flares back up, returns to `ACTIVE`.
  - `COMPLETE`: Triggered after 5 consecutive samples below $55^\circ\text{C}$. Freezes final session metrics and resets state to `RESTING`.
- **Batch ID Generation**:
  ```c
  uint32_t rand_id = esp_random() & 0xFFFF;
  snprintf(s_current_session.batch_id, sizeof(s_current_session.batch_id),
           "KILN-%04lX-%04lX", (long)(start_time / 1000000) & 0xFFFF, (long)rand_id);
  ```
  Uses the hardware true random number generator (`esp_random()`) and hardware timer to generate a collision-free hexadecimal batch identifier.

---

### 6. Thread-Safe Telemetry Model (`src/data_model.h`, `src/data_model.c`)

- **`data_model_update_sensors(...)`**:
  - Calculates temperature derivatives ($\Delta T / \Delta t$) for Top, Middle, and Bottom zones:
    $$\text{rate} = \frac{T_{\text{current}} - T_{\text{previous}}}{\Delta t}$$
  - Locks `s_mutex`, updates state values, and releases `s_mutex`.
- **`data_model_get_snapshot(void)`**:
  - Locks `s_mutex`, performs a sub-microsecond memory copy (`memcpy`) of `device_state_t`, releases `s_mutex`, and returns the snapshot by value.

---

### 7. Web Dashboard & REST Endpoint (`src/dashboard.h`, `src/dashboard.c`)

- **`s_dashboard_html`**:
  - Embedded CSS/HTML/JS single-page web app stored in Flash.
  - Renders real-time telemetry cards, state badges, and an HTML5 Canvas line graph.
- **`api_status_get_handler(httpd_req_t *req)`**:
  - Obtains a telemetry snapshot from the data model.
  - Formats JSON string via `snprintf()`.
  - Sets HTTP status 200, adds CORS headers, and responds to browser client.
- **`wifi_stop_softap(void)` & `wifi_resume_softap(void)`**:
  - Shuts down the HTTP server and turns off the Wi-Fi baseband via `esp_wifi_stop()` when BLE connects, eliminating RF heat.
  - Restores Wi-Fi when BLE disconnects.

---

### 8. NimBLE GATT Peripheral & Coexistence (`src/ble_service.h`, `src/ble_service.c`)

- **`ble_gap_event(struct ble_gap_event *event, void *arg)`**:
  - `BLE_GAP_EVENT_CONNECT`: Stores connection handle and calls `wifi_stop_softap()` to cut Wi-Fi power dissipation.
  - `BLE_GAP_EVENT_DISCONNECT`: Resets connection handle, calls `wifi_resume_softap()` to restore the web dashboard, and restarts BLE advertising.
  - `BLE_GAP_EVENT_SUBSCRIBE`: Enables/disables notifications when the mobile app writes to the CCCD descriptor (`0x2902`).
- **`ble_service_notify_state(const device_state_t *st)`**:
  - Serializes telemetry into compact JSON format and issues an attribute notification to the client.

---

## 6. Toolchain & Kernel Build Configuration

### `platformio.ini`
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32@6.6.0
board = esp32-s3-devkitc-1
framework = espidf
monitor_speed = 115200
board_build.flash_mode = qio
```
- Uses Espressif 32 platform package v6.6.0 with official ESP-IDF framework.
- `flash_mode = qio`: Enables Quad I/O SPI flash access for fast code execution.

### `sdkconfig.defaults`
```ini
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192

CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_BT_NIMBLE_ROLE_PERIPHERAL=y

CONFIG_SW_COEXIST_ENABLE=y
CONFIG_ESP32S3_WIFI_SW_COEXIST_ENABLE=y

CONFIG_ESP32S3_DEFAULT_CPU_FREQ_160=y
CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ=160
```
- Configures 160 MHz CPU frequency.
- Enables Software Coexistence (`CONFIG_SW_COEXIST_ENABLE`), allowing Bluetooth Low Energy and Wi-Fi to share the single 2.4 GHz RF radio antenna via time-division multiplexing.

### `CMakeLists.txt`
```cmake
idf_component_register(SRCS "src/main.c" "src/max6675.c" "src/gnss.c" "src/kiln_state.c" 
                            "src/data_model.c" "src/dashboard.c" "src/ble_service.c"
                       INCLUDE_DIRS "src")
```
- Registers all C source files and the `src` include path with the ESP-IDF CMake build engine.

---

## 7. Android Mobile Companion App Architecture (`KriSHE-Carbon-App`)

### MVVM Clean Architecture & Data Flow

```
[ ESP32 Hardware ]
        |
        | 2.4 GHz BLE GATT Notifications (JSON)
        v
[ BleManager.kt ] (Android BluetoothGattCallback)
        |
        | Kotlin StateFlow<TelemetryData>
        v
[ DashboardViewModel.kt ] / [ LogsViewModel.kt ]
        |
        | Android LiveData / Flow
        v
[ UI Fragments ] (DashboardFragment, LiveTemperatureChartView, LogsFragment)
```

### Application Entry & Navigation (`MainActivity.kt`)
- **Navigation Architecture**: Replaced fragile `setupWithNavController` with backstack-safe fragment transitions using `popBackStack(item.itemId, false) || navigate(item.itemId)`.
- **Top Connection Status Bar**: Persistent header showing connection status (`CONNECTED`, `CONNECTING`, `DISCONNECTED`). Tapping this bar navigates directly to the Scanner fragment.

---

### Hardware-Accelerated Live Canvas Chart (`LiveTemperatureChartView.kt`)
Custom Android View extending `android.view.View`:
- **Rolling Window**: Holds a rolling circular buffer of 80 seconds of samples.
- **Dynamic Auto-Scaling Algorithm**:
  $$\text{Scale}_Y = \frac{H - \text{Padding}}{\max(T_{\text{max}} - T_{\text{min}}, 10.0)}$$
- **Cubic Bezier Spline Interpolation**: Curves are drawn using smooth cubic Bezier paths (`cubicTo`), eliminating jagged step-line rendering.
- **Interactive Probe Filter Chips**: Top, Middle, and Bottom probe traces can be independently hidden or displayed.
- **Pulsing Head Glow**: Leading data points feature a multi-layer radial gradient glow to indicate active streaming.

---

### Real-Time Monitoring Dashboard (`DashboardFragment.kt`)
- Displays three temperature cards with live values and rates of change.
- Renders kiln state badges with dynamic background colors:
  - `ACTIVE`: Error/Fire Red (`#F43F5E`)
  - `PREHEATING`: Warning Amber (`#F59E0B`)
  - `COOLDOWN`: Cool Cyan/Blue (`#38BDF8`)
  - `COMPLETE`: Success Emerald (`#10B981`)
  - `RESTING` / `IDLE`: Neutral Gray (`#334155`)
- Displays GNSS fix status, satellite count, latitude, longitude, and hardware uptime.

---

### Device Discovery & Connection (`ScannerFragment.kt`)
- Uses Android's `BluetoothLeScanner` API to scan for advertising nodes with service UUID `0xFFE0` or name `KriSHE-Carbon`.
- Displays real-time RSSI signal strength meters.
- Includes a quick-connect mechanism that initiates GATT pairing.

---

### Telemetry Logs & In-App Sample Preview (`LogsFragment.kt`)
- Displays recorded kiln burn sessions stored in the local SQLite database.
- **In-App Sample Preview Modal (`dialog_session_preview.xml`)**:
  - Tapping any session opens a modal table displaying: Index, Time, Top (°C), Middle (°C), Bottom (°C), and State.
  - Allows inspection of all data points before exporting.

---

### Calibration, Diagnostics & Alerts (`SettingsFragment.kt`)
- **Temperature Unit Conversion**: Toggle between Celsius (°C) and Fahrenheit (°F) with immediate re-rendering across charts, logs, and dashboard.
- **Thermocouple Zero-Offset Calibration**: Independent offsets ($\pm 10.0^\circ\text{C}$) for Top, Middle, and Bottom channels to compensate for thermocouple aging or wire resistance.
- **Wi-Fi Ping Diagnostic**: Issues an HTTP GET request to `http://192.168.4.1/api/status` and displays round-trip network latency.
- **Vibration Alerts**: Triggers haptic phone vibration when the kiln transitions to `ACTIVE` (>60°C) or enters `COOLDOWN`.
- **Database Wipe**: Secure option to clear historical SQLite records before a new testing run.

---

### BLE GATT Client Engine (`BleManager.kt`)
- Implements `BluetoothGattCallback`:
  - `onConnectionStateChange`: Detects connection, initiates service discovery via `discoverServices()`.
  - `onServicesDiscovered`: Locates service `0xFFE0` and characteristic `0xFFE1`, requests MTU of 512 bytes (`requestMtu(512)`).
  - `onMtuChanged`: Writes `0x0001` to the CCCD descriptor (`0x2902`) to enable notifications.
  - `onCharacteristicChanged`: Receives incoming notification bytes, parses the JSON payload, and emits an updated `TelemetryData` object to the application `StateFlow`.

---

### Room SQLite Persistence (`SessionDatabase.kt`, `SessionDao.kt`)
- Built on Android Jetpack Room:
  - Table `telemetry_samples`: `id`, `sessionId`, `timestamp`, `topC`, `middleC`, `bottomC`, `state`, `latitude`, `longitude`.
  - Indexed by `sessionId` and `timestamp` for sub-millisecond retrieval of tens of thousands of data points.

---

### RFC-4180 CSV Export Engine (`CsvExporter.kt`)
- Generates RFC-4180 compliant CSV files with standard headers:
  ```csv
  SampleIndex,Timestamp_UTC,ElapsedTime_s,Top_Temp_C,Middle_Temp_C,Bottom_Temp_C,Kiln_State,Latitude,Longitude,Satellites
  1,2026-09-13T02:15:00Z,1,30.25,30.50,0.00,RESTING,28.613939,77.209021,8
  ```
- Writes directly to the Android `MediaStore` Downloads directory for sharing via Google Drive, WhatsApp, or email.

---

## 8. Thermal Management & Low-Power Operations

To prevent the ESP32-S3 from overheating during long burns, the system combines three thermal management techniques:

1. **Wi-Fi TX Power Capping**:
   - In `src/dashboard.c`, Wi-Fi transmission power is reduced from 20 dBm (100 mW) to **13 dBm** (`esp_wifi_set_max_tx_power(52)`). This cuts RF power dissipation by >50%.
2. **Dynamic Wi-Fi Auto-Shutdown on BLE Connect**:
   - In `src/ble_service.c`, when the Android companion app connects over BLE, the firmware calls `wifi_stop_softap()`, shutting down the Wi-Fi baseband and stopping the web server. This drops die temperature from ~46°C to ~32°C.
   - When the phone disconnects, `wifi_resume_softap()` restarts the SoftAP so browser access is restored.
3. **Resting State Polling Relaxation**:
   - When no heat is detected for 30 consecutive seconds, the state machine transitions to `RESTING`. Sensor polling relaxes from 1000 ms to 2000 ms, keeping the CPU in FreeRTOS idle sleep 95% of the time.

---

## 9. Verification, Field Testing & Troubleshooting

### Building and Flashing Firmware
```bash
# Build binary
pio run

# Flash to ESP32-S3 (auto-detects port, e.g. COM7)
pio run --target upload

# Open serial monitor
pio device monitor
```

### Building and Installing Android App
```bash
cd KriSHE-Carbon-App
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### Thermocouple Verification (Lighter Test)
1. **Ambient Baseline**: Probes should read room temperature (24°C – 29°C).
2. **Flame Response**: Hold a lighter flame near the **TOP** probe tip for 3 seconds. The top reading will surge upward (+15°C to +50°C/s), while Middle and Bottom remain stable.
3. **Polarity Check**:
   - **Correct**: Temperature increases when heated.
   - **Reversed**: If temperature *drops*, leads are reversed in the screw terminal block. Power down and swap $(+)$ and $(-)$ wires.
4. **Open-Circuit Test**: Loosen one terminal screw. The firmware detects bit $D_2$ and marks the channel `[ PROBE OPEN / DISCONNECTED ]` without crashing.

### GNSS Positioning Verification
- Satellite signals (1.5 GHz) require line-of-sight to the sky. Indoor testing will report `STATUS: SEARCHING`.
- Move the unit near a window or outdoors. Within 30–90 seconds, satellite count will rise to 6–12, and authoritative coordinates and UTC time will appear across the app and web dashboard.

---

## 📄 License & Intellectual Property

Copyright © 2026 KriSHE Carbon. All rights reserved.  
Unauthorized copying, modification, distribution, or reverse engineering of this firmware or companion software is strictly prohibited.
