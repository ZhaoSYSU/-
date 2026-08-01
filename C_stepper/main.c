/*
 * Balance ball controller for MSPM0G3507 (48-pin package).
 *
 * Wiring used by this file:
 *   STEP  -> PA17 / TIMA1_CCP0
 *   DIR   -> PA13
 *   EN    -> PA12
 *   DEBUG -> UART0 on PA10/PA11 (printf + text tuning commands)
 *   K230  -> UART1 on PB6/PB7 (binary ball X frames)
 *
 * Tuning commands (send via DEBUG UART at 115200 8N1, end with \r or \n):
 *   kp_b <float>   ki_b <float>   kd_b <float>   -- balance (outer) PID
 *   kp_e <float>   ki_e <float>   kd_e <float>   -- encoder (inner) PID
 *   freq <0-4> <Hz>   -- set frequency tier (0=tiny,1=fine,2=near,3=med,4=far)
 *   burst <N>          -- max steps per burst (1-50)
 *   tilt_max <deg>     -- max tilt angle
 *   enc_max <deg>      -- max encoder output
 *   deadband <mm>      -- vision deadband
 *   timeout <ms>       -- vision timeout
 *   status             -- print all tunable parameters
 *   reset              -- reset PID integrals + encoder zero
 *   self_test          -- run motor self-test
 *   help               -- show command list
 *   save               -- show current params (copy-paste to save)
 */
#include "ti_msp_dl_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================== Hardware Constants (do not change) ==================== */
#define SYSTEM_CLOCK_HZ            (32000000U)
#define TIMER_TICK_HZ              (1000000U)

#define D36A_MICROSTEP             (16U)
#define STEP_DEG_PER_MICROSTEP     (1.8f / (float)D36A_MICROSTEP)

#define STEP_TIMER_MIN_FREQ_HZ     (300U)
#define STEP_TIMER_MAX_FREQ_HZ     (2500U)
#define STEP_DEADBAND_DEG          (0.18f)
#define STEP_POSITIVE_DIR_LEVEL    (1U)
#define DRIVER_ENABLE_LEVEL        (1U)
#define MOTOR_SELF_TEST_ON_BOOT    (1U)
#define MOTOR_SELF_TEST_STEPS      (44U)
#define MOTOR_SELF_TEST_FREQ_HZ    (600U)

#define K230_FRAME_HEAD            (0xAAU)
#define K230_FRAME_CMD_X_MM        (0x01U)
#define K230_FRAME_TAIL            (0x55U)

#define ENCODER_COUNTS_PER_REV     (4096.0f)
#define ENCODER_DEG_PER_COUNT      (360.0f / ENCODER_COUNTS_PER_REV)
#define ENCODER_COUNT_POLARITY     (1)

#define STEP_PORT                  GPIOA
#define STEP_PIN                   DL_GPIO_PIN_17
#define STEP_IOMUX                 IOMUX_PINCM39
#define STEP_IOMUX_FUNC            IOMUX_PINCM39_PF_TIMA1_CCP0

#define DIR_PORT                   GPIOA
#define DIR_PIN                    DL_GPIO_PIN_13
#define DIR_IOMUX                  IOMUX_PINCM35

#define EN_PORT                    GPIOA
#define EN_PIN                     DL_GPIO_PIN_12
#define EN_IOMUX                   IOMUX_PINCM34

#define DEBUG_UART                 UART0
#define DEBUG_TX_IOMUX             IOMUX_PINCM21
#define DEBUG_TX_FUNC              IOMUX_PINCM21_PF_UART0_TX
#define DEBUG_RX_IOMUX             IOMUX_PINCM22
#define DEBUG_RX_FUNC              IOMUX_PINCM22_PF_UART0_RX

#define VISION_UART                UART1
#define VISION_TX_IOMUX            IOMUX_PINCM23
#define VISION_TX_FUNC             IOMUX_PINCM23_PF_UART1_TX
#define VISION_RX_IOMUX            IOMUX_PINCM24
#define VISION_RX_FUNC             IOMUX_PINCM24_PF_UART1_RX

#define ENC_A_IOMUX                IOMUX_PINCM1
#define ENC_B_IOMUX                IOMUX_PINCM2
#define ENC_Z_IOMUX                IOMUX_PINCM59

#define ENC_A_PIN                  DL_GPIO_PIN_0
#define ENC_B_PIN                  DL_GPIO_PIN_1
#define ENC_Z_PIN                  DL_GPIO_PIN_26

#define STEP_TIMER                 TIMA1
#define CTRL_TIMER                 TIMG0

/* ==================== Runtime-adjustable PID Parameters ==================== */
/* Encoder (inner loop) PID — angle servo */
static float g_enc_kp                = 1.25f;
static float g_enc_ki                = 0.10f;
static float g_enc_kd                = 0.03f;
static float g_enc_integral_limit    = 35.0f;
static float g_enc_max_output_deg    = 2.0f;
static float g_enc_deadband_deg      = 0.08f;

