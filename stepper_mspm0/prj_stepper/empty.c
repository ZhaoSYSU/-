#include "ti_msp_dl_config.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Balance ball controller for MSPM0G3507 (48-pin package).
 *
 * Wiring used by this file:
 *   STEP  -> PA17 / TIMA1_CCP0
 *   DIR   -> PA13
 *   EN    -> PA12
 *   DEBUG -> UART0 on PA10/PA11
 *   K230  -> UART1 on PB6/PB7
 */

#define SYSTEM_CLOCK_HZ            (32000000U)
#define TIMER_TICK_HZ              (1000000U)

#define D36A_MICROSTEP             (16U)
#define STEP_DEG_PER_MICROSTEP     (1.8f / (float)D36A_MICROSTEP)

#define STEP_TIMER_MIN_FREQ_HZ     (300U)
#define STEP_TIMER_MAX_FREQ_HZ     (2500U)
#define STEP_BURST_MAX_STEPS       (8U)
#define STEP_DEADBAND_DEG          (0.18f)
#define STEP_POSITIVE_DIR_LEVEL    (1U)
#define DRIVER_ENABLE_LEVEL        (1U)
#define MOTOR_SELF_TEST_ON_BOOT    (1U)
#define MOTOR_SELF_TEST_STEPS      (44U)
#define MOTOR_SELF_TEST_FREQ_HZ    (600U)

#define VISION_TIMEOUT_MS          (200U)
#define VISION_DEADBAND_MM         (1.0f)

#define K230_FRAME_HEAD            (0xAAU)
#define K230_FRAME_CMD_X_MM        (0x01U)
#define K230_FRAME_TAIL            (0x55U)

#define ENCODER_COUNTS_PER_REV     (4096.0f)
#define ENCODER_DEG_PER_COUNT      (360.0f / ENCODER_COUNTS_PER_REV)
#define ENCODER_DEADBAND_DEG       (0.08f)
#define ENCODER_PID_KP             (1.25f)
#define ENCODER_PID_KI             (0.10f)
#define ENCODER_PID_KD             (0.03f)
#define ENCODER_INTEGRAL_LIMIT     (35.0f)
#define ENCODER_MAX_OUTPUT_DEG     (2.0f)
#define ENCODER_COUNT_POLARITY     (1)

#define BALANCE_KP                 (0.055f)
#define BALANCE_KI                 (0.0008f)
#define BALANCE_KD                 (0.00018f)
#define BALANCE_MAX_TILT_DEG       (4.0f)
#define BALANCE_INTEGRAL_LIMIT     (250.0f)
#define BALANCE_POLARITY           (1.0f)

#define STEP_FREQ_FAR_HZ           (2200U)
#define STEP_FREQ_MED_HZ           (1600U)
#define STEP_FREQ_NEAR_HZ          (1100U)
#define STEP_FREQ_FINE_HZ          (700U)
#define STEP_FREQ_TINY_HZ          (400U)

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
    if (abs_tilt_deg > 8.0f) return STEP_FREQ_FAR_HZ;
    if (abs_tilt_deg > 4.0f) return STEP_FREQ_MED_HZ;
    if (abs_tilt_deg > 2.0f) return STEP_FREQ_NEAR_HZ;
    if (abs_tilt_deg > 0.8f) return STEP_FREQ_FINE_HZ;
    return STEP_FREQ_TINY_HZ;
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

static void Vision_HandlePositionMm(int16_t x_mm)
{
    g_ball_x_mm = (float)x_mm;
    g_ball_valid = 1U;
    g_ball_last_ms = g_ms;
}

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

