# Heart Rate Monitor App

A comprehensive real-time ECG monitoring system combining embedded firmware for signal acquisition and processing with an Android mobile application for data visualization and heart rate monitoring.

## Project Overview

This project implements a complete heart rate monitoring solution using:
- **Embedded System**: STM32F401RET6 microcontroller with ECG signal acquisition and processing
- **Hardware Sensors**: AD8232 ECG front-end amplifier for biomedical signal capture
- **Wireless Communication**: HC-05 Bluetooth module for wireless data transmission
- **Mobile Application**: Android app (API 24+) for real-time ECG waveform visualization and BPM display

## System Architecture

```
┌─────────────────┐          ┌──────────────────┐          ┌──────────────────┐
│                 │          │                  │          │                  │
│    AD8232       │──ADC──→  │  STM32F401RET6   │─USART─→  │   HC-05          │
│  (ECG Module)   │          │ (Microcontroller)│   TX/RX  │ (Bluetooth)      │
│                 │          │                  │          │                  │
└─────────────────┘          └──────────────────┘          └──────────────────┘
                                     ↓
                            Pan-Tompkins Algorithm
                            (R-peak Detection)
                                     ↓
                              ECG Data + BPM
                                (via HC-05)
                                     ↓
                             ┌──────────────────┐
                             │                  │
                             │   Android App    │
                             │ (Bluetooth RFCOMM)
                             │                  │
                             └──────────────────┘
                                  Displays:
                              • Real-time ECG
                              • Heart Rate (BPM)
                              • Connection Status
```

---

## Hardware Components

### 1. **STM32F401RET6 Microcontroller**
- **Architecture**: ARM Cortex-M4 @ 84 MHz with FPU
- **Memory**: 512 KB Flash, 96 KB SRAM
- **Interfaces**: 
  - ADC1 (12-bit) on PA0 for ECG signal
  - USART1 (PA9-TX, PA10-RX) for HC-05 communication
  - TIM3 for sampling rate control

### 2. **AD8232 ECG Front-End Module**
- **Function**: Low-power, single-lead ECG amplifier
- **Signal Processing**:
  - Input impedance: ~1 GΩ
  - Gain: 100 V/V
  - Bandwidth: 0.5 Hz – 40 Hz
- **Outputs**:
  - PA0: Analog ECG signal (ADC input)
  - PA1, PA4: Leads Off detection (GPIO input)
- **Sampling Rate**: 360 Hz (fixed, configured by TIM3)

### 3. **HC-05 Bluetooth Module**
- **Protocol**: Bluetooth Classic (SPP - Serial Port Profile)
- **Baud Rate**: 115200 bps (hardware level)
- **Connection**: USART1 on STM32F401RET6
- **Data Format**: CSV format (ECG_value,BPM\r\n)

---

## Software Components

### Embedded Firmware (STM32CubeMX + ARM CMSIS-DSP)

#### **Main Processing Pipeline**

**File**: `Embedded/src/main.c`

```c
// 1. Initialization Phase
AD8232_Init(360);              // ECG ADC at 360 Hz
HC05_Init();                   // Bluetooth UART
PT_Init(&pt_handle);           // Pan-Tompkins algorithm

// 2. Main Loop
while(1) {
    if(ad8232_sample_ready) {  // Sample at configured rate
        ecg_val = AD8232_ReadValue();
        PT_Process(&pt_handle, ecg_val);
        bpm = PT_GetBPM(&pt_handle);
        HC05_SendString("ECG,BPM\r\n");
    }
}
```

#### **Core Modules**

1. **AD8232 Driver** (`Embedded/src/ad8232.c`)
   - ADC1 configuration (PA0 analog input)
   - TIM3 interrupt for sample-rate control
   - Leads-off detection (PA1, PA4)
   - **Sampling Rate**: Configurable via TIM3 ARR

2. **HC-05 Bluetooth Interface** (`Embedded/src/hc05.c`)
   - USART1 initialization (115200 baud)
   - Character and string transmission
   - ASCII data format for mobile parsing

3. **Pan-Tompkins Algorithm** (`Embedded/src/pan_tompkins.c`)
   - **Stage 1**: Bandpass filter (5–15 Hz equivalent)
     - Implemented using difference equations (LPF + HPF)
     - Parameters scaled for Fs = 360 Hz
   - **Stage 2**: Derivative filter (5-point)
     - Emphasizes rapid signal changes
   - **Stage 3**: Squaring
     - Non-linear enhancement of QRS peaks
   - **Stage 4**: Moving window integration (54 samples ≈ 150 ms @ 360 Hz)
     - Temporal smoothing
   - **Stage 5**: Adaptive threshold R-peak detection
     - Refractory period: 200 ms (prevents T-wave false positives)
     - Valid BPM range: 40-200 bpm
     - Adaptive threshold adjustment based on signal/noise levels

