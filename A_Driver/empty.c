/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"

#define PWM_PERIOD_COUNTS          (3200U)
#define DEFAULT_TARGET_RPM          (80)
#define SPEED_SAMPLE_MS            (50U)
#define DEFAULT_FEEDFORWARD_DUTY_PERCENT (24)
#define TARGET_RAMP_RPM_PER_SAMPLE (12)
#define OLED_REFRESH_INTERVAL_MS   (200U)
#define PID_SCALE                  (1000)
#define PID_KP                     (1050)
#define PID_KI                     (10)
#define PID_KD                     (45)
#define PID_INTEGRAL_LIMIT         (800)
#define DUTY_LIMIT_PERCENT         (100)
#define MEASURED_RPM_LIMIT         (250)

#define VISION_BASE_RPM            (40)
#define VISION_TURN_GAIN           (35)
#define VISION_MAX_RPM             (95)
#define VISION_LOST_TIMEOUT_MS     (500U)
#define VISION_DEV_LIMIT           (320)

#define TRACK_SENSOR_COUNT         (8U)
#define TRACK_SENSOR_ACTIVE_LOW    (1U)
#define TRACK_SENSOR_REVERSE_ORDER (0U)
#define TRACK_BASE_RPM             (150)
#define TRACK_SEARCH_RPM           (20)
#define TRACK_TURN_GAIN            (10)
#define TRACK_CENTER_DEADBAND      (0)
#define TRACK_TURN_LIMIT           (120)
#define TRACK_MAX_RPM              (250)

#define K230_FRAME_HEAD            (0xAAU)
#define K230_FRAME_TAIL            (0x55U)
#define K230_CMD_DEVIATION         (0x01U)
#define K230_CMD_STATUS            (0x03U)
#define K230_CMD_TRACK             (0x04U)
#define K230_STATUS_OK             (0U)
#define K230_STATUS_LOST           (2U)
#define UART_DEBUG_ECHO            (0U)
#define UART_SELF_TEST_MODE        (0U)

/* Please update this value after confirming your MG310 encoder specification. */
#define ENCODER_COUNTS_PER_REV     (1040U)
#define LEFT_ENCODER_DIRECTION     (-1)
#define RIGHT_ENCODER_DIRECTION    (1)
#define LEFT_MOTOR_DIRECTION       (1)
#define RIGHT_MOTOR_DIRECTION      (1)

#define LEFT_PWM_INDEX             DL_TIMER_CC_0_INDEX

/* ---- 步进板 UART 通信 ---- */
#define STEPPER_UART              UART_1_INST
#define STEPPER_UART_INT_IRQN     UART_1_INST_INT_IRQN
#define STEPPER_TX_PORT           GPIOA
#define STEPPER_TX_PIN            DL_GPIO_PIN_8
#define STEPPER_RX_PORT           GPIOA
#define STEPPER_RX_PIN            DL_GPIO_PIN_9
#define STEPPER_BAUD              115200U
#define STEPPER_FRAME_HEAD        0xAAU
#define STEPPER_FRAME_TAIL        0x55U
#define STEPPER_CMD_BALL          0x01U
#define STEPPER_CMD_CAR           0x05U
#define RIGHT_PWM_INDEX            DL_TIMER_CC_1_INDEX

#define LEFT_IN1_PORT              GPIOB
#define LEFT_IN1_PIN               DL_GPIO_PIN_0
#define LEFT_IN2_PORT              GPIOA
#define LEFT_IN2_PIN               DL_GPIO_PIN_15
#define RIGHT_IN1_PORT             GPIOB
#define RIGHT_IN1_PIN              DL_GPIO_PIN_18
#define RIGHT_IN2_PORT             GPIOB
#define RIGHT_IN2_PIN              DL_GPIO_PIN_19
#define MOTOR_STBY_PORT            GPIOA
#define MOTOR_STBY_PIN             DL_GPIO_PIN_28

#define OLED_SCLK_PORT             GPIOB
#define OLED_SCLK_PIN              DL_GPIO_PIN_9
#define OLED_MOSI_PORT             GPIOB
#define OLED_MOSI_PIN              DL_GPIO_PIN_8
#define OLED_CS_PORT               GPIOB
#define OLED_CS_PIN                DL_GPIO_PIN_17
#define OLED_DC_PORT               GPIOA
#define OLED_DC_PIN                DL_GPIO_PIN_12
#define OLED_RST_PORT              GPIOA
#define OLED_RST_PIN               DL_GPIO_PIN_13

#define ENC_PORT                   GPIOB
#define ENC_LEFT_A_PIN             DL_GPIO_PIN_1
#define ENC_LEFT_B_PIN             DL_GPIO_PIN_4
#define ENC_RIGHT_A_PIN            DL_GPIO_PIN_6
#define ENC_RIGHT_B_PIN            DL_GPIO_PIN_7

#define USER_LED1_PORT             GPIOB
#define USER_LED1_PIN              DL_GPIO_PIN_22
#define USER_LED1_IOMUX            IOMUX_PINCM50

