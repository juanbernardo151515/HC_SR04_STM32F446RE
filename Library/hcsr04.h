/* ================================================================
 *  hcsr04.h
 *  Library HC-SR04 Ultrasonic Sensor untuk STM32 HAL
 *
 *  Metode  : Input Capture Timer (TIM2, CH1 rising + CH2 falling)
 *  Author  : KamariTech
 *  Target  : STM32F446RE (dapat diportasi ke STM32 lain)
 * ================================================================ */

#ifndef HCSR04_H
#define HCSR04_H

#include "stm32f4xx_hal.h"

/* ================================================================
 *  BAGIAN 1 — KONSTANTA KONFIGURASI
 *  Boleh diubah sesuai kebutuhan proyek
 * ================================================================ */

/* Kecepatan suara di udara pada 20°C (m/s)
 * Rumus koreksi suhu: v = 331.4 + (0.606 * suhu_celsius)
 * Contoh Yogyakarta ~33°C: ubah ke 351.4f */
#define HCSR04_SOUND_SPEED_MS           343.0f

/* Batas waktu tunggu sinyal ECHO (mikrodetik)
 * 30000 µs = 30 ms = jarak ~4 meter
 * Perkecil jika jarak maksimum proyek < 4 meter */
#define HCSR04_TIMEOUT_US               30000U

/* Jarak minimum yang dianggap valid (cm)
 * Di bawah 2 cm, sensor tidak reliable secara fisik */
#define HCSR04_MIN_DISTANCE_CM          2.0f

/* Jarak maksimum yang dianggap valid (cm)
 * Di atas 400 cm, di luar spesifikasi sensor */
#define HCSR04_MAX_DISTANCE_CM          400.0f

/* Interval minimum antar dua pengukuran (milidetik)
 * Minimum hardware: 60ms — default 100ms untuk keamanan */
#define HCSR04_TRIGGER_INTERVAL_MS      100U

/* Jumlah sampel untuk averaging (moving average)
 * 1  = averaging dimatikan, langsung pakai nilai mentah
 * >1 = rata-rata dari N sampel terakhir (kurangi noise) */
#define HCSR04_AVERAGE_SAMPLES          3U

/* ================================================================
 *  BAGIAN 2 — KONSTANTA TURUNAN
 *  Dihitung otomatis dari konstanta di atas — jangan diubah manual
 * ================================================================ */

/* Faktor konversi: T_echo (µs) → jarak (cm)
 *
 * Penurunan rumus:
 *   d (m)  = v_suara * T_echo (s) / 2
 *   d (m)  = v_suara * T_echo (µs) * 1e-6 / 2
 *   d (cm) = v_suara * T_echo (µs) * 1e-6 / 2 * 100
 *   d (cm) = T_echo (µs) * (v_suara / 2 / 10000)
 *
 * Contoh: T_echo = 1240 µs
 *   d = 1240 * (343 / 2 / 10000) = 1240 * 0.01715 = 21.27 cm
 */
#define HCSR04_DISTANCE_CONSTANT        (HCSR04_SOUND_SPEED_MS / 2.0f / 10000.0f)

/* ================================================================
 *  BAGIAN 3 — STATUS ENUM
 *  Nilai kembalian fungsi — user bisa cek kondisi sensor
 * ================================================================ */

typedef enum {
    HCSR04_OK            = 0,   /* Pengukuran berhasil, data valid       */
    HCSR04_TIMEOUT       = 1,   /* ECHO tidak datang dalam TIMEOUT_US    */
    HCSR04_OUT_OF_RANGE  = 2,   /* Jarak di luar MIN atau MAX            */
    HCSR04_BUSY          = 3,   /* Pengukuran sebelumnya belum selesai   */
    HCSR04_ERROR         = 4    /* Error umum (timer belum diinit, dll)  */
} HCSR04_StatusTypeDef;

/* ================================================================
 *  BAGIAN 4 — STRUCT HANDLE
 *
 *  Zona 1 (user isi saat init) :  trig_port, trig_pin, htim
 *  Zona 2 (library kelola)     :  ccr1, ccr2, avg_buffer, dst
 *  Zona 3 (user baca)          :  distance_cm, status
 * ================================================================ */

