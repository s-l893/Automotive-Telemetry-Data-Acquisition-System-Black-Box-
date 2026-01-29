# 🏎️ Automotive Black Box - Real-Time Vehicle Telemetry System

An embedded systems project that transforms an STM32 microcontroller into a comprehensive automotive data logger, capturing real-time telemetry from a 2016 Honda Accord V6 and generating interactive visualizations.

![Project Status](https://img.shields.io/badge/status-in%20development-yellow)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-STM32F446RE-orange)

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware](#hardware)
- [System Architecture](#system-architecture)
- [Installation](#installation)
- [Usage](#usage)
- [Data Visualization](#data-visualization)
- [Project Structure](#project-structure)
- [Future Enhancements](#future-enhancements)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## 🎯 Overview

This project implements a professional-grade automotive black box system that captures, logs, and visualizes vehicle telemetry data. Originally developed as a learning project to understand embedded systems, CAN bus communication, and real-time data processing, it has evolved into a comprehensive telemetry solution.

### Why This Project?

- **Learn by doing**: Hands-on experience with automotive protocols (CAN bus, OBD-II)
- **Real-world application**: Working with production vehicle systems
- **Data science integration**: Bridge embedded systems with data visualization
- **Portfolio showcase**: Demonstrates full-stack embedded development skills

---

## ✨ Features

### Current Implementation

- ✅ **CAN Bus Interface**: Read real-time engine data from Honda J35 V6 ECU
- ✅ **Multi-Sensor Fusion**:
  - MPU6050 accelerometer (±2g range, I²C)
  - NEO-6M GPS module (UART, NMEA parsing)
  - SD card data logging (FAT32, SPI)
  - SSD1306 OLED display (128×64, I²C)
- ✅ **Real-Time Data Logging**: 2Hz sampling rate to SD card (CSV format)
- ✅ **VTEC Detection**: Automatic detection of Honda VTEC engagement (>5150 RPM)
- ✅ **Interactive Heatmaps**: Python-based visualization showing RPM, speed, and G-force intensity
- ✅ **Safety Features**: Hardware fuse protection, error handling, graceful degradation

### Data Channels Captured

| Channel      | Description               | Source         | Range      |
| ------------ | ------------------------- | -------------- | ---------- |
| **RPM**      | Engine speed              | CAN (ID 0x158) | 0-7000     |
| **Speed**    | Vehicle velocity          | GPS            | 0-200 km/h |
| **Ax**       | Longitudinal acceleration | MPU6050        | ±2g        |
| **Ay**       | Lateral acceleration      | MPU6050        | ±2g        |
| **Az**       | Vertical acceleration     | MPU6050        | ±2g        |
| **VTEC**     | VTEC system status        | CAN (derived)  | 0/1        |
| **Throttle** | Accelerator position      | CAN (planned)  | 0-100%     |
| **Location** | GPS coordinates           | NEO-6M         | WGS84      |

---

## 🔧 Hardware

### Bill of Materials

| Component           | Part Number                      | Quantity | Purpose                 | Cost (approx) |
| ------------------- | -------------------------------- | -------- | ----------------------- | ------------- |
| **Microcontroller** | STM32F446RET6 (Nucleo-64)        | 1        | Main processor          | $25           |
| **CAN Transceiver** | SN65HVD230                       | 1        | CAN bus interface       | $3            |
| **Accelerometer**   | MPU6050                          | 1        | G-force measurement     | $5            |
| **GPS Module**      | NEO-6M                           | 1        | Position/speed tracking | $12           |
| **Display**         | SSD1306 OLED (128×64)            | 1        | Real-time data display  | $8            |
| **Storage**         | MicroSD card + adapter           | 1        | Data logging            | $8            |
| **Misc**            | OBD-II cable, fuse holder, wires | -        | Connectivity            | $15           |
|                     |                                  |          | **Total**               | **~$76**      |

### Pinout Configuration

```
STM32F446RE Connections:
├─ CAN Bus
│  ├─ PA11: CAN1_RX  → SN65HVD230 TX
│  └─ PA12: CAN1_TX  → SN65HVD230 RX
├─ I²C1 (MPU6050 + OLED)
│  ├─ PB8: I2C1_SCL
│  └─ PB9: I2C1_SDA
├─ SPI1 (SD Card)
│  ├─ PA5: SPI1_SCK
│  ├─ PA6: SPI1_MISO
│  ├─ PA7: SPI1_MOSI
│  └─ PA4: SD_CS (chip select)
├─ UART4 (GPS)
│  ├─ PA0: UART4_TX
│  └─ PA1: UART4_RX
└─ Debug
   └─ PA2/PA3: USART2 (USB virtual COM port)
```

### Wiring Diagram

```
                    ┌─────────────────┐
                    │   STM32F446RE   │
                    │   (Nucleo-64)   │
                    └─────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
   ┌────▼────┐         ┌────▼────┐        ┌────▼────┐
   │ MPU6050 │         │SSD1306  │        │SN65HVD230│
   │(I²C)    │         │(I²C)    │        │(CAN)     │
   └─────────┘         └─────────┘        └────┬─────┘
                                               │
        ┌──────────────────┬──────────────────┘
        │                  │
   ┌────▼────┐        ┌────▼────┐
   │ NEO-6M  │        │OBD-II   │
   │ (UART)  │        │Port     │
   └─────────┘        └─────────┘
        │
   ┌────▼────┐
   │ SD Card │
   │ (SPI)   │
   └─────────┘
```

---

## 🏗️ System Architecture

### Software Stack

```
┌─────────────────────────────────────────┐
│         Application Layer               │
│  (Data logging, GPS parsing, display)   │
├─────────────────────────────────────────┤
│         HAL Driver Layer                │
│  (CAN, I²C, SPI, UART, GPIO, RTC)       │
├─────────────────────────────────────────┤
│         Hardware Abstraction            │
│  (STM32 HAL, CMSIS)                     │
└─────────────────────────────────────────┘
```

### Data Flow

```
CAN Bus (Car) ──┐
GPS Module ─────┼──> STM32 ──> Processing ──> SD Card (CSV)
MPU6050 ────────┘              │
                               └──> OLED Display

SD Card (CSV) ──> Python Script ──> Interactive HTML Map
```

---

## 📦 Installation

### Prerequisites

**Hardware:**

- STM32 Nucleo-F446RE development board
- Components from [Bill of Materials](#bill-of-materials)
- 2016 Honda Accord V6 (or compatible Honda vehicle)

**Software:**

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html) (for firmware)
- [Python 3.8+](https://www.python.org/downloads/) (for visualization)
- Git

### Firmware Setup

1. **Clone the repository:**

   ```bash
   git clone https://github.com/yourusername/automotive-black-box.git
   cd automotive-black-box
   ```

2. **Open in STM32CubeIDE:**
   - File → Open Projects from File System
   - Select the `BlackBox_Final` folder
   - Build the project (Ctrl+B)

3. **Flash to STM32:**
   - Connect Nucleo board via USB
   - Run → Debug (F11) or Run (Ctrl+F11)

### Visualization Setup

1. **Create Python virtual environment:**

   ```bash
   cd tools
   python -m venv map_env
   ```

2. **Activate environment:**

   ```bash
   # Windows
   .\map_env\Scripts\activate

   # macOS/Linux
   source map_env/bin/activate
   ```

3. **Install dependencies:**
   ```bash
   pip install pandas folium branca numpy
   ```

---

## 🚀 Usage

### 1. Hardware Connection

**⚠️ CRITICAL: Follow safety precautions**

1. **Connect CAN transceiver:**
   - SN65HVD230 CANH → OBD-II Pin 6
   - SN65HVD230 CANL → OBD-II Pin 14
   - OBD-II Pin 5 (GND) → **1A FUSE** → STM32 GND
   - **DO NOT connect Pin 16 (12V)**

2. **Power the system:**
   - STM32 powered via USB from laptop
   - Keep laptop on battery power (avoid ground loops)

3. **Mount sensors:**
   - MPU6050: Secure to flat surface in car
   - GPS: Position with clear sky view
   - OLED: Visible to driver (optional)

### 2. Data Collection

1. **Start the car** (engine running)
2. **Plug OBD-II connector** into car's port
3. **Monitor OLED display:**
   - Should show RPM updating in real-time
   - G-force readings during acceleration/braking
   - GPS status indicator

4. **Drive normally** - data logs automatically to SD card

5. **Stop logging:**
   - Press blue button on Nucleo (PC13) to stop
   - Or unplug OBD-II connector

### 3. Data Visualization

1. **Remove SD card** from STM32

2. **Copy CSV file** to `data/` folder:

   ```bash
   cp /path/to/sdcard/MMDDHHSS.csv ./data/
   ```

3. **Generate test data** (optional, for development):

   ```bash
   python data_sim.py
   ```

4. **Create heatmap:**

   ```bash
   python map_gen.py
   ```

5. **Open HTML file** in browser:
   ```bash
   # Output: data/your_file_RPM_heatmap.html
   ```

### Visualization Options

Generate different data channel visualizations:

```python
# In map_gen.py, change data_channel parameter:
generate_heatmap(data_path, data_channel='RPM')   # Engine speed
generate_heatmap(data_path, data_channel='Spd')   # Vehicle speed
generate_heatmap(data_path, data_channel='Ax')    # Acceleration
generate_heatmap(data_path, data_channel='Ay')    # Cornering
```

---

## 📊 Data Visualization

The Python visualization tool generates interactive heatmaps showing driving intensity:

### Color Schemes

**RPM Heatmap (Engine Load):**

- 🟢 **Green**: Cruising (0-3000 RPM)
- 🟡 **Yellow**: Moderate acceleration (3000-5000 RPM)
- 🔴 **Red**: High performance (5000-7000 RPM, VTEC engaged)

**Speed Heatmap:**

- 🔵 **Blue**: Low speed (<50 km/h)
- 🔴 **Red**: High speed (>100 km/h)

**G-Force Heatmap (Ax - Longitudinal):**

- 🟢 **Green**: Steady state
- 🟡 **Yellow**: Moderate acceleration/braking
- 🔴 **Red**: Hard acceleration/braking (>0.6g)

### Example Output

![Example Heatmap](docs/example_heatmap.png)
_Heatmap showing VTEC engagement during acceleration on Richmond Street_

### Features

- ⚡ **VTEC markers**: Orange circles show Honda VTEC engagement points
- 🏁 **Start/Finish flags**: Green flag (start) and red flag (end)
- 📊 **Statistics overlay**: Real-time telemetry summary
- 🗺️ **Interactive map**: Zoom, pan, click markers for details

---

## 📁 Project Structure

```
automotive-black-box/
├── Core/
│   ├── Src/
│   │   ├── main.c              # Main application logic
│   │   ├── can.c               # CAN bus driver
│   │   ├── gpio.c              # GPIO configuration
│   │   └── ...
│   └── Inc/
│       ├── main.h
│       ├── mpu6050.h           # Accelerometer driver
│       ├── ssd1306.h           # OLED display driver
│       └── ...
├── Drivers/                    # STM32 HAL drivers
├── Middlewares/
│   └── FatFs/                  # FAT filesystem for SD card
├── tools/
│   ├── map_gen.py              # Main visualization script
│   ├── data_sim.py             # Test data generator
│   └── requirements.txt        # Python dependencies
├── data/                       # CSV data files (gitignored)
├── docs/                       # Documentation and images
├── .gitignore
├── README.md
└── LICENSE
```

---

## 🔮 Future Enhancements

### Planned Features

- [ ] **OBD-II Standard PIDs**: Read throttle position, coolant temp, MAF
- [ ] **Real-time transmission**: Bluetooth/WiFi streaming to phone app
- [ ] **Machine learning**: Driving behavior analysis and scoring
- [ ] **3D visualization**: Altitude-aware path rendering
- [ ] **Video integration**: Sync dashcam footage with telemetry
- [ ] **Cloud storage**: Automatic upload to AWS/Firebase
- [ ] **Multi-vehicle support**: Configurable CAN IDs for different makes/models

### Hardware Improvements

- [ ] Custom PCB design (eliminate breadboard)
- [ ] Weatherproof enclosure
- [ ] Backup power (supercapacitor for safe shutdown)
- [ ] OBD-II power regulation (eliminate USB dependency)
- [ ] High-precision IMU (9-DOF with gyroscope)

### Software Optimizations

- [ ] DMA for CAN/UART (reduce CPU load)
- [ ] FreeRTOS integration (real-time task scheduling)
- [ ] Circular buffer logging (prevent SD card wear)
- [ ] Compression (reduce file size by 60%)
- [ ] Web dashboard (real-time browser-based monitoring)

---

## 🤝 Contributing

Contributions are welcome! This project is a learning resource for anyone interested in automotive embedded systems.

### How to Contribute

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

### Areas for Contribution

- **Firmware**: Optimize sampling rates, add new sensors
- **Visualization**: New chart types, 3D rendering, animated playback
- **Documentation**: Tutorials, wiring guides, troubleshooting
- **Testing**: Compatibility with other Honda models or manufacturers
- **Hardware**: Custom PCB designs, 3D-printed enclosures

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

### Libraries & Tools

- **STM32 HAL**: STMicroelectronics Hardware Abstraction Layer
- **FatFs**: ELM-ChaN's FAT filesystem module
- **Folium**: Python library for interactive maps
- **Branca**: Color mapping for Python visualizations

### Learning Resources

- [STM32 CAN Tutorial](https://controllerstech.com/can-protocol-in-stm32/) by ControllersTech
- [Understanding OBD-II](https://en.wikipedia.org/wiki/OBD-II_PIDs) - Wikipedia
- [MPU6050 Guide](https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/) by TDK InvenSense
- [CAN Bus Explained](https://www.csselectronics.com/pages/can-bus-simple-intro-tutorial) by CSS Electronics

### Inspiration

This project was inspired by my love for cars and to make something cool. 😎

---

## 📧 Contact

**Sunny Lin** - sunnylin893@gmail.com

Project Link: [https://github.com/s-l893/Automotive-Telemetry-Data-Acquisition-System-Black-Box-](https://github.com/s-l893/Automotive-Telemetry-Data-Acquisition-System-Black-Box-)

---

## ⚠️ Safety & Legal Disclaimer

**IMPORTANT**: This device interfaces with critical vehicle systems. Improper use may:

- Cause vehicle malfunction
- Void warranty
- Violate local regulations

**User Responsibilities:**

- ✅ Only use on vehicles you own or have permission to modify
- ✅ Follow all local laws regarding OBD-II access
- ✅ Never modify while vehicle is in motion
- ✅ Use fused connections to prevent electrical damage
- ✅ Test in safe, controlled environments

**This project is for educational purposes. The authors assume no liability for damages, injuries, or legal issues arising from its use.**

---

<div align="center">

Made with ❤️ and ☕ by automotive enthusiasts

**If this project helped you, please ⭐ star the repo!**

</div>