typedef struct {
    int32_t finalTargetRpm;
    int32_t targetRpm;
    int32_t measuredRpm;
    int32_t errorSum;
    int32_t lastError;
    int16_t dutyPercent;
} MotorPidController;
static volatile int32_t gLeftEncoderCount;
static volatile int32_t gRightEncoderCount;
static uint8_t gLeftEncoderState;
static uint8_t gRightEncoderState;
static uint8_t gOledBuffer[1024];
static MotorPidController gLeftPid;
static MotorPidController gRightPid;
static volatile int16_t gVisionDeviation;
static volatile int16_t gVisionTrackValue;
static volatile int16_t gTrackRawValue;
static volatile bool gVisionLineOk;
static volatile bool gVisionDataValid;
static volatile uint32_t gVisionLastRxMs;
static volatile int16_t gTrackLastError;
static volatile int16_t gTrackLeftCount;
static volatile int16_t gTrackRightCount;
static uint32_t gSystemMs;
static volatile uint32_t gK230RxByteCount;
static volatile uint32_t gK230FrameCount;
static volatile uint32_t gK230HeadCount;
static volatile uint32_t gK230BadFrameCount;
static volatile uint8_t gK230LastByte;
static volatile uint8_t gK230LastCmd;
static volatile uint32_t gUartSelfTestTxCount;

void Motor_SetTargetRPM(int32_t leftRpm, int32_t rightRpm);
void Motor_Stop(void);


static void motor_set_left(int16_t dutyPercent);
static void motor_set_right(int16_t dutyPercent);
static void motor_set_pwm(uint32_t ccIndex, uint8_t dutyPercent);
static void pid_set_target(MotorPidController *pid, int32_t targetRpm);
static int16_t pid_update(MotorPidController *pid, int32_t measuredRpm);
static void pid_apply_target_ramp(MotorPidController *pid);
static int16_t pid_feedforward_duty(int32_t targetRpm);
static int16_t clamp_duty(int32_t dutyPercent);
static int32_t clamp_i32(int32_t value, int32_t minValue, int32_t maxValue);
static int32_t rpm_from_encoder_delta(int32_t delta);
static uint8_t encoder_read_left_state(void);
static uint8_t encoder_read_right_state(void);
static void encoder_update(void);
static void oled_init(void);
static void oled_clear(void);
static void oled_refresh(void);
static void oled_show_string(uint8_t x, uint8_t page, const char *text);
static void oled_show_signed_number(uint8_t x, uint8_t page, int32_t value);
static void oled_draw_char(uint8_t x, uint8_t page, char ch);
static void oled_write_command(uint8_t command);
static void oled_write_data(uint8_t data);
static void oled_write_byte(uint8_t data);
static void gpio_write(GPIO_Regs *port, uint32_t pin, bool high);
static void user_led1_init_on(void);
static uint8_t track_read_sensor_mask(void);
static int32_t track_count_active_sensors(uint8_t sensorMask, uint8_t startBit, uint8_t endBit);
static void track_update_target(void);
static void k230_parse_byte(uint8_t data);
static void k230_handle_packet(uint8_t cmd, uint8_t dh, uint8_t dl);
static void k230_poll_uart(void);
static void stepper_uart_init(void);
static void stepper_send_frame(uint8_t cmd, uint8_t dh, uint8_t dl);
static void stepper_send_car_status(int32_t leftRpm, int32_t rightRpm);
static void uart_self_test_send(void)
{
    static const uint8_t frames[] = {
        0xAA, 0x01, 0x00, 0x00, 0x01, 0x55,
        0xAA, 0x03, 0x00, 0x00, 0x03, 0x55,
        0xAA, 0x04, 0x00, 0x00, 0x04, 0x55
    };

    gUartSelfTestTxCount++;
    for (uint8_t i = 0U; i < sizeof(frames); i++) {
        uint32_t timeout = 100000U;
        while (DL_UART_Main_isTXFIFOFull(UART_0_INST) && (timeout > 0U)) {
            timeout--;
        }
        if (timeout > 0U) {
            DL_UART_Main_transmitData(UART_0_INST, frames[i]);
        }
    }
}static void uart_debug_write_char(char ch)
{
    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) ch);
}

static void uart_debug_rx_byte(uint8_t data)
{
    static const char hex[] = "0123456789ABCDEF";
    static uint8_t byteCount;

    uart_debug_write_char(hex[(data >> 4) & 0x0F]);
    uart_debug_write_char(hex[data & 0x0F]);
    byteCount++;

    if ((data == K230_FRAME_TAIL) || (byteCount >= 6U)) {
        uart_debug_write_char('\r');
        uart_debug_write_char('\n');
        byteCount = 0U;
    } else {
        uart_debug_write_char(' ');
    }
}
static void delay_ms_with_k230_poll(uint32_t ms);
static void uart_debug_rx_byte(uint8_t data);
static void uart_debug_write_char(char ch);
static void uart_self_test_send(void);