typedef struct {

    /* ---- Zona 1: User tentukan --------------------------------- */
    GPIO_TypeDef        *trig_port;     /* Port GPIO pin TRIG       ex: GPIOA        */
    uint16_t             trig_pin;      /* Nomor pin TRIG           ex: GPIO_PIN_1   */
    TIM_HandleTypeDef   *htim;          /* Pointer ke handle timer  ex: &htim2       */

    /* ---- Zona 2: Library kelola — jangan diakses langsung ------ */
    volatile uint32_t    ccr1;                              /* Timestamp rising edge  (µs) */
    volatile uint32_t    ccr2;                              /* Timestamp falling edge (µs) */
    volatile uint8_t     capture_state;                     /* 0=idle, 1=tunggu falling    */
    volatile uint8_t     data_ready;                        /* 1 jika data baru tersedia   */
    float                avg_buffer[HCSR04_AVERAGE_SAMPLES];/* Buffer averaging            */
    uint8_t              avg_index;                         /* Index buffer saat ini       */
    uint8_t              avg_count;                         /* Jumlah data valid di buffer */
    uint32_t             last_trigger_tick;                 /* Tick terakhir kali trigger  */

    /* ---- Zona 3: User baca ------------------------------------- */
    float                    distance_cm;   /* Hasil jarak dalam cm (diperbarui tiap ukur) */
    HCSR04_StatusTypeDef     status;        /* Status pengukuran terakhir                  */

} HCSR04_HandleTypeDef;

/* ================================================================
 *  BAGIAN 5 — DEKLARASI FUNGSI PUBLIC
 *  Fungsi yang dipanggil user dari main.c
 * ================================================================ */

/**
 * @brief  Inisialisasi sensor — wajib dipanggil sekali sebelum pakai
 * @param  hsonar  Pointer ke struct handle yang sudah diisi user
 * @retval HCSR04_OK jika berhasil, HCSR04_ERROR jika timer belum siap
 */
HCSR04_StatusTypeDef HCSR04_Init(HCSR04_HandleTypeDef *hsonar);

/**
 * @brief  Kirim pulsa TRIG 10µs — memulai satu siklus pengukuran
 * @note   Panggil setiap HCSR04_TRIGGER_INTERVAL_MS atau lebih
 * @param  hsonar  Pointer ke struct handle
 * @retval HCSR04_OK jika trigger berhasil dikirim
 *         HCSR04_BUSY jika pengukuran sebelumnya belum selesai
 */
HCSR04_StatusTypeDef HCSR04_Trigger(HCSR04_HandleTypeDef *hsonar);

/**
 * @brief  Baca hasil jarak dari struct
 * @note   Panggil setelah Trigger + tunggu minimal TRIGGER_INTERVAL_MS
 * @param  hsonar       Pointer ke struct handle
 * @param  distance_cm  Pointer ke variabel penyimpan hasil (output)
 * @retval HCSR04_OK            jika data valid
 *         HCSR04_TIMEOUT       jika tidak ada pantulan
 *         HCSR04_OUT_OF_RANGE  jika di luar batas MIN/MAX
 *         HCSR04_BUSY          jika data belum siap
 */
HCSR04_StatusTypeDef HCSR04_Read(HCSR04_HandleTypeDef *hsonar, float *distance_cm);

/**
 * @brief  Trigger + tunggu + baca dalam satu panggilan (blocking)
 * @note   Memblokir CPU selama ~TRIGGER_INTERVAL_MS
 *         Cocok untuk penggunaan simpel tanpa FreeRTOS
 * @param  hsonar       Pointer ke struct handle
 * @param  distance_cm  Pointer ke variabel penyimpan hasil (output)
 * @retval Status pengukuran
 */
HCSR04_StatusTypeDef HCSR04_MeasureBlocking(HCSR04_HandleTypeDef *hsonar, float *distance_cm);

/**
 * @brief  Callback Input Capture — dipanggil dari HAL_TIM_IC_CaptureCallback
 * @note   Letakkan di dalam HAL_TIM_IC_CaptureCallback() di main.c atau stm32f4xx_it.c
 *         Fungsi ini menangkap CCR1 (rising) dan CCR2 (falling),
 *         menghitung jarak, dan memperbarui distance_cm di struct
 * @param  hsonar  Pointer ke struct handle
 */
void HCSR04_CaptureCallback(HCSR04_HandleTypeDef *hsonar);

/**
 * @brief  Reset struct ke kondisi awal tanpa hapus konfigurasi pin
 * @note   Berguna jika sensor timeout dan perlu dicoba ulang
 * @param  hsonar  Pointer ke struct handle
 */
void HCSR04_Reset(HCSR04_HandleTypeDef *hsonar);

/**
 * @brief  Koreksi kecepatan suara berdasarkan suhu (opsional)
 * @note   Gunakan jika ada sensor suhu (DHT11, BME280, dll)
 *         Mengembalikan konstanta jarak yang dikoreksi suhu
 * @param  temperature_c  Suhu udara dalam derajat Celsius
 * @retval Konstanta jarak yang dikoreksi (pengganti HCSR04_DISTANCE_CONSTANT)
 */
float HCSR04_GetDistanceConstant(float temperature_c);

#endif /* HCSR04_H */
