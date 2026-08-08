/* USER CODE BEGIN Header */
/*
 * ================================================================
 *        NEXUS EV - STM32F446FE MOTOR CONTROL ECU
 * ================================================================
 *
 * Functions:
 *
 *  - Receive CAN messages from ESP32
 *  - Decode battery / temperature / encoder telemetry
 *  - Receive motor control commands
 *  - Generate PWM for TB6612FNG
 *  - Control motor direction
 *  - Apply thermal protection
 *  - Emergency motor shutdown
 *
 * CAN:
 *      500 kbps
 *
 * CAN IDs:
 *
 *      0x100 -> Battery voltage/current
 *      0x101 -> Temperature/protection
 *      0x102 -> RPM/speed/frequency
 *      0x200 -> Motor command
 *
 * Motor command:
 *
 *      Byte 0   = Direction
 *                 0 = STOP
 *                 1 = FORWARD
 *                 2 = BACKWARD
 *                 3 = BRAKE
 *
 *      Byte 1   = PWM 0-255
 *
 *      Byte 2-3 = PWM carrier frequency in Hz
 *
 * ================================================================
 */

/* Includes -------------------------------------------------------*/

#include "main.h"
#include "can.h"
#include "tim.h"
#include "gpio.h"

#include <string.h>
#include <stdint.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */


/* ================================================================
 * CAN CONFIGURATION
 * ================================================================ */

#define CAN_ID_BATTERY       0x100
#define CAN_ID_TEMPERATURE   0x101
#define CAN_ID_MOTOR_DATA    0x102
#define CAN_ID_MOTOR_COMMAND 0x200


/* ================================================================
 * THERMAL PROTECTION
 * ================================================================ */

/*
 * Matches the current ESP32 firmware:
 *
 * Temperature >= 60°C
 *      -> PWM capped to 150
 *
 * Temperature < 55°C
 *      -> Protection cleared
 *
 * 5°C hysteresis
 */

#define TEMP_OVERHEAT_THRESHOLD  60.0f
#define TEMP_CLEAR_THRESHOLD     55.0f

#define TEMP_PROTECTION_PWM      150


/* ================================================================
 * PWM CONFIGURATION
 * ================================================================ */

#define PWM_MAX                  255


/* ================================================================
 * MOTOR DIRECTION
 * ================================================================ */

#define MOTOR_STOP               0
#define MOTOR_FORWARD            1
#define MOTOR_BACKWARD           2
#define MOTOR_BRAKE              3


/* ================================================================
 * CAN DATA STRUCTURE
 * ================================================================ */

typedef struct
{
    uint8_t direction;

    uint8_t pwm;

    uint16_t frequency;

} MotorCommand_t;


/* ================================================================
 * GLOBAL VARIABLES
 * ================================================================ */

CAN_RxHeaderTypeDef RxHeader;

uint8_t RxData[8];

MotorCommand_t motorCommand;


/* Battery data */

float batteryVoltage = 0.0f;
float batteryCurrent = 0.0f;


/* Temperature */

float batteryTemperature = 0.0f;

uint8_t overTemperature = 0;


/* Motor feedback */

uint16_t motorRPM = 0;

float motorSpeed = 0.0f;

float motorFrequency = 0.0f;

uint8_t motorDirectionFeedback = 0;


/* CAN status */

volatile uint8_t canMessageReceived = 0;


/* ================================================================
 * FUNCTION PROTOTYPES
 * ================================================================ */

void CAN_Filter_Config(void);

void CAN_Process_Message(
    CAN_RxHeaderTypeDef *header,
    uint8_t *data
);

void Process_Battery_Data(uint8_t *data);

void Process_Temperature_Data(uint8_t *data);

void Process_Motor_Data(uint8_t *data);

void Process_Motor_Command(uint8_t *data);

void Motor_Set_PWM(uint8_t pwm);

void Motor_Forward(uint8_t pwm);

void Motor_Backward(uint8_t pwm);

void Motor_Brake(void);

void Motor_Stop(void);

void Motor_Emergency_Stop(void);

void Thermal_Protection(void);


/* ================================================================
 * CAN FILTER
 * ================================================================ */