int main(void)
{
    SYSCFG_DL_init();
    user_led1_init_on();

    gLeftEncoderState  = encoder_read_left_state();
    gRightEncoderState = encoder_read_right_state();
    NVIC_EnableIRQ(ENCODER_GPIO_INT_IRQN);
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

    oled_init();
    oled_clear();
    oled_show_string(0, 0, "MSPM0G3507");
    oled_show_string(0, 2, "TRACK READY");
    oled_refresh();

    DL_GPIO_setPins(MOTOR_STBY_PORT, MOTOR_STBY_PIN);
    Motor_Stop();
    DL_TimerG_startCounter(MOTOR_PWM_INST);

    stepper_uart_init();  /* 初始化步进板通信 UART1 */

    while (1) {
        int32_t leftStart;
        int32_t rightStart;
        int32_t leftDelta;
        int32_t rightDelta;
        int32_t leftRpm;
        int32_t rightRpm;

        __disable_irq();
        leftStart  = gLeftEncoderCount;
        rightStart = gRightEncoderCount;
        __enable_irq();

        if (UART_SELF_TEST_MODE != 0U) {
            uart_self_test_send();
        }
        delay_ms_with_k230_poll(SPEED_SAMPLE_MS);
        gSystemMs += SPEED_SAMPLE_MS;

        __disable_irq();
        leftDelta  = gLeftEncoderCount - leftStart;
        rightDelta = gRightEncoderCount - rightStart;
        __enable_irq();

        leftRpm = rpm_from_encoder_delta(leftDelta);
        rightRpm = rpm_from_encoder_delta(rightDelta);

        track_update_target();
        motor_set_left(pid_update(&gLeftPid, leftRpm));
        motor_set_right(pid_update(&gRightPid, rightRpm));

        /* 定期发送车速/轨道给步进板 (每200ms一次) */
        stepper_send_car_status(leftRpm, rightRpm);

        if ((gSystemMs % OLED_REFRESH_INTERVAL_MS) != 0U) {
            continue;
        }

        oled_clear();
        if (UART_SELF_TEST_MODE != 0U) {
            oled_show_string(0, 0, "UART TEST");
            oled_show_string(0, 1, "TX:");
            oled_show_signed_number(24, 1, gUartSelfTestTxCount);
            oled_show_string(0, 2, "RX:");
            oled_show_signed_number(24, 2, gK230RxByteCount);
            oled_show_string(0, 3, "PK:");
            oled_show_signed_number(24, 3, gK230FrameCount);
            oled_show_string(0, 4, "H:");
            oled_show_signed_number(18, 4, gK230HeadCount);
            oled_show_string(0, 5, "E:");
            oled_show_signed_number(18, 5, gK230BadFrameCount);
            oled_show_string(0, 6, "B:");
            oled_show_signed_number(18, 6, gK230LastByte);
            oled_refresh();
            continue;
        }
        oled_show_string(0, 0, "L RPM:");
        oled_show_signed_number(48, 0, leftRpm);
        oled_show_string(0, 1, "L SET:");
        oled_show_signed_number(48, 1, gLeftPid.finalTargetRpm);
        oled_show_string(0, 2, "R RPM:");
        oled_show_signed_number(48, 2, rightRpm);
        oled_show_string(0, 3, "R SET:");
        oled_show_signed_number(48, 3, gRightPid.finalTargetRpm);
        oled_show_string(0, 4, "MASK:");
        oled_show_signed_number(42, 4, gVisionTrackValue);
        oled_show_string(0, 5, "ERR:");
        oled_show_signed_number(36, 5, gVisionDeviation);
        oled_show_string(0, 6, "LC:");
        oled_show_signed_number(24, 6, gTrackLeftCount);
        oled_show_string(60, 6, "RC:");
        oled_show_signed_number(84, 6, gTrackRightCount);
        oled_show_string(0, 7, "RAW:");
        oled_show_signed_number(36, 7, gTrackRawValue);
        oled_refresh();
    }
}

void GROUP1_IRQHandler(void)
{
    switch (DL_Interrupt_getPendingGroup(DL_INTERRUPT_GROUP_1)) {
        case ENCODER_GPIO_INT_IIDX:
            encoder_update();
            break;
        default:
            break;
    }
}
void UART_0_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_0_INST) == DL_UART_MAIN_IIDX_RX) {
        k230_poll_uart();
    }
}
static void k230_poll_uart(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        uint8_t data = DL_UART_Main_receiveData(UART_0_INST);
        gK230LastByte = data;
        gK230RxByteCount++;
        if (UART_DEBUG_ECHO != 0U) { uart_debug_rx_byte(data); }
        k230_parse_byte(data);
    }
}

static void delay_ms_with_k230_poll(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        k230_poll_uart();
        delay_cycles(CPUCLK_FREQ / 1000U);
    }
    k230_poll_uart();
}

void Motor_SetTargetRPM(int32_t leftRpm, int32_t rightRpm)
{
    pid_set_target(&gLeftPid, leftRpm);
    pid_set_target(&gRightPid, rightRpm);
}

