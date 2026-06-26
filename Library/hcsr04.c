/* ================================================================
 *  hcsr04.c
 *  Implementasi Library HC-SR04 Ultrasonic Sensor
 *
 *  Author  : KamariTech
 *  Target  : STM32F446RE HAL
 * ================================================================ */

#include "hcsr04.h"

/* ================================================================
 *  FUNGSI PRIVATE (hanya dipakai di dalam file ini)
 * ================================================================ */

/**
 * @brief  Hitung rata-rata dari buffer averaging
 */
static float _HCSR04_CalcAverage(HCSR04_HandleTypeDef *hsonar)
{
    float sum = 0.0f;
    uint8_t count = (hsonar->avg_count < HCSR04_AVERAGE_SAMPLES)
                     ? hsonar->avg_count
                     : HCSR04_AVERAGE_SAMPLES;

    for (uint8_t i = 0; i < count; i++) {
        sum += hsonar->avg_buffer[i];
    }
    return (count > 0) ? (sum / count) : 0.0f;
}

/**
 * @brief  Masukkan nilai baru ke buffer averaging (circular)
 */
static void _HCSR04_PushAverage(HCSR04_HandleTypeDef *hsonar, float value)
{
    hsonar->avg_buffer[hsonar->avg_index] = value;
    hsonar->avg_index = (hsonar->avg_index + 1) % HCSR04_AVERAGE_SAMPLES;
    if (hsonar->avg_count < HCSR04_AVERAGE_SAMPLES) {
        hsonar->avg_count++;
    }
}

/* ================================================================
 *  FUNGSI PUBLIC — IMPLEMENTASI
 * ================================================================ */

/* ----------------------------------------------------------------
 *  HCSR04_Init
 * ---------------------------------------------------------------- */
HCSR04_StatusTypeDef HCSR04_Init(HCSR04_HandleTypeDef *hsonar)
{
    /* Validasi pointer — pastikan user sudah isi semua field wajib */
    if (hsonar == NULL)             return HCSR04_ERROR;
    if (hsonar->trig_port == NULL)  return HCSR04_ERROR;
    if (hsonar->htim == NULL)       return HCSR04_ERROR;

    /* Reset semua field internal ke kondisi awal */
    hsonar->ccr1              = 0;
    hsonar->ccr2              = 0;
    hsonar->capture_state     = 0;
    hsonar->data_ready        = 0;
    hsonar->avg_index         = 0;
    hsonar->avg_count         = 0;
    hsonar->last_trigger_tick = 0;
    hsonar->distance_cm       = 0.0f;
    hsonar->status            = HCSR04_OK;

    /* Bersihkan buffer averaging */
    for (uint8_t i = 0; i < HCSR04_AVERAGE_SAMPLES; i++) {
        hsonar->avg_buffer[i] = 0.0f;
    }

    /* Pastikan TRIG pin dalam kondisi LOW sebelum mulai */
    HAL_GPIO_WritePin(hsonar->trig_port, hsonar->trig_pin, GPIO_PIN_RESET);

    /* Mulai timer Input Capture di CH1 (rising edge) */
    if (HAL_TIM_IC_Start_IT(hsonar->htim, TIM_CHANNEL_1) != HAL_OK) {
        return HCSR04_ERROR;
    }

    /* Mulai timer Input Capture di CH2 (falling edge, indirect) */
    if (HAL_TIM_IC_Start_IT(hsonar->htim, TIM_CHANNEL_2) != HAL_OK) {
        return HCSR04_ERROR;
    }

    return HCSR04_OK;
}

/* ----------------------------------------------------------------
 *  HCSR04_Trigger
 * ---------------------------------------------------------------- */
