# HC-SR04 Ultrasonic Sensor Library — STM32F446RE
**KamariTech** | Input Capture Timer Method | STM32 HAL

---

## Tentang Library Ini

Library HC-SR04 untuk STM32F446RE Nucleo menggunakan metode **Input Capture Timer (TIM2)** — bukan polling blocking seperti `pulseIn()` di Arduino. Pendekatan ini lebih akurat, tidak memblokir CPU, dan cocok untuk sistem FreeRTOS maupun multi-task.

---

## Fitur

- ✅ Input Capture TIM2 — resolusi **1 µs per tick**
- ✅ Instance-based design — support **multi-sensor**
- ✅ Moving average filter — kurangi noise pengukuran
- ✅ Status enum — `OK`, `TIMEOUT`, `OUT_OF_RANGE`, `BUSY`, `ERROR`
- ✅ Koreksi kecepatan suara berdasarkan **suhu real-time**
- ✅ Overflow-safe — handle wrap-around counter 32-bit
- ✅ Interval protection — cegah trigger terlalu cepat

---

## Hardware

| Pin STM32 | Fungsi | Keterangan |
|-----------|--------|------------|
| PA0 | TIM2_CH1 | ECHO input — **5V tolerant** |
| PA1 | GPIO_Output | TRIG output |
| 5V | VCC | Supply sensor |
| GND | GND | Ground |

> **Catatan:** PA0 dipilih karena 5V tolerant — ECHO HC-SR04 output 5V langsung aman tanpa voltage divider.

---

## Konfigurasi CubeMX

| Parameter | Nilai |
|-----------|-------|
| SYSCLK | 180 MHz (HSE + PLL) |
| APB1 Timer Clock | 90 MHz |
| TIM2 Prescaler (PSC) | **89** |
| TIM2 ARR | **0xFFFFFFFF** |
| TIM2 CH1 | Input Capture, Direct, Rising Edge |
| TIM2 CH2 | Input Capture, **Indirect**, Falling Edge |
| TIM2 NVIC Priority | 1 |
| USART2 | 115200 baud — debug output via ST-Link VCP |

---

## Cara Pakai

### 1. Salin file library
```
Core/Inc/hcsr04.h
Core/Src/hcsr04.c
```

### 2. Konfigurasi di main.c
```c
#include "hcsr04.h"

HCSR04_HandleTypeDef hsonar;
float jarak_cm = 0.0f;

// Inisialisasi
hsonar.trig_port = GPIOA;
hsonar.trig_pin  = GPIO_PIN_1;
hsonar.htim      = &htim2;
HCSR04_Init(&hsonar);

// Loop utama
while (1)
{
    HCSR04_Trigger(&hsonar);
    HAL_Delay(100);
    HCSR04_Read(&hsonar, &jarak_cm);

    char buf[50];
    sprintf(buf, "Jarak: %.2f cm\r\n", jarak_cm);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 100);
}
```

### 3. Pasang callback
```c
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        HCSR04_CaptureCallback(&hsonar);
    }
}
```

---

## Fungsi Library

| Fungsi | Keterangan |
|--------|------------|
| `HCSR04_Init()` | Inisialisasi sensor, validasi pin, start timer IC |
| `HCSR04_Trigger()` | Kirim pulsa TRIG 10µs |
| `HCSR04_Read()` | Baca hasil `distance_cm` dari struct |
| `HCSR04_MeasureBlocking()` | Trigger + tunggu + baca dalam satu panggilan |
| `HCSR04_CaptureCallback()` | Dipanggil dari HAL_TIM_IC_CaptureCallback |
| `HCSR04_Reset()` | Reset state tanpa hapus konfigurasi pin |
| `HCSR04_GetDistanceConstant()` | Koreksi kecepatan suara dari sensor suhu |

---

## Konstanta yang Bisa Dikustomisasi

```c
#define HCSR04_SOUND_SPEED_MS        343.0f   // kecepatan suara @20°C
#define HCSR04_TIMEOUT_US            30000U   // batas tunggu ECHO
#define HCSR04_MIN_DISTANCE_CM       2.0f     // jarak minimum valid
#define HCSR04_MAX_DISTANCE_CM       400.0f   // jarak maksimum valid
#define HCSR04_TRIGGER_INTERVAL_MS   100U     // interval antar pengukuran
#define HCSR04_AVERAGE_SAMPLES       3U       // jumlah sampel averaging
```

---

## Formula Jarak

```
Jarak (cm) = T_echo (µs) × (v_suara / 2 / 10000)
           = T_echo (µs) × 0.01715        // pada 20°C
```

Dengan koreksi suhu:
```
v_suara = 331.4 + (0.606 × T°C)  m/s
```

---

## Author

**KamariTech** — Embedded Systems Projects
GitHub: [@juanbernardo151515](https://github.com/juanbernardo151515)
