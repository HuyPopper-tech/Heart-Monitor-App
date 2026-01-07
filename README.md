# Heart Rate Monitor App

A full-stack ECG monitoring system: STM32 firmware acquires and processes ECG data, streams it over Bluetooth, and an Android app renders the waveform and BPM in real time.

## What’s Included

- **Embedded firmware (STM32F401RET6)** for sampling, filtering, and R-peak detection (Pan–Tompkins).
- **ECG front-end (AD8232)** for single‑lead signal acquisition.
- **Bluetooth streaming (HC‑05)** using UART SPP (CSV payload).
- **Android app (API 24+)** for waveform visualization and heart-rate display.

## System Architecture

```
AD8232 ECG  ->  STM32F401 (ADC + Pan–Tompkins)  ->  HC-05 (UART)  ->  Android App
```

## Repository Layout

```
Heart-Monitor-App/
├── Application/   # Android app (Kotlin)
└── Embedded/      # STM32 firmware (C)
```

## Data Format

Bluetooth payload is ASCII CSV:

```
ECG_VALUE,BPM\r\n
Example: 2048,75\r\n
```

## Build & Run

### Embedded Firmware (PlatformIO)

**Prerequisites**: PlatformIO (VS Code), STM32 toolchain, ST‑Link.

```powershell
# Build
pio run -e nucleo_f401re

# Upload
pio run -e nucleo_f401re -t upload

# Serial monitor (optional)
pio device monitor -b 115200
```

Key config file: `Embedded/platformio.ini` (includes CMSIS-DSP flags).

### Android App

**Prerequisites**: Android Studio 2023.x+, JDK 11+, Android SDK 34.

```powershell
cd Application

# Build APK
./gradlew.bat build

# Install to device/emulator
./gradlew.bat installDebug
```

## Configuration

### Sampling Rate
`Embedded/src/main.c`
```c
AD8232_Init(360);
```
Changing the rate requires re-tuning Pan–Tompkins filter delays and window size.

### Pan–Tompkins Parameters
`Embedded/include/pan_tompkins.h`
```c
#define SAMPLE_RATE_HZ      360.0f
#define INTEGRATION_WINDOW  54
```

### BPM Smoothing
`Embedded/src/pan_tompkins.c`
```c
ht->current_bpm = (int)(0.9f * ht->current_bpm + 0.1f * instant_bpm);
```

### App Chart Window
`Application/app/src/main/java/com/example/ecgmonitor/MainActivity.kt`
```kotlin
if (entries.size > 500) {
```

## Troubleshooting

**No ECG in app**: verify AD8232 leads, ADC pin PA0, and sampling ISR.

**BPM stuck at 0**: enable the simulator or check signal amplitude and thresholds.

**Bluetooth won’t connect**: confirm HC‑05 baud (115200) and pairing state.

**Chart isn’t updating**: validate the CSV format and line endings (`\r\n`).

## Notes

- **ECG Simulator**: toggle in `Embedded/src/main.c` with `USE_ECG_SIM` for development.
- **Bluetooth UUID**: `00001101-0000-1000-8000-00805F9B34FB` (SPP/RFCOMM).
- **Valid BPM range**: 40–200 bpm (adaptive thresholding + 200 ms refractory).

## License & Attribution

- CMSIS‑DSP (ARM)
- MPAndroidChart (PhilJay, Apache 2.0)
- Pan–Tompkins QRS algorithm (1985)

## Support

Open an issue with device logs, firmware settings, and your Android build details.