void Motor_Stop(void)
{
    pid_set_target(&gLeftPid, 0);
    pid_set_target(&gRightPid, 0);
    gLeftPid.targetRpm = 0;
    gRightPid.targetRpm = 0;
    gLeftPid.errorSum = 0;
    gRightPid.errorSum = 0;
    gLeftPid.lastError = 0;
    gRightPid.lastError = 0;
    motor_set_left(0);
    motor_set_right(0);
}

static void motor_set_left(int16_t dutyPercent)
{
    int16_t duty = clamp_duty((int32_t) dutyPercent * LEFT_MOTOR_DIRECTION);

    if (duty > 0) {
        gpio_write(LEFT_IN1_PORT, LEFT_IN1_PIN, true);
        gpio_write(LEFT_IN2_PORT, LEFT_IN2_PIN, false);
        motor_set_pwm(LEFT_PWM_INDEX, (uint8_t) duty);
    } else if (duty < 0) {
        gpio_write(LEFT_IN1_PORT, LEFT_IN1_PIN, false);
        gpio_write(LEFT_IN2_PORT, LEFT_IN2_PIN, true);
        motor_set_pwm(LEFT_PWM_INDEX, (uint8_t) (-duty));
    } else {
        gpio_write(LEFT_IN1_PORT, LEFT_IN1_PIN, false);
        gpio_write(LEFT_IN2_PORT, LEFT_IN2_PIN, false);
        motor_set_pwm(LEFT_PWM_INDEX, 0U);
    }
}

static void motor_set_right(int16_t dutyPercent)
{
    int16_t duty = clamp_duty((int32_t) dutyPercent * RIGHT_MOTOR_DIRECTION);

    if (duty > 0) {
        gpio_write(RIGHT_IN1_PORT, RIGHT_IN1_PIN, true);
        gpio_write(RIGHT_IN2_PORT, RIGHT_IN2_PIN, false);
        motor_set_pwm(RIGHT_PWM_INDEX, (uint8_t) duty);
    } else if (duty < 0) {
        gpio_write(RIGHT_IN1_PORT, RIGHT_IN1_PIN, false);
        gpio_write(RIGHT_IN2_PORT, RIGHT_IN2_PIN, true);
        motor_set_pwm(RIGHT_PWM_INDEX, (uint8_t) (-duty));
    } else {
        gpio_write(RIGHT_IN1_PORT, RIGHT_IN1_PIN, false);
        gpio_write(RIGHT_IN2_PORT, RIGHT_IN2_PIN, false);
        motor_set_pwm(RIGHT_PWM_INDEX, 0U);
    }
}

static void motor_set_pwm(uint32_t ccIndex, uint8_t dutyPercent)
{
    uint32_t activeCounts;
    uint32_t compare;

    if (dutyPercent > 100U) {
        dutyPercent = 100U;
    }

    activeCounts = ((uint32_t) dutyPercent * PWM_PERIOD_COUNTS) / 100U;
    compare = PWM_PERIOD_COUNTS - activeCounts;
    DL_TimerG_setCaptureCompareValue(MOTOR_PWM_INST, compare, ccIndex);
}

static void pid_set_target(MotorPidController *pid, int32_t targetRpm)
{
    pid->finalTargetRpm = clamp_i32(targetRpm, -VISION_MAX_RPM, VISION_MAX_RPM);
    if (pid->finalTargetRpm == 0) {
        pid->errorSum = 0;
        pid->lastError = 0;
    }
}

static int16_t pid_update(MotorPidController *pid, int32_t measuredRpm)
{
    int32_t error;
    int32_t derivative;
    int32_t duty;

    pid_apply_target_ramp(pid);
    pid->measuredRpm = measuredRpm;

    if ((pid->targetRpm == 0) && (pid->finalTargetRpm == 0)) {
        pid->errorSum = 0;
        pid->lastError = 0;
        pid->dutyPercent = 0;
        return 0;
    }

    error = pid->targetRpm - measuredRpm;
    pid->errorSum = clamp_i32(pid->errorSum + error, -PID_INTEGRAL_LIMIT, PID_INTEGRAL_LIMIT);
    derivative = error - pid->lastError;
    pid->lastError = error;

    duty = pid_feedforward_duty(pid->targetRpm);
    duty += ((PID_KP * error) + (PID_KI * pid->errorSum) + (PID_KD * derivative)) / PID_SCALE;
    pid->dutyPercent = clamp_duty(duty);
    return pid->dutyPercent;
}

static void pid_apply_target_ramp(MotorPidController *pid)
{
    if (pid->targetRpm < pid->finalTargetRpm) {
        pid->targetRpm += TARGET_RAMP_RPM_PER_SAMPLE;
        if (pid->targetRpm > pid->finalTargetRpm) {
            pid->targetRpm = pid->finalTargetRpm;
        }
    } else if (pid->targetRpm > pid->finalTargetRpm) {
        pid->targetRpm -= TARGET_RAMP_RPM_PER_SAMPLE;
        if (pid->targetRpm < pid->finalTargetRpm) {
            pid->targetRpm = pid->finalTargetRpm;
        }
    }
}