static void Balance_UpdateTarget(void)
{
    float error_mm;
    float derivative;
    float tilt_deg;
    uint32_t age_ms = g_ms - g_ball_last_ms;

    if (g_ball_valid == 0U || age_ms > VISION_TIMEOUT_MS) {
        g_tilt_integral *= 0.90f;
        g_tilt_last_error_mm = 0.0f;
        g_target_tilt_deg = 0.0f;
        return;
    }

    error_mm = -g_ball_x_mm;
    if (fabsf(error_mm) < VISION_DEADBAND_MM) {
        error_mm = 0.0f;
    }

    if (fabsf(error_mm) < 80.0f) {
        g_tilt_integral += error_mm * 0.005f;
        if (g_tilt_integral >  BALANCE_INTEGRAL_LIMIT) g_tilt_integral =  BALANCE_INTEGRAL_LIMIT;
        if (g_tilt_integral < -BALANCE_INTEGRAL_LIMIT) g_tilt_integral = -BALANCE_INTEGRAL_LIMIT;
    } else {
        g_tilt_integral *= 0.96f;
    }

    derivative = (error_mm - g_tilt_last_error_mm) / 0.005f;
    g_tilt_last_error_mm = error_mm;

    tilt_deg = BALANCE_POLARITY * (
        BALANCE_KP * error_mm +
        BALANCE_KI * g_tilt_integral +
        BALANCE_KD * derivative);

    if (tilt_deg >  BALANCE_MAX_TILT_DEG) tilt_deg =  BALANCE_MAX_TILT_DEG;
    if (tilt_deg < -BALANCE_MAX_TILT_DEG) tilt_deg = -BALANCE_MAX_TILT_DEG;

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

    if (abs_error_deg < ENCODER_DEADBAND_DEG) {
        g_encoder_integral *= 0.90f;
        g_encoder_last_error_deg = error_deg;
        return;
    }

    if (abs_error_deg < 40.0f) {
        g_encoder_integral += error_deg * 0.005f;
        if (g_encoder_integral >  ENCODER_INTEGRAL_LIMIT) g_encoder_integral =  ENCODER_INTEGRAL_LIMIT;
        if (g_encoder_integral < -ENCODER_INTEGRAL_LIMIT) g_encoder_integral = -ENCODER_INTEGRAL_LIMIT;
    } else {
        g_encoder_integral *= 0.95f;
    }

    derivative = (error_deg - g_encoder_last_error_deg) / 0.005f;
    g_encoder_last_error_deg = error_deg;

    control_deg = ENCODER_PID_KP * error_deg +
        ENCODER_PID_KI * g_encoder_integral +
        ENCODER_PID_KD * derivative;

    if (control_deg >  ENCODER_MAX_OUTPUT_DEG) control_deg =  ENCODER_MAX_OUTPUT_DEG;
    if (control_deg < -ENCODER_MAX_OUTPUT_DEG) control_deg = -ENCODER_MAX_OUTPUT_DEG;

    if (fabsf(control_deg) < STEP_DEADBAND_DEG) {
        return;
    }

    steps = (uint32_t)(fabsf(control_deg) / STEP_DEG_PER_MICROSTEP + 0.5f);
    if (steps == 0U) {
        steps = 1U;
    }
    if (steps > STEP_BURST_MAX_STEPS) {
        steps = STEP_BURST_MAX_STEPS;
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
    printf("[BAL] t=%lu ms ball_x=%.1f mm target=%.2f deg enc=%.2f deg busy=%u homed=%u\r\n",
        (unsigned long)g_ms,
        (double)g_ball_x_mm,
        (double)g_target_tilt_deg,
        (double)g_encoder_actual_deg,
        (unsigned)g_stepper_busy,
        (unsigned)g_encoder_homed);
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
    printf("\r\nMSPM0G3507 48-pin balance-ball controller\r\n");
    printf("STEP  : PA17 / TIMA1_CCP0\r\n");
    printf("DIR   : PA13\r\n");
    printf("EN    : PA12, active level = %u\r\n", (unsigned)DRIVER_ENABLE_LEVEL);
    printf("ENC   : PA0=A, PA1=B, PA26=Z / GPIOA_INT\r\n");
    printf("DEBUG : PA10 TX, PA11 RX / UART0\r\n");
    printf("K230  : PB6 TX, PB7 RX / UART1\r\n");
    printf("K230 frame: AA 01 DH DL CS 55, signed x_mm, 115200 8N1\r\n");
    printf("D36A microstep = %u\r\n", (unsigned)D36A_MICROSTEP);
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

    while (1) {
        int16_t x_mm;
        uint32_t pending_ticks;

        if (g_vision_packet_ready != 0U) {
            __disable_irq();
            x_mm = g_vision_packet_x_mm;
            g_vision_packet_ready = 0U;
            __enable_irq();
            Vision_HandlePositionMm(x_mm);
        }

        __disable_irq();
        pending_ticks = g_ctrl_tick_pending;
        g_ctrl_tick_pending = 0U;
        __enable_irq();

        while (pending_ticks != 0U) {
            Balance_Tick5ms();
            pending_ticks--;
        }

        if ((g_ms - g_last_status_ms) >= 200U) {
            App_PrintStatus();
            g_last_status_ms = g_ms;
        }

        __WFI();
    }
}

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


