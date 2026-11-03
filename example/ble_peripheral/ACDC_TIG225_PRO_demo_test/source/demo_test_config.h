#ifndef _H_DEMO_TEST_CONFIG_H_
#define _H_DEMO_TEST_CONFIG_H_

#define DEMO_TEST_MODEL_NAME                "TIG225 PRO"
#define DEMO_TEST_DEVICE_TYPE               0x0009u
#define DEMO_TEST_UART_BAUD                 57600u

#define DEMO_TEST_MODE_STICK                0u
#define DEMO_TEST_MODE_TIG_DC               1u
#define DEMO_TEST_MODE_AC_SQUARE            2u
#define DEMO_TEST_MODE_AC_TRIANGLE          3u
#define DEMO_TEST_MODE_MIN                  DEMO_TEST_MODE_STICK
#define DEMO_TEST_MODE_MAX                  DEMO_TEST_MODE_AC_TRIANGLE
#define DEMO_TEST_MODE_DEFAULT              DEMO_TEST_MODE_TIG_DC

#define DEMO_TEST_INPUT_VOLTAGE_110V        0u
#define DEMO_TEST_INPUT_VOLTAGE_220V        1u
#define DEMO_TEST_INPUT_VOLTAGE_MIN         DEMO_TEST_INPUT_VOLTAGE_110V
#define DEMO_TEST_INPUT_VOLTAGE_MAX         DEMO_TEST_INPUT_VOLTAGE_220V

#define DEMO_TEST_HOT_START_MIN             0u
#define DEMO_TEST_HOT_START_MAX             100u
#define DEMO_TEST_THRUST_MIN                0u
#define DEMO_TEST_THRUST_MAX                100u
#define DEMO_TEST_VRD_MIN                   0u
#define DEMO_TEST_VRD_MAX                   1u

#define DEMO_TEST_CURRENT_STICK_MIN         20u
#define DEMO_TEST_CURRENT_STICK_MAX_110V    140u
#define DEMO_TEST_CURRENT_STICK_MAX_220V    180u
#define DEMO_TEST_CURRENT_TIG_MIN           10u
#define DEMO_TEST_CURRENT_TIG_MAX_110V      160u
#define DEMO_TEST_CURRENT_TIG_MAX_220V      225u

#define DEMO_TEST_T2T4_SPOT_MIN             0u
#define DEMO_TEST_T2T4_SPOT_MAX             2u
#define DEMO_TEST_T2T4_2T                   0u
#define DEMO_TEST_T2T4_4T                   1u
#define DEMO_TEST_T2T4_SPOT                 2u

#define DEMO_TEST_PULSE_MIN                 0u
#define DEMO_TEST_PULSE_MAX                 1u
#define DEMO_TEST_SPOT_TIME_MIN             10u
#define DEMO_TEST_SPOT_TIME_MAX             150u
#define DEMO_TEST_PRE_GAS_MIN               1u
#define DEMO_TEST_PRE_GAS_MAX               30u
#define DEMO_TEST_UP_SLOPE_MIN              0u
#define DEMO_TEST_UP_SLOPE_MAX              15u
#define DEMO_TEST_PULSE_DUTY_MIN            10u
#define DEMO_TEST_PULSE_DUTY_MAX            90u
#define DEMO_TEST_PULSE_FREQ_MIN            5u
#define DEMO_TEST_PULSE_FREQ_MAX            2000u
#define DEMO_TEST_TRIANGLE_CMD_MIN          100u
#define DEMO_TEST_TRIANGLE_CMD_MAX          4000u
#define DEMO_TEST_PULSE_VALLEY_MIN          5u
#define DEMO_TEST_PULSE_VALLEY_MAX          95u
#define DEMO_TEST_AC_FREQ_MIN               40u
#define DEMO_TEST_AC_FREQ_MAX               200u
#define DEMO_TEST_AC_DUTY_MIN               20u
#define DEMO_TEST_AC_DUTY_MAX               60u
#define DEMO_TEST_DOWN_SLOPE_MIN            0u
#define DEMO_TEST_DOWN_SLOPE_MAX            25u
#define DEMO_TEST_POST_GAS_MIN              0u
#define DEMO_TEST_POST_GAS_MAX              150u
#define DEMO_TEST_ALARM_MIN                 0u
#define DEMO_TEST_ALARM_MAX                 1u

#define DEMO_TEST_AC_FREQ_DEFAULT           50u
#define DEMO_TEST_PULSE_FREQ_DEFAULT        100u

#endif /* _H_DEMO_TEST_CONFIG_H_ */
