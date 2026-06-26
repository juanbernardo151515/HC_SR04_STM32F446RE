/* ================================================================
 *  Contoh penggunaan hcsr04 library di main.c
 *  STM32F446RE Nucleo — KamariTech
 *
 *  Koneksi hardware:
 *    PA1 → TRIG pin HC-SR04
 *    PA0 → ECHO pin HC-SR04  (TIM2_CH1, 5V tolerant)
 *    5V  → VCC HC-SR04
 *    GND → GND HC-SR04
 * ================================================================ */

#include "main.h"
#include "hcsr04.h"

/* Handle timer dari CubeMX */
extern TIM_HandleTypeDef htim2;

/* ---- Buat instance sensor ---- */
HCSR04_HandleTypeDef hsonar;

/* ---- Variabel hasil ---- */
float jarak_cm = 0.0f;
HCSR04_StatusTypeDef status;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();

    /* ---- Isi konfigurasi sensor ---- */
    hsonar.trig_port = GPIOA;
    hsonar.trig_pin  = GPIO_PIN_1;
    hsonar.htim      = &htim2;

    /* ---- Inisialisasi ---- */
    HCSR04_Init(&hsonar);

    /* ================================================================
     *  PILIHAN A: Non-blocking (cocok untuk FreeRTOS / multi-task)
     *  Trigger dikirim manual, hasil dibaca terpisah
     * ================================================================ */
    while (1)
    {
        /* Kirim trigger */
        HCSR04_Trigger(&hsonar);

        /* Lakukan hal lain selama menunggu... */
        HAL_Delay(HCSR04_TRIGGER_INTERVAL_MS);

        /* Baca hasil */
        status = HCSR04_Read(&hsonar, &jarak_cm);

        if (status == HCSR04_OK) {
            /* Gunakan jarak_cm di sini */
        } else if (status == HCSR04_TIMEOUT) {
            /* Tidak ada objek terdeteksi */
        } else if (status == HCSR04_OUT_OF_RANGE) {
            /* Objek terlalu dekat atau terlalu jauh */
        }
    }

    /* ================================================================
     *  PILIHAN B: Blocking (cocok untuk main loop sederhana)
     *  Satu fungsi langsung dapat hasil
     * ================================================================ */
    while (1)
    {
        status = HCSR04_MeasureBlocking(&hsonar, &jarak_cm);

        if (status == HCSR04_OK) {
            /* Gunakan jarak_cm */
        }
    }

    /* ================================================================
     *  PILIHAN C: Dengan koreksi suhu (jika ada DHT11/BME280)
     * ================================================================ */
    float suhu_c = 33.0f;   /* baca dari sensor suhu */
    float distance_const = HCSR04_GetDistanceConstant(suhu_c);
    /* Konstanta ini bisa dipakai manual jika ingin override */
}

/* ================================================================
 *  WAJIB: Letakkan ini di stm32f4xx_it.c atau di bawah main()
 *  Menghubungkan HAL callback ke library kita
 * ================================================================ */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        HCSR04_CaptureCallback(&hsonar);
    }
}
