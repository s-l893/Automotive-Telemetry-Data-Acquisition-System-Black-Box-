# 🏎️ Automotive Black Box V2 — Custom-PCB Vehicle Telemetry Logger

A ground-up redesign of my [Automotive Black Box](https://github.com/s-l893/Automotive-Telemetry-Data-Acquisition-System-Black-Box-) vehicle data logger. V2 moves from a breadboard prototype to a **custom Altium-designed PCB**, from a simple application loop to a **non-blocking, interrupt-driven bare-metal architecture**, and from a small OLED to a **touchscreen dashboard**. It logs CAN bus traffic, GPS position/speed, and IMU acceleration to an SD card, with a firmware-enforced safety model designed around the mistakes V1 taught me.

![Project Status](https://img.shields.io/badge/status-in%20development-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-STM32F446RE-orange)
![Firmware](https://img.shields.io/badge/firmware-bare--metal%20C-lightgrey)

---

## 📋 Table of Contents

- [Overview](#overview)
- [What's New in V2](#whats-new-in-v2)
- [Project Status](#project-status)
- [Hardware](#hardware)
- [Firmware Architecture](#firmware-architecture)
- [Design Decisions](#design-decisions)
- [Fault Handling](#fault-handling)
- [Log Format](#log-format)
- [Build & Flash](#build--flash)
- [Safety Notes](#safety-notes)
- [Bring-Up Log: Bugs & Lessons](#bring-up-log-bugs--lessons)
- [Planned UI](#planned-ui)
- [Roadmap](#roadmap)
- [License](#license)
- [Contact](#contact)

---

## 🎯 Overview

V1 proved the concept: it logged RPM (CAN ID `0x158`), GPS, and accelerometer data from a 2016 Honda Accord V6 and was validated on real test drives. It also taught me hard lessons — most notably a floating CAN TXD line that held the bus dominant and caused ECU contention resulting in extreme battery drain ~7V.

V2 is the "do it properly" iteration:

- **Custom hardware** — schematic capture and layout in Altium Designer, fabricated by JLCPCB, hand-soldered, with real automotive input protection
- **Robust firmware** — non-blocking superloop, interrupt-driven CAN RX, lock-free ring buffers, an FSM for control flow, and a hardware watchdog
- **Safety by construction** — the CAN peripheral is configured so the logger cannot disturb the vehicle bus
- **Touchscreen UI** — live data and page navigation on an ILI9341 display with XPT2046 touch (in progress)
- **Generalized CAN capture** — no hardcoded Honda-specific RPM parsing; frames are captured by ID so the logger isn't tied to one vehicle

---

## 🆕 What's New in V2

| Area            | V1                            | V2                                                              |
| --------------- | ----------------------------- | --------------------------------------------------------------- |
| **Board**       | Breadboard + Nucleo           | Custom PCB (Altium → JLCPCB), OBD-II powered, protected input   |
| **Display**     | SSD1306 OLED 128×64           | ILI9341 320×240 SPI TFT + XPT2046 touch                         |
| **GPS**         | NEO-6M                        | NEO-M8N, interrupt-driven NMEA parsing                          |
| **CAN**         | Hardcoded Honda RPM (`0x158`) | Generalized per-ID frame capture (per-ID decode in progress)    |
| **Logging**     | Fixed 2 Hz CSV                | Per-CAN-frame rows + IMU/GPS fields, with a 200 ms sentinel row |
| **Reliability** | Fuse + graceful degradation   | Watchdog, fault flags, soft/hard fault split, silent-mode CAN   |
| **Shutdown**    | Manual (button / unplug)      | Inferred from CAN-bus silence (no voltage-sense circuit needed) |

---

## 📊 Project Status

| Subsystem                         | Status                                                                                                          |
| --------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| CAN RX → ring buffer → SD logging | ✅ Validated end-to-end on hardware                                                                             |
| FatFs SD driver (SPI)             | ✅ Hand-written `user_diskio.c`; validated on breadboard                                                        |
| IMU (MPU6050)                     | ✅ Driver complete and hardware-validated, integrated into log rows                                             |
| GPS (NEO-M8N)                     | ✅ Driver validated; RTC sync; lat/lon/speed in CSV                                                             |
| Display (ILI9341)                 | ✅ Hand-written driver with software landscape rotation                                                         |
| Touch (XPT2046)                   | 🟡 Driver + tap/swipe classification done; on-hardware calibration pending                                      |
| Fault management + IWDG           | ✅ Implemented                                                                                                  |
| CAN per-ID decode                 | 🔲 Currently passive frame capture; real signal decode planned                                                  |
| CAN bus-off recovery              | 🟡 Code written, not yet validated with a forced bus-off                                                        |
| Touchscreen UI / screens          | ✅ Complete, however still has some empty spots due to the sheer difficulty of finding the correct CAN PID code |

**Resource usage (F446RE, as of Sep 14, 2026):** ~10.7 KB / 128 KB RAM (8.4 %), ~63 KB / 512 KB Flash (12.3 %) — plenty of headroom for the UI.

---

## 🔧 Hardware

### Components

| Component           | Part                      | Interface     | Purpose                           |
| ------------------- | ------------------------- | ------------- | --------------------------------- |
| **MCU**             | STM32F446RE (Nucleo-64)   | —             | Main processor                    |
| **CAN transceiver** | SN65HVD230                | CAN1          | Vehicle bus interface             |
| **IMU**             | MPU6050                   | I²C1          | Acceleration (accel-only in use)  |
| **GPS**             | NEO-M8N                   | UART4 @ 9600  | Position, speed, time (NMEA 0183) |
| **Display**         | ILI9341 (SPI TFT)         | SPI1          | Dashboard / status                |
| **Touch**           | XPT2046                   | SPI2          | Tap and swipe input               |
| **Storage**         | microSD (FAT32 via FatFs) | SPI1 (shared) | Session logging                   |
| **Buck converter**  | LM2596                    | —             | 12 V → ~5 V                       |
| **LDO**             | AMS1117-3.3               | —             | 5 V → 3.3 V                       |
| **Debug**           | ST-LINK VCP               | USART2        | Serial debug output               |

Display `DC` / `RESET` are on `PA2` / `PA3`. The display and SD card share SPI1; each driver re-asserts its own SPI mode at the start of every transaction, so they can coexist without a manual bus-reconfiguration step.

### Power & Protection

```
OBD-II 12V ─► [ Blade fuse │ SMAJ15A TVS │ P-MOSFET reverse-polarity ] ─► LM2596 buck ─► ~5V rail ─► AMS1117-3.3 ─► 3.3V
```

- **Reverse-polarity protection:** P-channel MOSFET
- **Transient protection:** SMAJ15A TVS diode
- **Overcurrent protection:** automotive blade fuse
- **Decoupling** on all ICs
- **CAN termination:** intentionally omitted — the vehicle bus is already terminated

> ⚠️ **Known limitation (this board revision):** there is no protection between the LM2596 output and the STM32/ST-LINK rail. Powering the board over USB while the 12 V input is dead can backfeed the buck converter and damage it. See [Safety Notes](#safety-notes).

### PCB

Designed in **Altium Designer**, fabricated by **JLCPCB**, and hand-soldered. Two revisions have been built; see the [bring-up log](#bring-up-log-bugs--lessons) for what each revision taught me.

---

## 🏗️ Firmware Architecture

V2 is a **bare-metal, non-blocking superloop**. Nothing in the main loop waits on a peripheral; ISRs only move bytes and set flags, and the loop consumes them.

```
                ┌──────────────────────── ISRs ───────────────────────┐
  CAN RX (FIFO) ─► can_ring_buffer (SPSC)                              │
  UART4 RX (1B) ─► NMEA line buffer ─► nmea_parse_buffer + ready flag  │
                └──────────────────────────────────────────────────────┘
                                   │
                                   ▼
 ┌────────────────────────── Superloop ──────────────────────────┐
 │  IWDG refresh                                                 │
 │  fsm_sys (5-state system FSM)                                 │
 │    ├─ imu_read()            (I²C poll)                        │
 │    ├─ GPS_Driver_Update()   (parse RMC, fault timeout)        │
 │    ├─ Touch_Update()        (SPI2 poll, tap/swipe classify)   │
 │    ├─ fault checks          (CAN silence, GPS, touch, IMU)    │
 │    └─ SD_Logger_DrainCAN()  (ring buffer → CSV → FatFs)       │
 └───────────────────────────────────────────────────────────────┘
```

### Modules

| Module                 | Responsibility                                                            |
| ---------------------- | ------------------------------------------------------------------------- |
| `ring_buffer.c/.h`     | Generic single-producer/single-consumer ring buffer                       |
| `can_ring_buffer.c/.h` | CAN-frame ring buffer filled from the RX interrupt                        |
| `can_handler.c/.h`     | CAN configuration, RX handling, silence timeout, bus-off recovery         |
| `fsm_sys.c`            | Five-state system finite state machine                                    |
| `fault.c/.h`           | Fault flags and severity handling                                         |
| `sd_logger.c/.h`       | FatFs lifecycle, filename generation, CSV row formatting and drain        |
| `user_diskio.c`        | Hand-written SD-over-SPI FatFs disk driver                                |
| IMU driver             | MPU6050 init, `WHO_AM_I` handshake, calibration, offset-corrected reads   |
| `gps_driver.c/.h`      | Interrupt-driven NMEA receive, RMC parsing, decimal-degree conversion     |
| LCD driver             | ILI9341 init, windowing, chunked fills, logical-coordinate rotation layer |
| `touch_driver.c/.h`    | XPT2046 reads, pixel mapping, tap/swipe classification                    |

---

## 🧠 Design Decisions

**Bare-metal superloop, no RTOS.** The workload, which is a handful of periodic polls plus interrupt-fed buffers doesn't justify scheduler overhead or the concurrency hazards that come with it. I'm saving the RTOS for my FOC motor-controller project, where hard real-time scheduling actually matters.

**CAN peripheral can't disturb the vehicle.** After V1's bus-flood incident, the CAN handler enforces a silent mode in firmware so the logger never drives the bus, independent of what the wiring does.

**CAN Passive Listener over Active CAN PID Request** I heavily considered changing this project to do both last minute due to the new finding that ATF temps and VCM (Variable Cylinder Management)engine status may require the MCU to send requests to the ECU, however I ultimately chose not to due to the low reliability of my CAN PID source.

**Shutdown inferred from CAN silence.** Instead of adding a voltage-sense/ADC circuit, the firmware treats 2500 ms without a CAN frame as ignition-off and shuts down the log cleanly.

**Soft vs. hard faults.** Core CAN→SD logging is what matters. If the IMU fails to initialize, that's a _soft_ fault: logging continues without it rather than halting the whole system.

**Sentinel rows keep sensor data flowing.** When the CAN bus is quiet, a sentinel row (`id = 0xFFFF`, `dlc = 0`) is emitted every 200 ms so GPS and IMU data still get logged.

**ISR ↔ main-loop handoff via flag + stable copy.** The GPS ISR assembles a line in a fixed 85-byte buffer, copies the completed sentence into a separate parse buffer, then resets its index and sets a ready flag that the consumer clears. The same producer/consumer pattern fixed the CAN flag bug described below.

**Parse RMC only.** RMC alone carries fix validity, so it's sufficient for position, speed, and time without the extra parsing cost of GGA.

**Software rotation for the display.** The specific ILI9341 clone on my module ignores the documented `MADCTL` MV (row/column exchange) bit, so landscape is achieved in software: a `LCD_SetWindow_Logical()` layer maps a 320×240 logical space onto the panel's native 240×320 addressing.

---

## 🚨 Fault Handling

| Fault                | Trigger                                            | Severity | Effect                                          |
| -------------------- | -------------------------------------------------- | -------- | ----------------------------------------------- |
| **CAN silence**      | No CAN frame for 2500 ms                           | —        | Interpreted as ignition-off → clean stop        |
| **GPS fault**        | No NMEA line at all for 1.5 s (module not talking) | Soft     | Distinct from "alive but no fix" (`gps.locked`) |
| **Touch fault**      | Touch held/stuck for 12 s                          | Soft     | Flagged for the UI                              |
| **IMU init failure** | `WHO_AM_I` mismatch / no response                  | Soft     | Logging continues without IMU data              |
| **Watchdog (IWDG)**  | Main loop stalls                                   | Hard     | MCU reset                                       |

The GPS timeout is stamped in the UART RX ISR on every complete line — _before_ sentence-type filtering — so it answers "is the module talking?" rather than "does it have a lock?".

---

## 📝 Log Format

Sessions are logged as CSV to a FAT32 SD card. Each row contains the CAN frame fields, followed by GPS fields (latitude, longitude, speed) and IMU fields (Ax, Ay, Az). GPS and IMU columns appear in the same order in both row types:

- **CAN-frame rows** — written per received frame
- **Sentinel rows** — `id = 0xFFFF`, `dlc = 0`, written every 200 ms of CAN silence so GPS/IMU data keep flowing

> **Toolchain note:** float formatting via `snprintf` requires newlib-nano float support — add `-u _printf_float` under _MCU GCC Linker → Miscellaneous_.

---

## 🛠️ Build & Flash

**Requirements:** STM32CubeIDE, STM32CubeMX (bundled), an ST-LINK (on the Nucleo), a serial terminal (PuTTY etc.) for debug output on USART2.

1. **Clone the repository**

   ```bash
   git clone https://github.com/s-l893/<your-v2-repo>.git
   cd <your-v2-repo>
   ```

2. **Open in STM32CubeIDE** — _File → Open Projects from File System_, select the project folder.

3. **Check the CubeMX configuration**
   - Enable the **UART4 global interrupt** in NVIC (GPS reception depends on it)
   - Confirm SPI1 (display + SD) and SPI2 (touch) are enabled

4. **Add the linker flag** `-u _printf_float` (see [Log Format](#log-format)).

5. **Build** (Ctrl+B) and **flash** (Run → Debug, F11).

6. **Open a serial terminal** on the ST-LINK virtual COM port to watch debug output.

---

## ⚠️ Safety Notes

- **Power via USB only on the current board revision.** Because of the missing output-side protection on the LM2596, do **not** have USB and the 12 V OBD-II input live at the same time. Until the fix lands, the OBD/CAN transceiver side is left unpowered/unwired during bench work.
- **Planned fix:** a correctly oriented diode between the LM2596 output and the STM32 rail to diode-OR the two power sources.
- **The logger must never transmit on the vehicle bus.** Keep the firmware's CAN silent-mode configuration intact and always keep a fuse in the ground/power path when connecting to a real vehicle.
- Only use on vehicles you own or have permission to work on, and follow local regulations around OBD-II access.

**This is an educational project. The author assumes no liability for damage, injury, or legal issues arising from its use.**

---

## 🔍 Bring-Up Log: Bugs & Lessons

Hardware bring-up taught me more than the firmware did. The notable ones:

| Problem                                               | Root cause                                                                                                                   | Fix / Lesson                                                                                                       |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **Rev 1: blown fuses + smoke**                        | LM2596 buck module had an internally failed switch — near-0 Ω short between IN+ and OUT+                                     | Parked rev 1; continued on breadboard + bare Nucleo. Always check a power module before mating it to the board.    |
| **Rev 1: I²C SDA/SCL swapped**                        | Hardware I²C1 pin roles are fixed in silicon; I routed them crossed                                                          | Cut traces, cross with jumper wires. Ultimately led to PCB redesign. **Check AF pin roles before routing.**        |
| **Rev 2: mirrored DB9 footprint**                     | Footprint orientation error                                                                                                  | Custom crossover adapter cable                                                                                     |
| **Altium / fab issues**                               | TVS orientation error, CAN transceiver `VREF` miswired, vault-component locking, binary-format drill file rejected by JLCPCB | Fixed in schematic/library; used a local integrated library; exported ASCII drill files                            |
| **Display DC/RESET dead**                             | `PA2`/`PA3` are solder-bridged to USART2 on the Nucleo                                                                       | Bus-prep routine to release USART2 first. **Don't pick pins that silently double as other peripherals' defaults.** |
| **Display landscape broken**                          | ILI9341 clone ignores `MADCTL` MV bit                                                                                        | Software coordinate-rotation layer, verified with a 4-corner test                                                  |
| **CAN silence timeout never fired**                   | `can_frame_received_flag` was never cleared                                                                                  | Clear the flag on consumption — same pattern later reused for GPS                                                  |
| **GPS stalled / garbled**                             | Wrong UART handle, bad `strtok` delimiters, no overflow recovery, non-RMC sentences fed to parser                            | Correct `huart4`, RMC filter, overflow recovery, UART-instance guard in the shared RX callback                     |
| **SD module dead on PCB (MISO stuck low)**            | Module's VHCT125 buffer needs 4.5–5.5 V but its VCC was tapped from the 3.3 V LDO output                                     | Cut trace, bodge-wired buffer VCC to the raw ~5 V rail. **Read the datasheet for every chip on a "module."**       |
| **Rev 1 SD intermittent**                             | Intermittent SCK line (likely PCB damage or lifted pad)                                                                      | Bodge wire for SCK                                                                                                 |
| **Debugging SD with a logic analyzer showed nothing** | Card wasn't physically connected during captures (MISO held high by MCU internal pull-up)                                    | Confirm physical connectivity before blaming firmware                                                              |

---

## 🖥️ UI

Dash-style layout on the 320×240 touchscreen:

- Transmission / engine coolant temperature
- Gear + RPM, with a shift lights near redline
- G-force (lateral, longitudinal) number display
- Max acceleration / cornering stats
- Fault / status box
- GPS locked/unlocked
- VCM active/inactive

Touch priorities: **page navigation via swipe left/right** first (telemetry / GPS / fault status), then manual session start/stop, tap-to-acknowledge faults, and live config toggles if time allows.

> Some of these readouts (gear, temperatures) depend on the CAN per-ID decode work below.

---

## 🔮 Roadmap

**Firmware**

- [ ] On-hardware touch calibration (replace theoretical full-range mapping with measured corner values)
- [ ] CAN per-ID decode for real vehicle signals (RPM, etc.), configurable rather than hardcoded
- [ ] Validate CAN bus-off recovery with a deliberately forced bus-off
- [ ] `fsm_ui.c`, font table + `draw_string`, circle primitive, screen layouts
- [ ] Display / SD SPI bus arbitration under real load

**Hardware**

- [ ] Diode-OR between LM2596 output and STM32 rail (fix the USB backfeed)
- [ ] Resolve the remaining SD interface fault on the rev-2 PCB
- [ ] Next PCB revision incorporating all bodge-wire fixes and overall more space efficient design

**Stretch**

- [ ] On-device graphing / session review
- [ ] Standard OBD-II PID support
- [ ] Multi-vehicle CAN configuration

---

## 📄 License

MIT — see [LICENSE](LICENSE).

---

## 📧 Contact

**Sunny Lin** — sunnylin893@gmail.com

V1 repository: [Automotive-Telemetry-Data-Acquisition-System-Black-Box-](https://github.com/s-l893/Automotive-Telemetry-Data-Acquisition-System-Black-Box-)