4. **ECG Simulator** (`Embedded/src/ecg_sim.c`)
   - Generates realistic ECG waveforms for testing
   -  Signal timing scaled for Fs = 360 Hz
   - Toggle via `#define USE_ECG_SIM 1` in main.c
   - Useful for development without patient connection

#### **ARM CMSIS-DSP Library**
- **Location**: `Embedded/lib/DSP/`
- **Build Flag**: `-DARM_MATH_CM4` enables Cortex-M4 optimizations

### Android Application (Kotlin + Material Design)

**File**: `Application/app/src/main/java/com/example/ecgmonitor/MainActivity.kt`

#### **Key Features**

1. **Bluetooth Connection Management**
   - Device pairing and connection
   - UUID: `00001101-0000-1000-8000-00805F9B34FB` (RFCOMM)
   - Automatic reconnection on failure
   - Runtime permissions (Android 6+, 12+)

2. **Real-Time Data Visualization**
   - MPAndroidChart library for ECG waveform display
   - Scrolling graph with last 500 samples
   - Cubic Bezier curve interpolation
   - Pink color scheme (#FF4081) with transparency fill

3. **Heart Rate Display**
   - Live BPM reading update
   - Connection status indicator
   - Connect/Disconnect toggle button

4. **Screen Rotation Handling**
   - Preserves Bluetooth connection during rotation
   - Maintains chart data and BPM value
   - Layout reloads without reconnection


#### **Data Reception Format**
```
ECG_VALUE,BPM\r\n
Example: 2048,75\r\n
```

---

## Build & Deployment

### Embedded Firmware

**Prerequisites**:
- PlatformIO IDE or VSCode + PlatformIO extension
- STM32CubeMX (for register-level code)
- ARM GNU Toolchain

**Build Configuration** (`Embedded/platformio.ini`):
```ini
[env:nucleo_f401re]
platform = ststm32
board = nucleo_f401re
framework = stm32cube
upload_protocol = stlink
monitor_speed = 115200
build_flags =
    -DARM_MATH_CM4
    -D__FPU_PRESENT=1
    -O3
    -ffast-math
    -Ilib/DSP/Include
```

**Compilation & Upload**:
```powershell
# Build firmware
pio run -e nucleo_f401re

# Upload to device
pio run -e nucleo_f401re -t upload

# Monitor serial output
pio device monitor -b 115200
```

### Android Application

**Prerequisites**:
- Android Studio 2023.x or higher
- JDK 11+
- Android SDK API 34

**Build Configuration** (`Application/app/build.gradle`):
- Minimum API: 24 (Android 7.0)
- Target API: 34 (Android 14)
- Java: 1.8 compatible
- Kotlin: 1.8

**Compilation & Installation**:
```powershell
# Navigate to Application directory
cd Application

# Build APK
./gradlew.bat build

# Install to device/emulator
./gradlew.bat installDebug

# Run app
./gradlew.bat connectedAndroidTest
```

---

## Configuration & Customization

### Sampling Rate
Modify in `Embedded/src/main.c`:
```c
AD8232_Init(360);
```
**Note**: Changing the sampling rate requires redesign of all Pan–Tompkins filter delays and window sizes.

### Pan-Tompkins Sensitivity
Modify in `Embedded/include/pan_tompkins.h`:
```c
#define SAMPLE_RATE_HZ      360.0f
#define INTEGRATION_WINDOW  54   // 150 ms window @ 360 Hz
```

### BPM Smoothing
Modify in `Embedded/src/pan_tompkins.c`:
```c
ht->current_bpm = (int)(0.9f * ht->current_bpm + 0.1f * instant_bpm);
//                        ^^^                     ^^^
//                    Filter coefficient (increase for more smoothing)
```

### ECG Display Range
Modify in `Application/app/src/main/java/com/example/ecgmonitor/MainActivity.kt`:
```kotlin
// Limit entries to prevent memory overflow
if (entries.size > 500) {  // Change 500 to desired point count
```

---

## Data Flow

### Signal Path: Patient → Display

```
Patient (Leads) 
    ↓
AD8232 (Amplification & Filtering)
    ↓
STM32F401 ADC (Sampling at configured rate)
    ↓
Pan-Tompkins Algorithm
    ├─ Bandpass Filter (5-15 Hz)
    ├─ Derivative Filter
    ├─ Squaring & Integration
    └─ Peak Detection → BPM Calculation
    ↓
HC-05 Bluetooth Module (UART transmission)
    ↓
Android Phone (Bluetooth reception)
    ├─ Data Parsing (ECG value, BPM)
    ├─ Chart Update
    └─ BPM Display Update
    ↓
User Visualization
```

---

## Troubleshooting

### Firmware Issues

| Problem | Solution |
|---------|----------|
| No ECG signal in app | Check AD8232 leads connection; Verify ADC PA0 configuration |
| BPM reading 0 | Enable ECG simulator or check signal amplitude; verify Pan-Tompkins algorithm receives data |
| Bluetooth no connection | Verify HC-05 baud rate matches (115200); Check USART1 PA9/PA10 configuration |
| Signal noise/instability | Increase `INTEGRATION_WINDOW`; Lower sampling rate; Add capacitive filtering on AD8232 input |

### Android App Issues

| Problem | Solution |
|---------|----------|
| Bluetooth permission denied | Grant BLUETOOTH_SCAN & BLUETOOTH_CONNECT on Android 12+ |
| No device found | Ensure HC-05 is paired in system Bluetooth settings; Turn on device |
| Chart doesn't update | Check data format (CSV with \r\n); Verify connection status |
| App crashes on rotation | Ensure `MainActivity` has `android:configChanges="orientation\|screenSize"` in manifest |

---

## Project Structure

```
Heart-Monitor-App/
├── Application/                    # Android App (Kotlin)
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/example/ecgmonitor/
│   │   │   │   └── MainActivity.kt
│   │   │   ├── res/                # UI resources
│   │   │   └── AndroidManifest.xml
│   │   └── build.gradle
│   ├── settings.gradle
│   └── build.gradle
│
├── Embedded/                       # STM32 Firmware (C)
│   ├── platformio.ini              # PlatformIO configuration
│   ├── src/
│   │   ├── main.c                  # Main application loop
│   │   ├── ad8232.c                # ECG module driver
│   │   ├── hc05.c                  # Bluetooth UART driver
│   │   ├── pan_tompkins.c          # R-peak detection algorithm
│   │   ├── ecg_sim.c               # ECG waveform simulator
│   │   └── [system files]
│   ├── include/
│   │   ├── main.h
│   │   ├── ad8232.h
│   │   ├── hc05.h
│   │   ├── pan_tompkins.h
│   │   ├── ecg_sim.h
│   │   └── [HAL & system headers]
│   ├── lib/
│   │   └── DSP/                    # ARM CMSIS-DSP library
│   │       ├── Include/
│   │       └── Source/
│   └── test/
│
└── README.md                       # This file
```

---

## Performance Specifications

| Metric | Value |
|--------|-------|
| **ECG Sampling Rate** | 360 Hz |
| **ADC Resolution** | 12-bit (0-4095) |
| **Bandpass Filter** | 5–15 Hz equivalent (difference-equation Pan–Tompkins) |
| **R-Peak Detection Latency** | ~150 ms (algorithm + transmission) |
| **Valid BPM Range** | 40-200 bpm |
| **Refractory Period** | 200 ms (prevents false detections) |
| **Bluetooth Range** | ~10-30 meters (line-of-sight) |
| **Android Min SDK** | API 24 (Android 7.0) |
| **MCU Clock** | 84 MHz |
| **Power Consumption (typical)** | ~100 mA (MCU + AD8232 + HC-05) |

---

## Development Notes

### Testing with ECG Simulator
For development without patient connection, enable simulator in `Embedded/src/main.c`:
```c
#define USE_ECG_SIM 1   // Enable simulator
#define USE_ECG_SIM 0   // Disable simulator (use real AD8232)
```

### Real-Time Constraints
- **ADC Sampling**: Interrupt-driven (TIM3)
- **Signal Processing**: Executed in main loop (non-blocking)
- **Data Transmission**: Asynchronous via UART

### Future Enhancements
- Multi-lead ECG (requires additional AD8232 modules)
- Data logging to SD card
- Arrhythmia detection algorithms
- Mobile app offline data analysis
- Cloud synchronization with healthcare platforms

---

## License & Attribution

- **CMSIS-DSP**: ARM Embedded Microcontroller Software Interface - Standard
- **MPAndroidChart**: PhilJay (Apache 2.0)
- **Embedded Framework**: STM32CubeMX / STM32Cube HAL
- **Pan-Tompkins Algorithm**: Based on "A Real-Time QRS Detection Algorithm" by Pan & Tompkins (1985)

---

## Support & Contribution

For issues, questions, or contributions:
1. Check troubleshooting section above
2. Review hardware connections and configurations
3. Enable serial/Bluetooth debugging
4. Submit detailed logs and error messages