/* Balance (outer loop) PID — ball position to target tilt */
static float g_bal_kp                = 0.055f;
static float g_bal_ki                = 0.0008f;
static float g_bal_kd                = 0.00018f;
static float g_bal_max_tilt_deg      = 4.0f;
static float g_bal_integral_limit    = 250.0f;
static float g_bal_polarity          = 1.0f;

/* Vision */
static float    g_vision_deadband_mm = 1.0f;
static uint32_t g_vision_timeout_ms  = 200U;

/* Stepper frequency tiers (Hz) — index 0=tiny, 1=fine, 2=near, 3=med, 4=far */
static uint32_t g_freq_tiers[5] = {400U, 700U, 1100U, 1600U, 2200U};
static float    g_freq_thresholds[4] = {0.8f, 2.0f, 4.0f, 8.0f};

/* Burst control */
static uint32_t g_burst_max_steps    = 8U;

/* System state */
static volatile uint32_t g_ms;
static volatile uint32_t g_ctrl_tick_pending;
static volatile uint8_t  g_vision_packet_ready;
static volatile int16_t  g_vision_packet_x_mm;

static volatile float    g_ball_x_mm;
static volatile uint8_t   g_ball_valid;
static volatile uint32_t  g_ball_last_ms;

static volatile uint8_t  g_k230_state;
static volatile uint8_t  g_k230_dh;
static volatile uint8_t  g_k230_dl;
static volatile uint8_t  g_k230_checksum;

static float g_target_tilt_deg;
static float g_tilt_integral;
static float g_tilt_last_error_mm;

static volatile int32_t  g_encoder_count;
static volatile uint8_t  g_encoder_prev_ab;
static volatile uint8_t  g_encoder_homed;
static float g_encoder_integral;
static float g_encoder_last_error_deg;
static float g_encoder_actual_deg;

static volatile uint8_t  g_stepper_busy;
static volatile uint32_t g_step_burst_remaining;
static volatile uint32_t g_step_burst_planned;
static volatile int8_t   g_step_burst_sign;

static uint32_t g_last_status_ms;

/* Forward declarations for static functions used by command parser */
static void Stepper_SetDirection(uint8_t high_level);
static void Stepper_StartBurst(uint8_t direction_high, uint32_t steps, uint32_t frequency_hz);
static void Stepper_StopUnsafe(void);

/* ==================== UART Tuning Command Parser ==================== */
#define CMD_BUF_SIZE 64
static char     g_cmd_buf[CMD_BUF_SIZE];
static uint8_t  g_cmd_idx;
static volatile uint8_t g_cmd_ready;

static void Cmd_ParseByte(uint8_t c)
{
    if (c == '\r' || c == '\n') {
        if (g_cmd_idx > 0) {
            g_cmd_buf[g_cmd_idx] = '\0';
            g_cmd_ready = 1U;
        }
        return;
    }
    if (c == '\b' || c == 0x7F) {
        if (g_cmd_idx > 0) g_cmd_idx--;
        return;
    }
    if (g_cmd_idx < (CMD_BUF_SIZE - 1)) {
        g_cmd_buf[g_cmd_idx++] = c;
    }
}

static void Cmd_PrintParams(void)
{
    printf("\r\n=== Tunable Parameters ===\r\n");
    printf("[Balance PID]  kp=%.6f  ki=%.6f  kd=%.6f  max_tilt=%.2f deg  int_limit=%.1f  polarity=%.1f\r\n",
        (double)g_bal_kp, (double)g_bal_ki, (double)g_bal_kd,
        (double)g_bal_max_tilt_deg, (double)g_bal_integral_limit, (double)g_bal_polarity);
    printf("[Encoder PID]  kp=%.4f  ki=%.4f  kd=%.4f  max_out=%.2f deg  int_limit=%.1f  deadband=%.3f deg\r\n",
        (double)g_enc_kp, (double)g_enc_ki, (double)g_enc_kd,
        (double)g_enc_max_output_deg, (double)g_enc_integral_limit, (double)g_enc_deadband_deg);
    printf("[Freq Tiers]   tiny=%lu  fine=%lu  near=%lu  med=%lu  far=%lu Hz\r\n",
        (unsigned long)g_freq_tiers[0], (unsigned long)g_freq_tiers[1],
        (unsigned long)g_freq_tiers[2], (unsigned long)g_freq_tiers[3],
        (unsigned long)g_freq_tiers[4]);
    printf("[Freq Thresh]  tiny<%.1f  fine<%.1f  near<%.1f  med<%.1f deg\r\n",
        (double)g_freq_thresholds[0], (double)g_freq_thresholds[1],
        (double)g_freq_thresholds[2], (double)g_freq_thresholds[3]);
    printf("[Burst]        max_steps=%lu\r\n", (unsigned long)g_burst_max_steps);
    printf("[Vision]       deadband=%.1f mm  timeout=%lu ms\r\n",
        (double)g_vision_deadband_mm, (unsigned long)g_vision_timeout_ms);
    printf("===========================\r\n");
}