void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef filterConfig;

    /*
     * Accept all standard CAN IDs.
     *
     * The software checks the CAN ID.
     *
     * This makes debugging easier.
     */

    filterConfig.FilterBank = 0;

    filterConfig.FilterMode = CAN_FILTERMODE_IDMASK;

    filterConfig.FilterScale = CAN_FILTERSCALE_32BIT;

    filterConfig.FilterIdHigh = 0x0000;

    filterConfig.FilterIdLow = 0x0000;

    filterConfig.FilterMaskIdHigh = 0x0000;

    filterConfig.FilterMaskIdLow = 0x0000;

    filterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;

    filterConfig.FilterActivation = ENABLE;

    filterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &filterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ================================================================
 * CAN START
 * ================================================================ */

void CAN_Start(void)
{
    CAN_Filter_Config();

    /*
     * Start CAN peripheral
     */

    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Enable FIFO0 message pending interrupt
     */

    if (HAL_CAN_ActivateNotification(
            &hcan1,
            CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ================================================================
 * CAN RECEIVE CALLBACK
 * ================================================================ */

void HAL_CAN_RxFifo0MsgPendingCallback(
    CAN_HandleTypeDef *hcan
)
{
    if (hcan->Instance == CAN1)
    {
        /*
         * Read CAN message
         */

        if (HAL_CAN_GetRxMessage(
                hcan,
                CAN_RX_FIFO0,
                &RxHeader,
                RxData) == HAL_OK)
        {
            canMessageReceived = 1;

            CAN_Process_Message(
                &RxHeader,
                RxData
            );
        }
    }
}


/* ================================================================
 * CAN MESSAGE PROCESSOR
 * ================================================================ */

void CAN_Process_Message(
    CAN_RxHeaderTypeDef *header,
    uint8_t *data
)
{
    /*
     * Only process standard CAN frames
     */

    if (header->IDE != CAN_ID_STD)
    {
        return;
    }


    switch (header->StdId)
    {
        case CAN_ID_BATTERY:

            Process_Battery_Data(data);

            break;


        case CAN_ID_TEMPERATURE:

            Process_Temperature_Data(data);

            break;


        case CAN_ID_MOTOR_DATA:

            Process_Motor_Data(data);

            break;


        case CAN_ID_MOTOR_COMMAND:

            Process_Motor_Command(data);

            break;


        default:

            /*
             * Unknown CAN ID
             */

            break;
    }
}


/* ================================================================
 * BATTERY CAN MESSAGE
 *
 * 0x100
 *
 * Byte 0-1 = Voltage x100
 * Byte 2-3 = Current x100
 * ================================================================ */

void Process_Battery_Data(uint8_t *data)
{
    uint16_t voltageRaw;

    int16_t currentRaw;


    voltageRaw =
        ((uint16_t)data[0] << 8) |
        data[1];


    currentRaw =
        ((int16_t)data[2] << 8) |
        data[3];


    batteryVoltage =
        voltageRaw / 100.0f;


    batteryCurrent =
        currentRaw / 100.0f;
}


/* ================================================================
 * TEMPERATURE CAN MESSAGE
 *
 * 0x101
 *
 * Byte 0-1 = Temperature x100
 * Byte 2   = Overheat flag
 * Byte 3   = DS18B20 detected
 * ================================================================ */

void Process_Temperature_Data(uint8_t *data)
{
    int16_t temperatureRaw;


    temperatureRaw =
        ((int16_t)data[0] << 8) |
        data[1];


    batteryTemperature =
        temperatureRaw / 100.0f;


    overTemperature = data[2];


    /*
     * Run thermal protection immediately.
     */

    Thermal_Protection();
}


/* ================================================================
 * MOTOR FEEDBACK MESSAGE
 *
 * 0x102
 *
 * Byte 0-1 = RPM
 * Byte 2-3 = Speed x100
 * Byte 4-5 = Frequency x10
 * Byte 6   = Direction
 * ================================================================ */

void Process_Motor_Data(uint8_t *data)
{
    uint16_t rpmRaw;

    uint16_t speedRaw;

    uint16_t frequencyRaw;


    rpmRaw =
        ((uint16_t)data[0] << 8) |
        data[1];


    speedRaw =
        ((uint16_t)data[2] << 8) |
        data[3];


    frequencyRaw =
        ((uint16_t)data[4] << 8) |
        data[5];


    motorRPM = rpmRaw;


    motorSpeed =
        speedRaw / 100.0f;


    motorFrequency =
        frequencyRaw / 10.0f;


    motorDirectionFeedback =
        data[6];
}


/* ================================================================
 * MOTOR COMMAND MESSAGE
 *
 * 0x200
 *
 * Byte 0 = Direction
 * Byte 1 = PWM
 * Byte 2-3 = Frequency
 * ================================================================ */

void Process_Motor_Command(uint8_t *data)
{
    uint16_t requestedFrequency;


    motorCommand.direction = data[0];


    motorCommand.pwm =
        data[1];


    requestedFrequency =
        ((uint16_t)data[2] << 8) |
        data[3];


    motorCommand.frequency =
        requestedFrequency;


    /*
     * Check thermal condition before
     * applying the command.
     */

    if (overTemperature)
    {
        if (motorCommand.pwm >
            TEMP_PROTECTION_PWM)
        {
            motorCommand.pwm =
                TEMP_PROTECTION_PWM;
        }
    }


    /*
     * Execute motor command.
     */

    switch (motorCommand.direction)
    {
        case MOTOR_FORWARD:

            Motor_Forward(
                motorCommand.pwm
            );

            break;


        case MOTOR_BACKWARD:

            Motor_Backward(
                motorCommand.pwm
            );

            break;


        case MOTOR_BRAKE:

            Motor_Brake();

            break;


        case MOTOR_STOP:

        default:

            Motor_Stop();

            break;
    }
}


/* ================================================================
 * SET PWM
 * ================================================================ */

void Motor_Set_PWM(uint8_t pwm)
{
    /*
     * Convert 0-255 command to
     * timer compare value.
     *
     * This assumes TIM3 ARR = 255.
     */

    if (pwm > PWM_MAX)
    {
        pwm = PWM_MAX;
    }


    __HAL_TIM_SET_COMPARE(
        &htim3,
        TIM_CHANNEL_1,
        pwm
    );


    __HAL_TIM_SET_COMPARE(
        &htim3,
        TIM_CHANNEL_2,
        pwm
    );
}


/* ================================================================
 * MOTOR FORWARD
 * ================================================================ */

void Motor_Forward(uint8_t pwm)
{
    /*
     * TB6612FNG:
     *
     * AIN1 = HIGH
     * AIN2 = LOW
     *
     * BIN1 = LOW
     * BIN2 = HIGH
     *
     * This assumes the second motor
     * is physically reversed.
     */


    HAL_GPIO_WritePin(
        AIN1_GPIO_Port,
        AIN1_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        AIN2_GPIO_Port,
        AIN2_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN1_GPIO_Port,
        BIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN2_GPIO_Port,
        BIN2_Pin,
        GPIO_PIN_SET
    );


    /*
     * Enable TB6612FNG
     */

    HAL_GPIO_WritePin(
        STBY_GPIO_Port,
        STBY_Pin,
        GPIO_PIN_SET
    );


    Motor_Set_PWM(pwm);
}


/* ================================================================
 * MOTOR BACKWARD
 * ================================================================ */

void Motor_Backward(uint8_t pwm)
{
    HAL_GPIO_WritePin(
        AIN1_GPIO_Port,
        AIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        AIN2_GPIO_Port,
        AIN2_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        BIN1_GPIO_Port,
        BIN1_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        BIN2_GPIO_Port,
        BIN2_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        STBY_GPIO_Port,
        STBY_Pin,
        GPIO_PIN_SET
    );


    Motor_Set_PWM(pwm);
}


/* ================================================================
 * MOTOR BRAKE
 * ================================================================ */

void Motor_Brake(void)
{
    /*
     * TB6612FNG short brake:
     *
     * IN1 = HIGH
     * IN2 = HIGH
     */

    HAL_GPIO_WritePin(
        AIN1_GPIO_Port,
        AIN1_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        AIN2_GPIO_Port,
        AIN2_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        BIN1_GPIO_Port,
        BIN1_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        BIN2_GPIO_Port,
        BIN2_Pin,
        GPIO_PIN_SET
    );


    HAL_GPIO_WritePin(
        STBY_GPIO_Port,
        STBY_Pin,
        GPIO_PIN_SET
    );


    Motor_Set_PWM(255);
}


/* ================================================================
 * MOTOR STOP
 * ================================================================ */

void Motor_Stop(void)
{
    /*
     * Set PWM to zero first.
     */

    Motor_Set_PWM(0);


    HAL_GPIO_WritePin(
        AIN1_GPIO_Port,
        AIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        AIN2_GPIO_Port,
        AIN2_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN1_GPIO_Port,
        BIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN2_GPIO_Port,
        BIN2_Pin,
        GPIO_PIN_RESET
    );


    /*
     * Disable TB6612FNG
     */

    HAL_GPIO_WritePin(
        STBY_GPIO_Port,
        STBY_Pin,
        GPIO_PIN_RESET
    );
}


/* ================================================================
 * EMERGENCY STOP
 * ================================================================ */

void Motor_Emergency_Stop(void)
{
    /*
     * Immediately remove PWM.
     */

    Motor_Set_PWM(0);


    /*
     * Disable motor driver.
     */

    HAL_GPIO_WritePin(
        STBY_GPIO_Port,
        STBY_Pin,
        GPIO_PIN_RESET
    );


    /*
     * Force all direction pins LOW.
     */

    HAL_GPIO_WritePin(
        AIN1_GPIO_Port,
        AIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        AIN2_GPIO_Port,
        AIN2_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN1_GPIO_Port,
        BIN1_Pin,
        GPIO_PIN_RESET
    );


    HAL_GPIO_WritePin(
        BIN2_GPIO_Port,
        BIN2_Pin,
        GPIO_PIN_RESET
    );
}


/* ================================================================
 * THERMAL PROTECTION
 * ================================================================ */

void Thermal_Protection(void)
{
    /*
     * Emergency thermal condition
     */

    if (batteryTemperature >=
        TEMP_OVERHEAT_THRESHOLD)
    {
        overTemperature = 1;


        /*
         * Immediately cap current PWM.
         */

        if (motorCommand.pwm >
            TEMP_PROTECTION_PWM)
        {
            motorCommand.pwm =
                TEMP_PROTECTION_PWM;
        }


        /*
         * If the motor is running,
         * reapply the reduced PWM.
         */

        if (motorCommand.direction ==
            MOTOR_FORWARD)
        {
            Motor_Forward(
                motorCommand.pwm
            );
        }


        else if (motorCommand.direction ==
                 MOTOR_BACKWARD)
        {
            Motor_Backward(
                motorCommand.pwm
            );
        }
    }


    /*
     * Hysteresis:
     *
     * Protection is cleared only when
     * temperature drops below 55°C.
     */

    else if (batteryTemperature <
             TEMP_CLEAR_THRESHOLD)
    {
        overTemperature = 0;
    }
}


/* ================================================================
 * MAIN
 * ================================================================ */

int main(void)
{
    /* MCU Configuration -----------------------------------------*/

    HAL_Init();


    SystemClock_Config();


    /* Initialize all configured peripherals ---------------------*/

    MX_GPIO_Init();

    MX_CAN1_Init();

    MX_TIM3_Init();


    /* USER CODE BEGIN 2 */


    /*
     * Initialize motor state.
     */

    motorCommand.direction =
        MOTOR_STOP;

    motorCommand.pwm =
        0;

    motorCommand.frequency =
        5000;


    /*
     * Start PWM channels.
     */

    HAL_TIM_PWM_Start(
        &htim3,
        TIM_CHANNEL_1
    );


    HAL_TIM_PWM_Start(
        &htim3,
        TIM_CHANNEL_2
    );


    /*
     * Make sure motor is stopped
     * during startup.
     */

    Motor_Stop();


    /*
     * Configure CAN filters and start CAN.
     */

    CAN_Start();


    /* USER CODE END 2 */


    /* Infinite loop ----------------------------------------------*/

    while (1)
    {
        /*
         * Main safety check.
         *
         * CAN callback normally handles
         * temperature immediately, but this
         * provides an additional protection layer.
         */

        Thermal_Protection();


        /*
         * If over-temperature is active,
         * continuously enforce PWM limit.
         */

        if (overTemperature)
        {
            if (motorCommand.pwm >
                TEMP_PROTECTION_PWM)
            {
                motorCommand.pwm =
                    TEMP_PROTECTION_PWM;


                if (motorCommand.direction ==
                    MOTOR_FORWARD)
                {
                    Motor_Forward(
                        motorCommand.pwm
                    );
                }


                else if (
                    motorCommand.direction ==
                    MOTOR_BACKWARD)
                {
                    Motor_Backward(
                        motorCommand.pwm
                    );
                }
            }
        }


        HAL_Delay(10);
    }
}


/* ================================================================
 * END OF FILE
 * ================================================================ */