HCSR04_StatusTypeDef HCSR04_Trigger(HCSR04_HandleTypeDef *hsonar)
{
    if (hsonar == NULL) return HCSR04_ERROR;

    /* Cek apakah pengukuran sebelumnya masih berlangsung */
    if (hsonar->capture_state != 0) {
        return HCSR04_BUSY;
    }

    /* Cek interval minimum antar trigger */
    uint32_t now = HAL_GetTick();
    if ((now - hsonar->last_trigger_tick) < HCSR04_TRIGGER_INTERVAL_MS) {
        return HCSR04_BUSY;
    }

    /* Reset flag sebelum trigger baru */
    hsonar->data_ready    = 0;
    hsonar->ccr1          = 0;
    hsonar->ccr2          = 0;
    hsonar->capture_state = 0;

    /* Kirim pulsa TRIG:
     * HIGH selama 10µs, lalu LOW
     * DWT cycle counter digunakan untuk delay presisi µs */
    hsonar->last_trigger_tick = now;

    HAL_GPIO_WritePin(hsonar->trig_port, hsonar->trig_pin, GPIO_PIN_SET);

    /* Delay 10µs menggunakan timer HAL
     * Catatan: HAL_Delay resolusinya 1ms — untuk 10µs
     * kita gunakan loop DWT jika tersedia, atau polling timer */
    uint32_t t_start = hsonar->htim->Instance->CNT;
    while ((hsonar->htim->Instance->CNT - t_start) < 10U);

    HAL_GPIO_WritePin(hsonar->trig_port, hsonar->trig_pin, GPIO_PIN_RESET);

    return HCSR04_OK;
}

/* ----------------------------------------------------------------
 *  HCSR04_Read
 * ---------------------------------------------------------------- */
HCSR04_StatusTypeDef HCSR04_Read(HCSR04_HandleTypeDef *hsonar, float *distance_cm)
{
    if (hsonar == NULL || distance_cm == NULL) return HCSR04_ERROR;

    /* Data belum siap — pengukuran masih berlangsung */
    if (hsonar->data_ready == 0) {
        return HCSR04_BUSY;
    }

    /* Salin hasil ke output parameter */
    *distance_cm = hsonar->distance_cm;

    /* Reset flag agar siap menerima data baru */
    hsonar->data_ready = 0;

    return hsonar->status;
}

/* ----------------------------------------------------------------
 *  HCSR04_MeasureBlocking
 *  Trigger + tunggu + baca dalam satu panggilan
 *  Cocok untuk main loop sederhana tanpa FreeRTOS
 * ---------------------------------------------------------------- */
HCSR04_StatusTypeDef HCSR04_MeasureBlocking(HCSR04_HandleTypeDef *hsonar, float *distance_cm)
{
    if (hsonar == NULL || distance_cm == NULL) return HCSR04_ERROR;

    /* Kirim trigger */
    HCSR04_StatusTypeDef ret = HCSR04_Trigger(hsonar);
    if (ret != HCSR04_OK) return ret;

    /* Tunggu data siap atau timeout */
    uint32_t t_start = HAL_GetTick();
    while (hsonar->data_ready == 0) {
        if ((HAL_GetTick() - t_start) > (HCSR04_TIMEOUT_US / 1000U + 10U)) {
            hsonar->status        = HCSR04_TIMEOUT;
            hsonar->capture_state = 0;
            *distance_cm          = 0.0f;
            return HCSR04_TIMEOUT;
        }
    }

    return HCSR04_Read(hsonar, distance_cm);
}

/* ----------------------------------------------------------------
 *  HCSR04_CaptureCallback
 *
 *  Dipanggil dari HAL_TIM_IC_CaptureCallback() di stm32f4xx_it.c
 *
 *  Cara pasang di stm32f4xx_it.c atau main.c:
 *
 *  void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
 *  {
 *      HCSR04_CaptureCallback(&hsonar);
 *  }
 * ---------------------------------------------------------------- */