static void Cmd_ResetPID(void)
{
    g_tilt_integral = 0.0f;
    g_tilt_last_error_mm = 0.0f;
    g_target_tilt_deg = 0.0f;
    g_encoder_integral = 0.0f;
    g_encoder_last_error_deg = 0.0f;
    g_encoder_count = 0;
    g_encoder_homed = 1U;
    g_ball_valid = 0U;
    printf("[OK] PID state reset, encoder zeroed\r\n");
}

static void Cmd_SelfTest(void)
{
    printf("[TEST] stepper self-test start\r\n");
    Stepper_SetDirection(STEP_POSITIVE_DIR_LEVEL);
    Stepper_StartBurst(STEP_POSITIVE_DIR_LEVEL, 44U, 600U);
    while (g_stepper_busy != 0U) {
        __WFI();
    }
    delay_cycles(SYSTEM_CLOCK_HZ / 5U);
    Stepper_StartBurst((uint8_t)!STEP_POSITIVE_DIR_LEVEL, 44U, 600U);
    while (g_stepper_busy != 0U) {
        __WFI();
    }
    printf("[TEST] stepper self-test done\r\n");
}

static void Cmd_Handle(void)
{
    char *token;
    float val_f;
    int   val_i;

    if (!g_cmd_ready) return;
    g_cmd_ready = 0U;
    g_cmd_idx = 0;

    token = strtok(g_cmd_buf, " \t");
    if (!token) return;

    /* === Balance PID === */
    if (strcmp(token, "kp_b") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_bal_kp = (float)atof(token);
            printf("[OK] bal_kp = %.6f\r\n", (double)g_bal_kp); }
    }
    else if (strcmp(token, "ki_b") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_bal_ki = (float)atof(token);
            printf("[OK] bal_ki = %.6f\r\n", (double)g_bal_ki); }
    }
    else if (strcmp(token, "kd_b") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_bal_kd = (float)atof(token);
            printf("[OK] bal_kd = %.6f\r\n", (double)g_bal_kd); }
    }
    else if (strcmp(token, "tilt_max") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_bal_max_tilt_deg = (float)atof(token);
            printf("[OK] tilt_max = %.2f deg\r\n", (double)g_bal_max_tilt_deg); }
    }
    else if (strcmp(token, "bal_int_lim") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_bal_integral_limit = (float)atof(token);
            printf("[OK] bal_int_limit = %.1f\r\n", (double)g_bal_integral_limit); }
    }

    /* === Encoder PID === */
    else if (strcmp(token, "kp_e") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_kp = (float)atof(token);
            printf("[OK] enc_kp = %.4f\r\n", (double)g_enc_kp); }
    }
    else if (strcmp(token, "ki_e") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_ki = (float)atof(token);
            printf("[OK] enc_ki = %.4f\r\n", (double)g_enc_ki); }
    }
    else if (strcmp(token, "kd_e") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_kd = (float)atof(token);
            printf("[OK] enc_kd = %.4f\r\n", (double)g_enc_kd); }
    }
    else if (strcmp(token, "enc_max") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_max_output_deg = (float)atof(token);
            printf("[OK] enc_max = %.2f deg\r\n", (double)g_enc_max_output_deg); }
    }
    else if (strcmp(token, "enc_int_lim") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_integral_limit = (float)atof(token);
            printf("[OK] enc_int_limit = %.1f\r\n", (double)g_enc_integral_limit); }
    }
    else if (strcmp(token, "enc_deadband") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_enc_deadband_deg = (float)atof(token);
            printf("[OK] enc_deadband = %.3f deg\r\n", (double)g_enc_deadband_deg); }
    }

    /* === Frequency Tiers === */
    else if (strcmp(token, "freq") == 0) {
        token = strtok(NULL, " \t");
        if (token) {
            val_i = atoi(token);
            token = strtok(NULL, " \t");
            if (token && val_i >= 0 && val_i <= 4) {
                g_freq_tiers[val_i] = (uint32_t)atoi(token);
                printf("[OK] freq[%d] = %lu Hz\r\n", val_i, (unsigned long)g_freq_tiers[val_i]);
            }
        }
    }
    else if (strcmp(token, "thresh") == 0) {
        token = strtok(NULL, " \t");
        if (token) {
            val_i = atoi(token);
            token = strtok(NULL, " \t");
            if (token && val_i >= 0 && val_i <= 3) {
                g_freq_thresholds[val_i] = (float)atof(token);
                printf("[OK] thresh[%d] = %.1f deg\r\n", val_i, (double)g_freq_thresholds[val_i]);
            }
        }
    }

    /* === Burst === */
    else if (strcmp(token, "burst") == 0) {
        token = strtok(NULL, " \t");
        if (token) {
            val_i = atoi(token);
            if (val_i >= 1 && val_i <= 50) {
                g_burst_max_steps = (uint32_t)val_i;
                printf("[OK] burst_max = %lu\r\n", (unsigned long)g_burst_max_steps);
            }
        }
    }

    /* === Vision === */
    else if (strcmp(token, "deadband") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_vision_deadband_mm = (float)atof(token);
            printf("[OK] deadband = %.1f mm\r\n", (double)g_vision_deadband_mm); }
    }
    else if (strcmp(token, "timeout") == 0) {
        token = strtok(NULL, " \t");
        if (token) { g_vision_timeout_ms = (uint32_t)atoi(token);
            printf("[OK] timeout = %lu ms\r\n", (unsigned long)g_vision_timeout_ms); }
    }

    /* === Actions === */
    else if (strcmp(token, "status") == 0 || strcmp(token, "st") == 0) {
        Cmd_PrintParams();
    }
    else if (strcmp(token, "reset") == 0) {
        Cmd_ResetPID();
    }
    else if (strcmp(token, "self_test") == 0) {
        Cmd_SelfTest();
    }
    else if (strcmp(token, "save") == 0) {
        Cmd_PrintParams();
        printf("[SAVE] Copy the above values into main.c #defines for permanent storage.\r\n");
    }
    else if (strcmp(token, "help") == 0 || strcmp(token, "?") == 0) {
        printf("\r\n=== Tuning Commands ===\r\n");
        printf("  kp_b/kd_b/ki_b <val>  -- balance PID\r\n");
        printf("  kp_e/kd_e/ki_e <val>  -- encoder PID\r\n");
        printf("  tilt_max <deg>         -- max tilt angle\r\n");
        printf("  enc_max <deg>           -- max encoder output\r\n");
        printf("  freq <0-4> <Hz>         -- set freq tier\r\n");
        printf("  thresh <0-3> <deg>      -- set freq threshold\r\n");
        printf("  burst <N>               -- max steps per burst\r\n");
        printf("  deadband <mm> / timeout <ms>\r\n");
        printf("  status | reset | self_test | save | help\r\n");
        printf("======================\r\n");
    }
    else {
        printf("[ERR] unknown: '%s'. Type 'help'.\r\n", token);
    }
}