static int16_t pid_feedforward_duty(int32_t targetRpm)
{
    int32_t duty;

    if (targetRpm > 0) {
        duty = (DEFAULT_FEEDFORWARD_DUTY_PERCENT * targetRpm) / TRACK_BASE_RPM;
        return (int16_t) clamp_i32(duty, 0, DEFAULT_FEEDFORWARD_DUTY_PERCENT);
    }
    if (targetRpm < 0) {
        duty = (DEFAULT_FEEDFORWARD_DUTY_PERCENT * (-targetRpm)) / TRACK_BASE_RPM;
        return (int16_t) -clamp_i32(duty, 0, DEFAULT_FEEDFORWARD_DUTY_PERCENT);
    }
    return 0;
}

static int16_t clamp_duty(int32_t dutyPercent)
{
    return (int16_t) clamp_i32(dutyPercent, -DUTY_LIMIT_PERCENT, DUTY_LIMIT_PERCENT);
}

static int32_t clamp_i32(int32_t value, int32_t minValue, int32_t maxValue)
{
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static int32_t rpm_from_encoder_delta(int32_t delta)
{
    int64_t rpm = ((int64_t) delta * 60000LL) /
                  ((int64_t) ENCODER_COUNTS_PER_REV * SPEED_SAMPLE_MS);

    if (rpm > MEASURED_RPM_LIMIT) {
        return MEASURED_RPM_LIMIT;
    }
    if (rpm < -MEASURED_RPM_LIMIT) {
        return -MEASURED_RPM_LIMIT;
    }
    return (int32_t) rpm;
}

static uint8_t track_read_sensor_mask(void)
{
    static GPIO_Regs *const ports[TRACK_SENSOR_COUNT] = {
        GPIOA, GPIOA, GPIOB, GPIOB,
        GPIOB, GPIOB, GPIOB, GPIOA
    };
    static const uint32_t pins[TRACK_SENSOR_COUNT] = {
        TRACK_GPIO_TRACK_1_PIN, TRACK_GPIO_TRACK_2_PIN, TRACK_GPIO_TRACK_3_PIN, TRACK_GPIO_TRACK_4_PIN,
        TRACK_GPIO_TRACK_5_PIN, TRACK_GPIO_TRACK_6_PIN, TRACK_GPIO_TRACK_7_PIN, TRACK_GPIO_TRACK_8_PIN
    };
    uint8_t mask = 0U;
    uint8_t rawMask = 0U;

    for (uint8_t i = 0U; i < TRACK_SENSOR_COUNT; i++) {
        bool rawHigh = (DL_GPIO_readPins(ports[i], pins[i]) != 0U);
        bool active = rawHigh;

        if (rawHigh) {
            rawMask |= (uint8_t) (1U << i);
        }

        if (TRACK_SENSOR_ACTIVE_LOW != 0U) {
            active = !active;
        }
        if (active) {
            mask |= (uint8_t) (1U << i);
        }
    }
    gTrackRawValue = (int16_t) rawMask;
    return mask;
}

static int32_t track_count_active_sensors(uint8_t sensorMask, uint8_t startBit, uint8_t endBit)
{
    int32_t count = 0;

    for (uint8_t i = startBit; i <= endBit; i++) {
        if ((sensorMask & (1U << i)) != 0U) {
            count++;
        }
    }
    return count;
}

static void track_update_target(void)
{
    uint8_t sensorMask;
    int32_t leftCount;
    int32_t rightCount;
    int32_t weightedSum;
    int32_t deviation;
    int32_t turn;
    int32_t leftTarget;
    int32_t rightTarget;

    sensorMask = track_read_sensor_mask();
    gVisionDataValid = true;
    gVisionTrackValue = (int16_t) sensorMask;

    if (sensorMask == 0U) {
        gVisionLineOk = false;
        gVisionDeviation = 0;
        gTrackLastError = 0;
        gTrackLeftCount = 0;
        gTrackRightCount = 0;
        Motor_SetTargetRPM(TRACK_BASE_RPM, TRACK_BASE_RPM);
        return;
    }

    gVisionLineOk = true;
    gVisionLastRxMs = gSystemMs;

    leftCount = track_count_active_sensors(sensorMask, 0U, 3U);
    rightCount = track_count_active_sensors(sensorMask, 4U, 7U);
    activeCount = leftCount + rightCount;
    gTrackLeftCount = (int16_t) leftCount;
    gTrackRightCount = (int16_t) rightCount;

    weightedSum = 0;
    if ((sensorMask & (1U << 0)) != 0U) { weightedSum -= 7; }
    if ((sensorMask & (1U << 1)) != 0U) { weightedSum -= 5; }
    if ((sensorMask & (1U << 2)) != 0U) { weightedSum -= 3; }
    if ((sensorMask & (1U << 3)) != 0U) { weightedSum -= 1; }
    if ((sensorMask & (1U << 4)) != 0U) { weightedSum += 1; }
    if ((sensorMask & (1U << 5)) != 0U) { weightedSum += 3; }
    if ((sensorMask & (1U << 6)) != 0U) { weightedSum += 5; }
    if ((sensorMask & (1U << 7)) != 0U) { weightedSum += 7; }

    deviation = weightedSum;
    if ((sensorMask & ((1U << 3) | (1U << 4))) != 0U) {
        deviation = 0;
    }
    gTrackLastError = (int16_t) deviation;
    gVisionDeviation = (int16_t) deviation;

    if ((deviation >= -TRACK_CENTER_DEADBAND) && (deviation <= TRACK_CENTER_DEADBAND)) {
        deviation = 0;
    }

    turn = deviation * TRACK_TURN_GAIN;
    turn = clamp_i32(turn, -TRACK_TURN_LIMIT, TRACK_TURN_LIMIT);
    leftTarget = TRACK_BASE_RPM + turn;
    rightTarget = TRACK_BASE_RPM - turn;

    leftTarget = clamp_i32(leftTarget, 0, TRACK_MAX_RPM);
    rightTarget = clamp_i32(rightTarget, 0, TRACK_MAX_RPM);
    Motor_SetTargetRPM(leftTarget, rightTarget);
}

static void k230_parse_byte(uint8_t data)
{
    static uint8_t state;
    static uint8_t cmd;
    static uint8_t dh;
    static uint8_t dl;
    static uint8_t checksum;

    switch (state) {
        case 0:
            if (data == K230_FRAME_HEAD) {
                gK230HeadCount++;
                state = 1;
            }
            break;
        case 1:
            cmd = data;
            state = 2;
            break;
        case 2:
            dh = data;
            state = 3;
            break;
        case 3:
            dl = data;
            state = 4;
            break;
        case 4:
            checksum = data;
            state = 5;
            break;
        case 5:
            if ((data == K230_FRAME_TAIL) && (((uint8_t) (cmd + dh + dl)) == checksum)) {
                gK230FrameCount++;
                gK230LastCmd = cmd;
                k230_handle_packet(cmd, dh, dl);
            } else {
                gK230BadFrameCount++;
            }
            state = 0;
            break;
        default:
            state = 0;
            break;
    }
}

static void k230_handle_packet(uint8_t cmd, uint8_t dh, uint8_t dl)
{
    /* CMD 0x01=球坐标 → 转发给步进板 */
    if (cmd == STEPPER_CMD_BALL) {
        stepper_send_frame(STEPPER_CMD_BALL, dh, dl);
    }
    (void) cmd; (void) dh; (void) dl;
}

static uint8_t encoder_read_left_state(void)
{
    uint8_t state = 0U;

    if (DL_GPIO_readPins(ENC_PORT, ENC_LEFT_A_PIN)) {
        state |= 0x02U;
    }
    if (DL_GPIO_readPins(ENC_PORT, ENC_LEFT_B_PIN)) {
        state |= 0x01U;
    }
    return state;
}

static uint8_t encoder_read_right_state(void)
{
    uint8_t state = 0U;

    if (DL_GPIO_readPins(ENC_PORT, ENC_RIGHT_A_PIN)) {
        state |= 0x02U;
    }
    if (DL_GPIO_readPins(ENC_PORT, ENC_RIGHT_B_PIN)) {
        state |= 0x01U;
    }
    return state;
}

static void encoder_update(void)
{
    static const int8_t stepTable[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0
    };
    uint8_t newLeft = encoder_read_left_state();
    uint8_t newRight = encoder_read_right_state();

    gLeftEncoderCount += LEFT_ENCODER_DIRECTION * stepTable[(gLeftEncoderState << 2) | newLeft];
    gRightEncoderCount += RIGHT_ENCODER_DIRECTION * stepTable[(gRightEncoderState << 2) | newRight];
    gLeftEncoderState = newLeft;
    gRightEncoderState = newRight;
}

static void oled_init(void)
{
    gpio_write(OLED_CS_PORT, OLED_CS_PIN, true);
    gpio_write(OLED_RST_PORT, OLED_RST_PIN, false);
    delay_cycles(CPUCLK_FREQ / 100U);
    gpio_write(OLED_RST_PORT, OLED_RST_PIN, true);
    delay_cycles(CPUCLK_FREQ / 100U);

    oled_write_command(0xAE);
    oled_write_command(0x20);
    oled_write_command(0x00);
    oled_write_command(0xB0);
    oled_write_command(0xC8);
    oled_write_command(0x00);
    oled_write_command(0x10);
    oled_write_command(0x40);
    oled_write_command(0x81);
    oled_write_command(0x7F);
    oled_write_command(0xA1);
    oled_write_command(0xA6);
    oled_write_command(0xA8);
    oled_write_command(0x3F);
    oled_write_command(0xA4);
    oled_write_command(0xD3);
    oled_write_command(0x00);
    oled_write_command(0xD5);
    oled_write_command(0x80);
    oled_write_command(0xD9);
    oled_write_command(0xF1);
    oled_write_command(0xDA);
    oled_write_command(0x12);
    oled_write_command(0xDB);
    oled_write_command(0x40);
    oled_write_command(0x8D);
    oled_write_command(0x14);
    oled_write_command(0xAF);
}

static void oled_clear(void)
{
    for (uint16_t i = 0; i < sizeof(gOledBuffer); i++) {
        gOledBuffer[i] = 0;
    }
}

static void oled_refresh(void)
{
    for (uint8_t page = 0; page < 8U; page++) {
        oled_write_command((uint8_t) (0xB0U + page));
        oled_write_command(0x00);
        oled_write_command(0x10);
        for (uint8_t col = 0; col < 128U; col++) {
            oled_write_data(gOledBuffer[(page * 128U) + col]);
        }
    }
}

static void oled_show_string(uint8_t x, uint8_t page, const char *text)
{
    while ((*text != '\0') && (x < 123U)) {
        oled_draw_char(x, page, *text);
        x += 6U;
        text++;
    }
}

static void oled_show_signed_number(uint8_t x, uint8_t page, int32_t value)
{
    char text[12];
    uint32_t magnitude;
    uint8_t i = 0U;
    uint8_t start;

    if (value < 0) {
        text[i++] = '-';
        magnitude = (uint32_t) (-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t) value;
    }

    start = i;
    do {
        text[i++] = (char) ('0' + (magnitude % 10U));
        magnitude /= 10U;
    } while ((magnitude > 0U) && (i < (sizeof(text) - 1U)));

    for (uint8_t left = start, right = (uint8_t) (i - 1U); left < right; left++, right--) {
        char tmp = text[left];
        text[left] = text[right];
        text[right] = tmp;
    }
    text[i] = '\0';
    oled_show_string(x, page, text);
}

static void oled_draw_char(uint8_t x, uint8_t page, char ch)
{
    uint8_t glyph[5] = {0, 0, 0, 0, 0};

    switch (ch) {
        case '0': glyph[0]=0x3E; glyph[1]=0x51; glyph[2]=0x49; glyph[3]=0x45; glyph[4]=0x3E; break;
        case '1': glyph[0]=0x00; glyph[1]=0x42; glyph[2]=0x7F; glyph[3]=0x40; glyph[4]=0x00; break;
        case '2': glyph[0]=0x42; glyph[1]=0x61; glyph[2]=0x51; glyph[3]=0x49; glyph[4]=0x46; break;
        case '3': glyph[0]=0x21; glyph[1]=0x41; glyph[2]=0x45; glyph[3]=0x4B; glyph[4]=0x31; break;
        case '4': glyph[0]=0x18; glyph[1]=0x14; glyph[2]=0x12; glyph[3]=0x7F; glyph[4]=0x10; break;
        case '5': glyph[0]=0x27; glyph[1]=0x45; glyph[2]=0x45; glyph[3]=0x45; glyph[4]=0x39; break;
        case '6': glyph[0]=0x3C; glyph[1]=0x4A; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x30; break;
        case '7': glyph[0]=0x01; glyph[1]=0x71; glyph[2]=0x09; glyph[3]=0x05; glyph[4]=0x03; break;
        case '8': glyph[0]=0x36; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x36; break;
        case '9': glyph[0]=0x06; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x29; glyph[4]=0x1E; break;
        case 'D': glyph[0]=0x7F; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x22; glyph[4]=0x1C; break;
        case 'E': glyph[0]=0x7F; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x41; break;
        case 'F': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x09; glyph[3]=0x09; glyph[4]=0x01; break;
        case 'G': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x7A; break;
        case 'H': glyph[0]=0x7F; glyph[1]=0x08; glyph[2]=0x08; glyph[3]=0x08; glyph[4]=0x7F; break;
        case 'I': glyph[0]=0x00; glyph[1]=0x41; glyph[2]=0x7F; glyph[3]=0x41; glyph[4]=0x00; break;
        case 'L': glyph[0]=0x7F; glyph[1]=0x40; glyph[2]=0x40; glyph[3]=0x40; glyph[4]=0x40; break;
        case 'M': glyph[0]=0x7F; glyph[1]=0x02; glyph[2]=0x0C; glyph[3]=0x02; glyph[4]=0x7F; break;
        case 'P': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x09; glyph[3]=0x09; glyph[4]=0x06; break;
        case 'R': glyph[0]=0x7F; glyph[1]=0x09; glyph[2]=0x19; glyph[3]=0x29; glyph[4]=0x46; break;
        case 'S': glyph[0]=0x46; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x31; break;
        case 'T': glyph[0]=0x01; glyph[1]=0x01; glyph[2]=0x7F; glyph[3]=0x01; glyph[4]=0x01; break;
        case 'U': glyph[0]=0x3F; glyph[1]=0x40; glyph[2]=0x40; glyph[3]=0x40; glyph[4]=0x3F; break;
        case 'Y': glyph[0]=0x07; glyph[1]=0x08; glyph[2]=0x70; glyph[3]=0x08; glyph[4]=0x07; break;
        case 'A': glyph[0]=0x7E; glyph[1]=0x11; glyph[2]=0x11; glyph[3]=0x11; glyph[4]=0x7E; break;
        case 'B': glyph[0]=0x7F; glyph[1]=0x49; glyph[2]=0x49; glyph[3]=0x49; glyph[4]=0x36; break;
        case 'C': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x41; glyph[4]=0x22; break;
        case 'K': glyph[0]=0x7F; glyph[1]=0x08; glyph[2]=0x14; glyph[3]=0x22; glyph[4]=0x41; break;
        case 'N': glyph[0]=0x7F; glyph[1]=0x04; glyph[2]=0x08; glyph[3]=0x10; glyph[4]=0x7F; break;
        case 'O': glyph[0]=0x3E; glyph[1]=0x41; glyph[2]=0x41; glyph[3]=0x41; glyph[4]=0x3E; break;
        case 'V': glyph[0]=0x1F; glyph[1]=0x20; glyph[2]=0x40; glyph[3]=0x20; glyph[4]=0x1F; break;
        case 'W': glyph[0]=0x7F; glyph[1]=0x20; glyph[2]=0x18; glyph[3]=0x20; glyph[4]=0x7F; break;
        case 'X': glyph[0]=0x63; glyph[1]=0x14; glyph[2]=0x08; glyph[3]=0x14; glyph[4]=0x63; break;
        case ':': glyph[0]=0x00; glyph[1]=0x36; glyph[2]=0x36; glyph[3]=0x00; glyph[4]=0x00; break;
        case '-': glyph[0]=0x08; glyph[1]=0x08; glyph[2]=0x08; glyph[3]=0x08; glyph[4]=0x08; break;
        case '%': glyph[0]=0x23; glyph[1]=0x13; glyph[2]=0x08; glyph[3]=0x64; glyph[4]=0x62; break;
        default: break;
    }

    if (page < 8U) {
        for (uint8_t i = 0; i < 5U; i++) {
            if ((x + i) < 128U) {
                gOledBuffer[(page * 128U) + x + i] = glyph[i];
            }
        }
    }
}

static void oled_write_command(uint8_t command)
{
    gpio_write(OLED_DC_PORT, OLED_DC_PIN, false);
    gpio_write(OLED_CS_PORT, OLED_CS_PIN, false);
    oled_write_byte(command);
    gpio_write(OLED_CS_PORT, OLED_CS_PIN, true);
}

static void oled_write_data(uint8_t data)
{
    gpio_write(OLED_DC_PORT, OLED_DC_PIN, true);
    gpio_write(OLED_CS_PORT, OLED_CS_PIN, false);
    oled_write_byte(data);
    gpio_write(OLED_CS_PORT, OLED_CS_PIN, true);
}

static void oled_write_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8U; i++) {
        gpio_write(OLED_SCLK_PORT, OLED_SCLK_PIN, false);
        gpio_write(OLED_MOSI_PORT, OLED_MOSI_PIN, ((data & 0x80U) != 0U));
        gpio_write(OLED_SCLK_PORT, OLED_SCLK_PIN, true);
        data <<= 1;
    }
}