void HCSR04_CaptureCallback(HCSR04_HandleTypeDef *hsonar)
{
    if (hsonar == NULL) return;

    /* Pastikan interrupt dari timer yang benar */
    if (hsonar->htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        /* ---- Rising edge terdeteksi ----
         * ECHO naik → catat waktu mulai */
        hsonar->ccr1          = HAL_TIM_ReadCapturedValue(hsonar->htim, TIM_CHANNEL_1);
        hsonar->capture_state = 1;  /* tandai: sedang menunggu falling edge */
    }
    else if (hsonar->htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        /* ---- Falling edge terdeteksi ----
         * ECHO turun → catat waktu selesai */
        if (hsonar->capture_state == 1)
        {
            hsonar->ccr2 = HAL_TIM_ReadCapturedValue(hsonar->htim, TIM_CHANNEL_2);

            /* Hitung lebar pulsa ECHO dalam µs
             * Handle overflow counter 32-bit TIM2 */
            uint32_t T_echo;
            if (hsonar->ccr2 >= hsonar->ccr1) {
                T_echo = hsonar->ccr2 - hsonar->ccr1;
            } else {
                /* Overflow: counter melewati 0xFFFFFFFF lalu wrap */
                T_echo = (0xFFFFFFFFU - hsonar->ccr1) + hsonar->ccr2 + 1U;
            }

            /* Cek timeout — tidak ada pantulan */
            if (T_echo > HCSR04_TIMEOUT_US) {
                hsonar->status     = HCSR04_TIMEOUT;
                hsonar->distance_cm = 0.0f;
            }
            else
            {
                /* Hitung jarak mentah */
                float raw_distance = (float)T_echo * HCSR04_DISTANCE_CONSTANT;

                /* Validasi batas range sensor */
                if (raw_distance < HCSR04_MIN_DISTANCE_CM ||
                    raw_distance > HCSR04_MAX_DISTANCE_CM)
                {
                    hsonar->status = HCSR04_OUT_OF_RANGE;
                }
                else
                {
                    /* Masukkan ke buffer averaging */
                    _HCSR04_PushAverage(hsonar, raw_distance);

                    /* Update distance_cm dengan hasil rata-rata */
                    hsonar->distance_cm = _HCSR04_CalcAverage(hsonar);
                    hsonar->status      = HCSR04_OK;
                }
            }

            /* Pengukuran selesai */
            hsonar->capture_state = 0;
            hsonar->data_ready    = 1;
        }
    }
}

/* ----------------------------------------------------------------
 *  HCSR04_Reset
 * ---------------------------------------------------------------- */
void HCSR04_Reset(HCSR04_HandleTypeDef *hsonar)
{
    if (hsonar == NULL) return;

    /* Reset semua state internal tanpa hapus konfigurasi pin */
    hsonar->ccr1              = 0;
    hsonar->ccr2              = 0;
    hsonar->capture_state     = 0;
    hsonar->data_ready        = 0;
    hsonar->distance_cm       = 0.0f;
    hsonar->status            = HCSR04_OK;
    hsonar->avg_index         = 0;
    hsonar->avg_count         = 0;
    hsonar->last_trigger_tick = 0;

    for (uint8_t i = 0; i < HCSR04_AVERAGE_SAMPLES; i++) {
        hsonar->avg_buffer[i] = 0.0f;
    }

    /* Pastikan TRIG kembali LOW */
    HAL_GPIO_WritePin(hsonar->trig_port, hsonar->trig_pin, GPIO_PIN_RESET);
}

/* ----------------------------------------------------------------
 *  HCSR04_GetDistanceConstant
 *  Koreksi kecepatan suara berdasarkan suhu real-time
 *  Gunakan jika ada sensor suhu (DHT11, BME280)
 * ---------------------------------------------------------------- */
float HCSR04_GetDistanceConstant(float temperature_c)
{
    /* Rumus kecepatan suara: v = 331.4 + (0.606 * T)  m/s */
    float v_sound = 331.4f + (0.606f * temperature_c);

    /* Kembalikan konstanta jarak yang dikoreksi suhu
     * Sama dengan HCSR04_DISTANCE_CONSTANT tapi dinamis */
    return (v_sound / 2.0f / 10000.0f);
}