/* ==================== Stepper Motor Low-Level ==================== */
static void Stepper_ForceLow(void)
{
    DL_TimerA_setCCPOutputDisabled(
        STEP_TIMER,
        DL_TIMER_CCP_DIS_OUT_LOW,
        DL_TIMER_CCP_DIS_OUT_LOW);
}

static void Stepper_EnablePWMOutput(void)
{
    DL_TimerA_setCCPOutputDisabled(
        STEP_TIMER,
        DL_TIMER_CCP_DIS_OUT_SET_BY_OCTL,
        DL_TIMER_CCP_DIS_OUT_SET_BY_OCTL);
}

static void Stepper_StopUnsafe(void)
{
    DL_TimerA_disableInterrupt(STEP_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerA_stopCounter(STEP_TIMER);
    DL_TimerA_setCaptureCompareValue(STEP_TIMER, 0U, DL_TIMER_CC_0_INDEX);
    Stepper_ForceLow();
    g_step_burst_remaining = 0U;
    g_step_burst_planned = 0U;
    g_stepper_busy = 0U;
}

static uint32_t Stepper_SelectFrequency(float abs_tilt_deg)
{
    /* Thresholds from g_freq_thresholds: [0]=tiny<, [1]=fine<, [2]=near<, [3]=med< */
    if (abs_tilt_deg > g_freq_thresholds[3]) return g_freq_tiers[4]; /* far */
    if (abs_tilt_deg > g_freq_thresholds[2]) return g_freq_tiers[3]; /* med  */
    if (abs_tilt_deg > g_freq_thresholds[1]) return g_freq_tiers[2]; /* near */
    if (abs_tilt_deg > g_freq_thresholds[0]) return g_freq_tiers[1]; /* fine */
    return g_freq_tiers[0];                                           /* tiny */
}

static uint16_t Stepper_FrequencyToPeriod(uint32_t frequency_hz)
{
    uint32_t period = (TIMER_TICK_HZ + (frequency_hz / 2U)) / frequency_hz;
    if (period < 20U) period = 20U;
    if (period > 65535U) period = 65535U;
    return (uint16_t)period;
}

static void Stepper_SetDirection(uint8_t high_level)
{
    if (high_level != 0U) {
        DL_GPIO_setPins(DIR_PORT, DIR_PIN);
    } else {
        DL_GPIO_clearPins(DIR_PORT, DIR_PIN);
    }
}

static void Stepper_StartBurst(uint8_t direction_high, uint32_t steps,
    uint32_t frequency_hz)
{
    uint16_t period;

    if (steps == 0U) {
        return;
    }

    if (frequency_hz < STEP_TIMER_MIN_FREQ_HZ) frequency_hz = STEP_TIMER_MIN_FREQ_HZ;
    if (frequency_hz > STEP_TIMER_MAX_FREQ_HZ) frequency_hz = STEP_TIMER_MAX_FREQ_HZ;

    if (g_stepper_busy != 0U) {
        return;
    }

    Stepper_SetDirection(direction_high);

    period = Stepper_FrequencyToPeriod(frequency_hz);
    DL_TimerA_stopCounter(STEP_TIMER);
    Stepper_EnablePWMOutput();
    DL_TimerA_setLoadValue(STEP_TIMER, (uint32_t)period - 1U);
    DL_TimerA_setTimerCount(STEP_TIMER, (uint32_t)period - 1U);
    DL_TimerA_setCaptureCompareValue(STEP_TIMER, (uint32_t)period / 2U,
        DL_TIMER_CC_0_INDEX);
    DL_TimerA_clearInterruptStatus(STEP_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerA_enableInterrupt(STEP_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);

    g_step_burst_remaining = steps;
    g_step_burst_planned = steps;
    g_step_burst_sign = (direction_high != 0U) ? 1 : -1;
    g_stepper_busy = 1U;
    DL_TimerA_startCounter(STEP_TIMER);
}

/* ==================== Vision Data Handling ==================== */
static void Vision_HandlePositionMm(int16_t x_mm)
{
    g_ball_x_mm = (float)x_mm;
    g_ball_valid = 1U;
    g_ball_last_ms = g_ms;
}

/* ==================== Encoder ==================== */
static uint8_t Encoder_ReadAB(void)
{
    uint8_t ab = 0U;

    if ((DL_GPIO_readPins(GPIOA, ENC_A_PIN) & ENC_A_PIN) != 0U) {
        ab |= 0x01U;
    }
    if ((DL_GPIO_readPins(GPIOA, ENC_B_PIN) & ENC_B_PIN) != 0U) {
        ab |= 0x02U;
    }

    return ab;
}

static float Encoder_GetAngleDeg(void)
{
    int32_t count;

    __disable_irq();
    count = g_encoder_count;
    __enable_irq();

    return (float)count * ENCODER_DEG_PER_COUNT;
}

static void Encoder_HandleZ(void)
{
    g_encoder_count = 0;
    g_encoder_homed = 1U;
    g_encoder_integral = 0.0f;
    g_encoder_last_error_deg = 0.0f;
}

static void Encoder_HandleABEdge(void)
{
    static const int8_t quad_table[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0
    };
    uint8_t curr_ab = Encoder_ReadAB();
    uint8_t prev_ab = g_encoder_prev_ab;
    int8_t delta;

    if (curr_ab == prev_ab) {
        return;
    }

    delta = quad_table[((prev_ab & 0x03U) << 2U) | (curr_ab & 0x03U)];
    if (delta != 0) {
        g_encoder_count += (int32_t)ENCODER_COUNT_POLARITY * (int32_t)delta;
    }
    g_encoder_prev_ab = curr_ab;
}

static void Encoder_ProcessInterrupts(void)
{
    uint32_t pending = DL_GPIO_getRawInterruptStatus(GPIOA,
        ENC_A_PIN | ENC_B_PIN | ENC_Z_PIN);

    if (pending == 0U) {
        return;
    }

    if ((pending & ENC_Z_PIN) != 0U) {
        Encoder_HandleZ();
    } else if ((pending & (ENC_A_PIN | ENC_B_PIN)) != 0U) {
        Encoder_HandleABEdge();
    }

    DL_GPIO_clearInterruptStatus(GPIOA, pending);
}

/* ==================== K230 Binary Protocol Parser ==================== */
static void K230_ParseByte(uint8_t byte)
{
    switch (g_k230_state) {
        case 0U:
            if (byte == K230_FRAME_HEAD) {
                g_k230_state = 1U;
            }
            break;

        case 1U:
            if (byte == K230_FRAME_CMD_X_MM) {
                g_k230_checksum = byte;
                g_k230_state = 2U;
            } else {
                g_k230_state = (byte == K230_FRAME_HEAD) ? 1U : 0U;
            }
            break;

        case 2U:
            g_k230_dh = byte;
            g_k230_checksum = (uint8_t)(g_k230_checksum + byte);
            g_k230_state = 3U;
            break;

        case 3U:
            g_k230_dl = byte;
            g_k230_checksum = (uint8_t)(g_k230_checksum + byte);
            g_k230_state = 4U;
            break;

        case 4U:
            if (byte == g_k230_checksum) {
                g_k230_state = 5U;
            } else {
                g_k230_state = 0U;
            }
            break;

        default:
            if (byte == K230_FRAME_TAIL) {
                uint16_t raw = ((uint16_t)g_k230_dh << 8U) | (uint16_t)g_k230_dl;
                g_vision_packet_x_mm = (int16_t)raw;
                g_vision_packet_ready = 1U;
            }
            g_k230_state = 0U;
            break;
    }
}

/* ==================== Balance Control ==================== */
static void Balance_UpdateTarget(void)
{
    float error_mm;
    float derivative;
    float tilt_deg;
    uint32_t age_ms = g_ms - g_ball_last_ms;

    if (g_ball_valid == 0U || age_ms > g_vision_timeout_ms) {
        g_tilt_integral *= 0.90f;
        g_tilt_last_error_mm = 0.0f;
        g_target_tilt_deg = 0.0f;
        return;
    }

    error_mm = -g_ball_x_mm;
    if (fabsf(error_mm) < g_vision_deadband_mm) {
        error_mm = 0.0f;
    }

    if (fabsf(error_mm) < 80.0f) {
        g_tilt_integral += error_mm * 0.005f;
        if (g_tilt_integral >  g_bal_integral_limit) g_tilt_integral =  g_bal_integral_limit;
        if (g_tilt_integral < -g_bal_integral_limit) g_tilt_integral = -g_bal_integral_limit;
    } else {
        g_tilt_integral *= 0.96f;
    }

    derivative = (error_mm - g_tilt_last_error_mm) / 0.005f;
    g_tilt_last_error_mm = error_mm;

    tilt_deg = g_bal_polarity * (
        g_bal_kp * error_mm +
        g_bal_ki * g_tilt_integral +
        g_bal_kd * derivative);

    if (tilt_deg >  g_bal_max_tilt_deg) tilt_deg =  g_bal_max_tilt_deg;
    if (tilt_deg < -g_bal_max_tilt_deg) tilt_deg = -g_bal_max_tilt_deg;

    g_target_tilt_deg = tilt_deg;
}

static void Balance_ServiceMotor(void)
{
    float actual_deg;
    float error_deg;
    float derivative;
    float control_deg;
    float abs_error_deg;
    uint32_t steps;
    uint32_t frequency_hz;
    uint8_t direction_high;

    if (g_stepper_busy != 0U) {
        return;
    }

    actual_deg = Encoder_GetAngleDeg();
    g_encoder_actual_deg = actual_deg;
    error_deg = g_target_tilt_deg - actual_deg;
    abs_error_deg = fabsf(error_deg);

    if (abs_error_deg < g_enc_deadband_deg) {
        g_encoder_integral *= 0.90f;
        g_encoder_last_error_deg = error_deg;
        return;
    }

    if (abs_error_deg < 40.0f) {
        g_encoder_integral += error_deg * 0.005f;
        if (g_encoder_integral >  g_enc_integral_limit) g_encoder_integral =  g_enc_integral_limit;
        if (g_encoder_integral < -g_enc_integral_limit) g_encoder_integral = -g_enc_integral_limit;
    } else {
        g_encoder_integral *= 0.95f;
    }

    derivative = (error_deg - g_encoder_last_error_deg) / 0.005f;
    g_encoder_last_error_deg = error_deg;

    control_deg = g_enc_kp * error_deg +
        g_enc_ki * g_encoder_integral +
        g_enc_kd * derivative;

    if (control_deg >  g_enc_max_output_deg) control_deg =  g_enc_max_output_deg;
    if (control_deg < -g_enc_max_output_deg) control_deg = -g_enc_max_output_deg;

    if (fabsf(control_deg) < STEP_DEADBAND_DEG) {
        return;
    }

    steps = (uint32_t)(fabsf(control_deg) / STEP_DEG_PER_MICROSTEP + 0.5f);
    if (steps == 0U) {
        steps = 1U;
    }
    if (steps > g_burst_max_steps) {
        steps = g_burst_max_steps;
    }

    direction_high = (control_deg >= 0.0f) ? STEP_POSITIVE_DIR_LEVEL : (uint8_t)!STEP_POSITIVE_DIR_LEVEL;
    frequency_hz = Stepper_SelectFrequency(abs_error_deg);
    Stepper_StartBurst(direction_high, steps, frequency_hz);
}

static void Balance_Tick5ms(void)
{
    g_ms += 5U;
    Balance_UpdateTarget();
    Balance_ServiceMotor();
}

static void App_RunMotorSelfTest(void)
{
#if MOTOR_SELF_TEST_ON_BOOT
    printf("[TEST] stepper self-test start\r\n");

    Stepper_StartBurst(STEP_POSITIVE_DIR_LEVEL,
        MOTOR_SELF_TEST_STEPS, MOTOR_SELF_TEST_FREQ_HZ);
    while (g_stepper_busy != 0U) {
        __WFI();
    }

    delay_cycles(SYSTEM_CLOCK_HZ / 5U);

    Stepper_StartBurst((uint8_t)!STEP_POSITIVE_DIR_LEVEL,
        MOTOR_SELF_TEST_STEPS, MOTOR_SELF_TEST_FREQ_HZ);
    while (g_stepper_busy != 0U) {
        __WFI();
    }

    printf("[TEST] stepper self-test done\r\n");
#endif
}

static void App_PrintStatus(void)
{
    printf("[BAL] t=%lums ball=%.1fmm target=%.2fdeg enc=%.2fdeg busy=%u "
           "kp_b=%.4f kp_e=%.2f\r\n",
        (unsigned long)g_ms,
        (double)g_ball_x_mm,
        (double)g_target_tilt_deg,
        (double)g_encoder_actual_deg,
        (unsigned)g_stepper_busy,
        (double)g_bal_kp,
        (double)g_enc_kp);
}

static void App_Init(void)
{
    static const DL_TimerA_ClockConfig stepClockConfig = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 31,
    };
    static const DL_TimerA_PWMConfig stepPwmConfig = {
        .period = 1000U,
        .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
        .isTimerWithFourCC = false,
        .startTimer = false,
    };
    static const DL_TimerG_ClockConfig ctrlClockConfig = {
        .clockSel = DL_TIMER_CLOCK_BUSCLK,
        .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
        .prescale = 31,
    };
    static const DL_TimerG_TimerConfig ctrlTimerConfig = {
        .period = 4999U,
        .timerMode = DL_TIMER_TIMER_MODE_PERIODIC,
        .startTimer = false,
    };
    static const DL_UART_Main_ClockConfig uartClockConfig = {
        .clockSel = DL_UART_MAIN_CLOCK_BUSCLK,
        .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1,
    };
    static const DL_UART_Main_Config uartConfig = {
        .mode = DL_UART_MAIN_MODE_NORMAL,
        .direction = DL_UART_MAIN_DIRECTION_TX_RX,
        .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
        .parity = DL_UART_MAIN_PARITY_NONE,
        .wordLength = DL_UART_MAIN_WORD_LENGTH_8_BITS,
        .stopBits = DL_UART_MAIN_STOP_BITS_ONE,
    };

    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(STEP_TIMER);
    DL_TimerG_enablePower(CTRL_TIMER);
    DL_UART_Main_enablePower(DEBUG_UART);
    DL_UART_Main_enablePower(VISION_UART);
    delay_cycles(POWER_STARTUP_DELAY);

    DL_GPIO_initPeripheralOutputFunction(STEP_IOMUX, STEP_IOMUX_FUNC);
    DL_GPIO_initDigitalOutput(DIR_IOMUX);
    DL_GPIO_initDigitalOutput(EN_IOMUX);
    DL_GPIO_initPeripheralOutputFunction(DEBUG_TX_IOMUX, DEBUG_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(DEBUG_RX_IOMUX, DEBUG_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(VISION_TX_IOMUX, VISION_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(VISION_RX_IOMUX, VISION_RX_FUNC);

    DL_GPIO_initDigitalInputFeatures(ENC_A_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(ENC_B_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_initDigitalInputFeatures(ENC_Z_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_HYSTERESIS_ENABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_enableOutput(DIR_PORT, DIR_PIN);
    DL_GPIO_enableOutput(EN_PORT, EN_PIN);
    DL_GPIO_clearPins(DIR_PORT, DIR_PIN);
    if (DRIVER_ENABLE_LEVEL != 0U) {
        DL_GPIO_setPins(EN_PORT, EN_PIN);
    } else {
        DL_GPIO_clearPins(EN_PORT, EN_PIN);
    }

    DL_TimerA_setClockConfig(STEP_TIMER, (DL_TimerA_ClockConfig *)&stepClockConfig);
    DL_TimerA_initPWMMode(STEP_TIMER, (DL_TimerA_PWMConfig *)&stepPwmConfig);
    DL_TimerA_setCCPDirection(STEP_TIMER, DL_TIMER_CC0_OUTPUT);
    DL_TimerA_setCCPOutputDisabled(STEP_TIMER,
        DL_TIMER_CCP_DIS_OUT_LOW, DL_TIMER_CCP_DIS_OUT_LOW);
    DL_TimerA_clearInterruptStatus(STEP_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerA_enableClock(STEP_TIMER);

    DL_TimerG_setClockConfig(CTRL_TIMER, (DL_TimerG_ClockConfig *)&ctrlClockConfig);
    DL_TimerG_initTimerMode(CTRL_TIMER, (DL_TimerG_TimerConfig *)&ctrlTimerConfig);
    DL_TimerG_clearInterruptStatus(CTRL_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableInterrupt(CTRL_TIMER, DL_TIMER_INTERRUPT_ZERO_EVENT);
    DL_TimerG_enableClock(CTRL_TIMER);
    DL_TimerG_startCounter(CTRL_TIMER);

    g_encoder_prev_ab = Encoder_ReadAB();
    g_encoder_count = 0;
    g_encoder_homed = 0U;

    DL_GPIO_setLowerPinsPolarity(GPIOA,
        DL_GPIO_PIN_0_EDGE_RISE_FALL | DL_GPIO_PIN_1_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(GPIOA, DL_GPIO_PIN_26_EDGE_RISE);
    DL_GPIO_clearInterruptStatus(GPIOA, ENC_A_PIN | ENC_B_PIN | ENC_Z_PIN);
    DL_GPIO_enableInterrupt(GPIOA, ENC_A_PIN | ENC_B_PIN | ENC_Z_PIN);

    DL_UART_Main_setClockConfig(DEBUG_UART, (DL_UART_Main_ClockConfig *)&uartClockConfig);
    DL_UART_Main_init(DEBUG_UART, (DL_UART_Main_Config *)&uartConfig);
    DL_UART_Main_setOversampling(DEBUG_UART, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(DEBUG_UART, 17U, 23U);
    DL_UART_Main_enable(DEBUG_UART);

    DL_UART_Main_setClockConfig(VISION_UART, (DL_UART_Main_ClockConfig *)&uartClockConfig);
    DL_UART_Main_init(VISION_UART, (DL_UART_Main_Config *)&uartConfig);
    DL_UART_Main_setOversampling(VISION_UART, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(VISION_UART, 17U, 23U);
    DL_UART_Main_enableInterrupt(VISION_UART, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enable(VISION_UART);

    NVIC_ClearPendingIRQ(TIMA1_INT_IRQn);
    NVIC_EnableIRQ(TIMA1_INT_IRQn);
    NVIC_ClearPendingIRQ(TIMG0_INT_IRQn);
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
    NVIC_ClearPendingIRQ(GPIOA_INT_IRQn);
    NVIC_EnableIRQ(GPIOA_INT_IRQn);
    NVIC_ClearPendingIRQ(UART1_INT_IRQn);
    NVIC_EnableIRQ(UART1_INT_IRQn);
}

static void App_PrintWiring(void)
{
    printf("\r\n========================================\r\n");
    printf("MSPM0G3507 Balance-Ball Controller\r\n");
    printf("========================================\r\n");
    printf("STEP  : PA17 / TIMA1_CCP0\r\n");
    printf("DIR   : PA13\r\n");
    printf("EN    : PA12, active = %u\r\n", (unsigned)DRIVER_ENABLE_LEVEL);
    printf("ENC   : PA0=A, PA1=B, PA26=Z\r\n");
    printf("DEBUG : PA10 TX, PA11 RX / UART0 115200\r\n");
    printf("K230  : PB6 TX, PB7 RX / UART1 115200\r\n");
    printf("Tuning: send text commands on DEBUG UART\r\n");
    printf("        type 'help' for command list\r\n");
    printf("========================================\r\n\r\n");
}

int fputc(int ch, FILE *stream)
{
    (void)stream;
    DL_UART_Main_transmitDataBlocking(DEBUG_UART, (uint8_t)ch);
    return ch;
}

int main(void)
{
    SYSCFG_DL_init();
    App_Init();
    App_PrintWiring();
    App_RunMotorSelfTest();

    g_cmd_idx = 0;
    g_cmd_ready = 0U;

    while (1) {
        int16_t x_mm;
        uint32_t pending_ticks;

        /* Handle vision data */
        if (g_vision_packet_ready != 0U) {
            __disable_irq();
            x_mm = g_vision_packet_x_mm;
            g_vision_packet_ready = 0U;
            __enable_irq();
            Vision_HandlePositionMm(x_mm);
        }

        /* Handle tuning commands from DEBUG UART */
        while (!DL_UART_Main_isRXFIFOEmpty(DEBUG_UART)) {
            uint8_t c = DL_UART_Main_receiveData(DEBUG_UART);
            Cmd_ParseByte(c);
        }
        if (g_cmd_ready) {
            Cmd_Handle();
        }

        /* Process control ticks */
        __disable_irq();
        pending_ticks = g_ctrl_tick_pending;
        g_ctrl_tick_pending = 0U;
        __enable_irq();

        while (pending_ticks != 0U) {
            Balance_Tick5ms();
            pending_ticks--;
        }

        /* Periodic status */
        if ((g_ms - g_last_status_ms) >= 200U) {
            App_PrintStatus();
            g_last_status_ms = g_ms;
        }

        __WFI();
    }
}

/* ==================== ISRs ==================== */
void TIMA1_IRQHandler(void)
{
    if (DL_TimerA_getPendingInterrupt(STEP_TIMER) != DL_TIMER_IIDX_ZERO) {
        return;
    }

    if (g_step_burst_remaining == 0U) {
        Stepper_StopUnsafe();
        return;
    }

    g_step_burst_remaining--;
    if (g_step_burst_remaining == 0U) {
        Stepper_StopUnsafe();
    }
}

void TIMG0_IRQHandler(void)
{
    if (DL_TimerG_getPendingInterrupt(CTRL_TIMER) != DL_TIMER_IIDX_ZERO) {
        return;
    }

    g_ctrl_tick_pending++;
}

void GPIOA_IRQHandler(void)
{
    Encoder_ProcessInterrupts();
}

void UART1_IRQHandler(void)
{
    uint8_t c;

    if (DL_UART_Main_getPendingInterrupt(VISION_UART) != DL_UART_MAIN_IIDX_RX) {
        return;
    }

    while (!DL_UART_Main_isRXFIFOEmpty(VISION_UART)) {
        c = DL_UART_Main_receiveData(VISION_UART);
        K230_ParseByte(c);
    }
}