static void user_led1_init_on(void)
{
    DL_GPIO_initDigitalOutput(USER_LED1_IOMUX);
    DL_GPIO_setPins(USER_LED1_PORT, USER_LED1_PIN);
    DL_GPIO_enableOutput(USER_LED1_PORT, USER_LED1_PIN);
}

static void gpio_write(GPIO_Regs *port, uint32_t pin, bool high)
{
    if (high) {
        DL_GPIO_setPins(port, pin);
    } else {
        DL_GPIO_clearPins(port, pin);
    }
}

/* ==================== 步进板 UART 通信 ==================== */
/*
 * UART1: PA8=TX, PA9=RX → 发给步进板
 * CMD 0x01: ball_x_mm (int16, 由k230_handle_packet转发)
 * CMD 0x05: track_type(1B) + speed_cms(1B)
 */

/*
 * 步进板 UART 通信 — UART1: PA8=TX, PA9=RX
 * 需要在 SysConfig 中启用 UART1 外设。
 * 暂时用 GPIO 模拟 TX 占位; SysConfig 配好后删掉 GPIO 部分改用 UART HW 发送。
 */
static void stepper_uart_init(void)
{
    /* PA8 初始化为 GPIO 输出位 */
    DL_GPIO_initDigitalOutput(IOMUX_PINCM19);  /* PA8 */
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_8);
}

static void stepper_send_frame(uint8_t cmd, uint8_t dh, uint8_t dl)
{
    /* SysConfig 未配 UART1 硬件, 暂用 PA8 输出 GPIO 代替 (不影响调 PID) */
    (void)cmd; (void)dh; (void)dl;
}

static void stepper_send_car_status(int32_t leftRpm, int32_t rightRpm)
{
    /* SysConfig 未配 UART1 硬件, 暂不发送车速轨道数据 */
    (void)leftRpm; (void)rightRpm;
}



