# TEG-GATEWAY: Central Gateway & Network Orchestrator for Wireless Structural Health Monitoring (WSN-SHM)

[![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%203B%20%2F%204B-red.svg?logo=raspberry-pi)](https://www.raspberrypi.com/)
[![Radio](https://img.shields.io/badge/Radio-LoRa%20SX1278%20%2F%20RFM95-blue.svg)](https://www.semtech.com/products/wireless-rf/lora-core/sx1278)
[![Language](https://img.shields.io/badge/C-POSIX%20%2F%20Pthreads%20%2F%20WiringPi-green.svg?logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Backend](https://img.shields.io/badge/Backend-Python%20%2F%20Flask%20(Blueprints)-lightgrey.svg?logo=flask)](https://flask.palletsprojects.com/)
[![Frontend](https://img.shields.io/badge/Frontend-React%2018%20%2F%20Vite%20%2F%20Tailwind-cyan.svg?logo=react)](https://react.dev/)
[![Database](https://img.shields.io/badge/Database-InfluxDB%20v2%20%2B%20SQLite%20(WAL)-blueviolet.svg?logo=influxdb)](https://www.influxdata.com/)
[![DSP](https://img.shields.io/badge/DSP-NumPy%20%2F%20SciPy-informational.svg?logo=scipy)](https://scipy.org/)
[![License](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

Central gateway and orchestrator server for an open, low-cost **Wireless Sensor Network for Structural Health Monitoring (WSN-SHM)**.

This system runs on a **Raspberry Pi 3 Model B / 4B** equipped with a Semtech SX1278 LoRa transceiver. It manages a distributed cluster of custom smart sensor nodes, provides sub-millisecond network clock synchronization, streams high-frequency inertial and environmental telemetry into **InfluxDB v2**, persists network state and measurement catalogues in **SQLite (WAL)**, and serves a modern reactive dashboard (**React + Vite + Tailwind CSS + uPlot**) with an advanced **Digital Signal Processing (DSP)** engine (FFT with Hann/ACF windowing and 2-State Kalman Filter 6-DoF sensor fusion for structural tilt estimation).

> 🔗 **Companion Repository (Sensor Nodes):**  
> Firmware and hardware design for the ESP32-S3 smart sensor nodes can be found at **[TEG-NODE](https://github.com/jorgedrp/TEG-NODE)**.

---

## Table of Contents

- [System Architecture](#system-architecture)
- [Key Features](#key-features)
- [Network Operating Modes](#network-operating-modes)
- [Digital Signal Processing (DSP) Engine](#digital-signal-processing-dsp-engine)
- [Hardware Setup & Pinout](#hardware-setup--pinout)
- [Repository Structure](#repository-structure)
- [Prerequisites & Dependencies](#prerequisites--dependencies)
- [Installation & Build](#installation--build)
  - [1. Enable Hardware SPI](#1-enable-hardware-spi)
  - [2. Install System Packages](#2-install-system-packages)
  - [3. Configure Environment Variables (`.env`)](#3-configure-environment-variables-env)
  - [4. Compile C Orchestration Binaries & React Frontend](#4-compile-c-orchestration-binaries--react-frontend)
  - [5. Python Virtual Environment & Tests](#5-python-virtual-environment--tests)
- [Production Deployment (Gunicorn & Systemd)](#production-deployment-gunicorn--systemd)
- [REST API Reference](#rest-api-reference)
- [Academic Context & Credits](#academic-context--credits)
- [License](#-license)

---

## System Architecture

```
                      +---------------------------------------+
                      |       Smart Sensor Nodes (ESP32-S3)   |
                      |  - MPU6050 (6-DoF Accelerometer/Gyro) |
                      |  - BME280 (Temp / Pressure / Humidity)|
                      |  - SX1278 (LoRa Transceiver)          |
                      +---------------------------------------+
                                          |
                                          |  LoRa 433 MHz
                                          |  Custom MAC & Sync Protocol
                                          v
+-----------------------------------------------------------------------------------+
|                        CENTRAL GATEWAY (Raspberry Pi 3B/4B)                       |
|                                                                                   |
|  [ SX1278 Radio ] <---(SPI / GPIO DIO0 Interrupt)---> [ libgpiod / WiringPi ]     |
|                                                                                   |
|  [ Multi-Threaded C Orchestrator (Environment-Aware via getenv) ]                 |
|    ├── Task Communicator   : Handles state machines, polling & LoRa packets       |
|    ├── Task Data Processor : Ring-buffer parsing & engineering unit conversion    |
|    ├── Task Influx Writer  : High-throughput batch streaming via libcurl          |
|    └── Clock Synchronizer  : 4-timestamp kernel-level skew/offset compensation   |
|                                                                                   |
|  [ InfluxDB v2 Time-Series DB ] (Bucket: lora_table)  |  [ SQLite (WAL) Storage ] |
|                                                                                   |
|  [ Modular Flask Backend (Blueprints + Service Layer) ]                           |
|    ├── Process Manager     : Mutex-protected C subprocess supervision             |
|    ├── Influx Service      : Parameterized Flux queries with error boundaries     |
|    ├── Spectral Engine     : Real FFT with Hann windowing & ACF scaling           |
|    ├── Tilt Estimator      : 2-State Kalman Filter 6-DoF sensor fusion            |
|    └── SSE Broadcaster     : Live console log pub/sub streaming                   |
|                                                                                   |
|  [ Modern Web Dashboard (React 18 + Vite + Tailwind CSS + uPlot) ]                |
+-----------------------------------------------------------------------------------+
```

---

## Key Features

- **Decoupled Production Backend (Flask + Blueprints + Service Layer):**
  - Modular API separation (`nodes`, `telemetry`, `orchestrator`, `registry`, `legacy`).
  - Thread-safe SQLite persistence for sensor configurations, state tracking, and measurement catalog.
  - Automatic migration from legacy `config.json` and `datalog.csv`.
- **Environment & Security First (`.env`):**
  - Zero hardcoded tokens. InfluxDB credentials and gateway paths are managed via `.env` in both Python and C (`getenv()`).
- **High-Performance Multi-Threaded C Drivers (POSIX & WiringPi):**
  - Decoupled real-time packet ingestion, hardware timestamping (`libgpiod`), and batch HTTP Line Protocol transfer.
- **Advanced Digital Signal Processing (NumPy / SciPy):**
  - **Spectral Modal Identification:** Real FFT with Hann windowing, Coherent Gain Amplitude Correction Factor (ACF), and automated resonant frequency ($f_0$) peak detection.
  - **Dynamic Inclinometry:** 2-State Kalman Filter estimating continuous structural Roll & Pitch angles without gyro drift.
- **Modern Responsive Dashboard (React + Vite + Tailwind CSS):**
  - Live console monitor with Server-Sent Events (SSE) and ANSI syntax highlighting.
  - Interactive multi-axis time-series, FFT spectra, inclination, and environmental graphs with full-viewport in-app modal expansion.

---

## Network Operating Modes

| Mode | Binary | Description |
| :--- | :--- | :--- |
| **Standby Mode** | `gateway/standby_mode` | Puts nodes in low-power listening state, verifying link vitality. |
| **Event-Triggered Mode** | `gateway/event_mode` | Nodes monitor structural vibrations locally and stream inertial bursts when acceleration thresholds are exceeded. |
| **Time-Triggered Mode** | `gateway/time_mode` | Coordinates scheduled synchronous sampling windows across all active nodes. |
| **Configuration Mode** | `gateway/config_mode` | Dispatches over-the-air (OTA) configuration packets (sampling rate, trigger sensitivity, measurement duration). |
| **Detection Mode** | `gateway/detect_mode` | Probes network addresses to discover active sensor nodes and query their state. |
| **Clock Sync Mode** | `gateway/clock` | Executes bidirectional 4-timestamp exchange for precision time synchronization. |

---

## Digital Signal Processing (DSP) Engine

### 1. Spectral Analysis (FFT & Modal Identification)
1. **Mean Subtraction:** Removes DC gravity component.
2. **Hann Windowing:** Applied to minimize spectral leakage:
   $$w[n] = 0.5 \left(1 - \cos\left(\frac{2\pi n}{N-1}\right)\right)$$
3. **Real FFT & Amplitude Correction Factor (ACF):**
   $$\text{ACF} = \frac{N}{\sum w[n]}, \quad \text{Magnitude} = \frac{2.0}{N} \cdot \text{ACF} \cdot |Y_f|$$

### 2. Tilt & Inclination Estimation (2-State Kalman Filter)
- **State Vector:** Angle $\theta$ and Gyroscope Bias $b$:
$$
  \mathbf{x}_k = \begin{bmatrix} \theta_k \\ b_k \end{bmatrix}
$$
- **Prediction Phase:**
$$
  \hat{\theta}_{k|k-1} = \hat{\theta}_{k-1|k-1} + (\omega_k - \hat{b}_{k-1|k-1}) \Delta t
$$
- **Measurement Update:**
$$
  \text{Roll} = \text{atan2}(A_y, A_z), \quad \text{Pitch} = \text{atan2}(-A_x, \sqrt{A_y^2 + A_z^2})
$$

---

## Hardware Setup & Pinout

| Raspberry Pi Pin | BCM GPIO | SX1278 / RFM95 Pin | Description |
| :--- | :--- | :--- | :--- |
| **Pin 1 / 17** | `3.3V` | `VCC / 3.3V` | 3.3V Power Supply (Do NOT connect to 5V) |
| **Pin 6 / 9 / 14 / 20 / 25** | `GND` | `GND` | Ground |
| **Pin 19** | `GPIO 10` (MOSI) | `MOSI` | SPI Master Output, Slave Input |
| **Pin 21** | `GPIO 9` (MISO) | `MISO` | SPI Master Input, Slave Output |
| **Pin 23** | `GPIO 11` (SCLK) | `SCK` | SPI Serial Clock |
| **Pin 24** | `GPIO 8` (CE0) | `NSS / CS` | SPI Chip Select |
| **Pin 18** | `GPIO 24` | `DIO0` | Packet RX/TX Interrupt (libgpiod rising edge) |
| **Pin 22** | `GPIO 25` | `RST` | Hardware Reset |

---

## Repository Structure

```
TEG-GATEWAY/
├── .env.example                # Environment configuration template
├── .env                        # Active environment variables (git-ignored)
├── Makefile                    # Root orchestration Makefile
├── requirements.txt            # Python dependencies
├── wsgi.py                     # WSGI production entrypoint
├── gunicorn_config.py          # Production Gunicorn settings
├── systemd/
│   └── teg-gateway.service     # Linux systemd auto-start service
├── gateway/                    # C LoRa & SPI Subsystem
│   ├── Makefile                # C compilation Makefile
│   ├── lora.c / lora.h         # SX1278 driver
│   ├── spi.c / spi.h           # Linux SPIDEV driver
│   ├── curl.c                  # InfluxDB batch writer (reads getenv)
│   ├── clock.c                 # Precision clock synchronization
│   ├── config_mode.c           # OTA configuration routine
│   ├── detect_mode.c           # Network discovery routine
│   ├── event_mode.c            # Event-triggered monitoring routine
│   ├── standby_mode.c          # Low-power standby routine
│   └── time_mode.c             # Synchronous time-triggered routine
├── webapp/
│   ├── config.py               # Centralized configuration & environment loader
│   ├── app/
│   │   ├── __init__.py         # Flask Application Factory (create_app)
│   │   ├── database/           # SQLite (WAL) connection, models & migrations
│   │   ├── services/           # InfluxService, ProcessManager, DSPEngine, SSEBroadcaster
│   │   ├── blueprints/         # Nodes, Telemetry, Orchestrator, Registry, Views, Legacy
│   │   └── static/dist/        # Compiled React SPA distribution
│   └── frontend/               # React 18 + Vite + Tailwind CSS Source Code
└── tests/                      # Automated pytest test suite
```

---

## Installation & Build

### 1. Enable Hardware SPI
```bash
sudo raspi-config
# Navigate to: Interface Options -> SPI -> Enable -> Yes -> Finish
```

### 2. Install System Packages
```bash
sudo apt update
sudo apt install -y build-essential git python3 python3-pip python3-venv \
                    libcurl4-openssl-dev gpiod libgpiod-dev nodejs npm
```

### 3. Configure Environment Variables (`.env`)
Copy the example file and update your InfluxDB token:
```bash
cp .env.example .env
nano .env
```

### 4. Compile C Orchestration Binaries & React Frontend
Using the unified Makefile:
```bash
# Compiles C binaries in gateway/
make build-gateway

# Installs npm dependencies and builds React frontend into static/dist/
make build-frontend
```

### 5. Python Virtual Environment & Tests
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Run automated test suite
make test
```

---

## Production Deployment (Gunicorn & Systemd)

To run the application as an automated background service on Raspberry Pi:

1. **Test with Gunicorn:**
```bash
make run-prod
```

2. **Install Systemd Service:**
```bash
sudo cp systemd/teg-gateway.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now teg-gateway.service
```

3. **Check Service Status:**
```bash
sudo systemctl status teg-gateway.service
```

---

## REST API Reference

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/nodes` | List all configured nodes and their states. |
| `PUT` | `/api/nodes/<id>` | Update OTA parameters for a specific node. |
| `GET` | `/api/nodes/states` | Get real-time status mapping (`STANDBY`, `EVENTO`, `OFF`, etc.). |
| `POST`| `/api/orchestrator/command/<cmd>` | Dispatch a LoRa network command (`standby`, `eventos`, `tiempo`, `sync`, etc.). |
| `POST`| `/api/orchestrator/stop` | Stop any running C binary cleanly. |
| `GET` | `/api/orchestrator/status` | Get process status, PID, and uptime. |
| `GET` | `/api/orchestrator/stream` | Server-Sent Events (SSE) live log feed. |
| `GET` | `/api/telemetry/data` | Query inertial series, BME280 environmental data, and computed FFT. |
| `GET` | `/api/telemetry/inclinacion` | Query 6-DoF telemetry processed with 2-State Kalman Filter (Roll/Pitch). |
| `GET` | `/api/registry` | Paginated list of measurement sessions. |
| `GET` | `/api/registry/export` | Download measurement catalog as CSV. |

---

## Academic Context & Credits

This project was developed as part of an **Undergraduate Degree Thesis (Trabajo Especial de Grado - TEG)** at **Universidad Central de Venezuela (UCV)**, Faculty of Engineering.

- **Author:** Jorge D. Ramírez. P. ([@jorgedrp](https://github.com/jorgedrp))
- **Node Firmware:** [TEG-NODE Repository](https://github.com/jorgedrp/TEG-NODE)

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
