/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <microrl.h>
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
// create microrl object and pointer on it
microrl_t rl;
microrl_t * prl = &rl;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* --------------- motor control and encoder input ---------------*/

    typedef struct {
        uint8_t pwm;
        uint8_t direction;
        uint16_t pos;
    } MotorState;

    MotorState motors[6]; // 0:M0A, 1:M0B, 2:M1A, 3:M1B, 4:M2A, 5:M2B

    void motor_control(MotorState* m)
    {
        GPIO_PinState in1_state[4] = {GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET};
        GPIO_PinState in2_state[4] = {GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET};

        // --- Motor 0A (index 0): IN1=A01, IN2=A02, PWM=pwmA0, STBY=STBY0 ---
        HAL_GPIO_WritePin(A01_GPIO_Port, A01_Pin, in1_state[m[0].direction]);
        HAL_GPIO_WritePin(A02_GPIO_Port, A02_Pin, in2_state[m[0].direction]);
        TIM4->CCR4 = m[0].pwm;

        // --- Motor 0B (index 1): IN1=B01, IN2=B02, PWM=pwmB0, STBY=STBY0 ---
        HAL_GPIO_WritePin(B01_GPIO_Port, B01_Pin, in1_state[m[1].direction]);
        HAL_GPIO_WritePin(B02_GPIO_Port, B02_Pin, in2_state[m[1].direction]);
        TIM4->CCR3 = m[1].pwm;

        // --- Motor 1A (index 2): IN1=A11, IN2=A12, PWM=pwmA1, STBY=STBY1 ---
        HAL_GPIO_WritePin(A11_GPIO_Port, A11_Pin, in1_state[m[2].direction]);
        HAL_GPIO_WritePin(A12_GPIO_Port, A12_Pin, in2_state[m[2].direction]);
        TIM4->CCR2 = m[2].pwm;

        // --- Motor 1B (index 3): IN1=B11, IN2=B12, PWM=pwmB1, STBY=STBY1 ---
        HAL_GPIO_WritePin(B11_GPIO_Port, B11_Pin, in1_state[m[3].direction]);
        HAL_GPIO_WritePin(B12_GPIO_Port, B12_Pin, in2_state[m[3].direction]);
        TIM4->CCR1 = m[3].pwm;

        // --- Motor 2A (index 4): IN1=A21, IN2=A22, PWM=pwmA2, STBY=STBY2 ---
        HAL_GPIO_WritePin(A21_GPIO_Port, A21_Pin, in1_state[m[4].direction]);
        HAL_GPIO_WritePin(A22_GPIO_Port, A22_Pin, in2_state[m[4].direction]);
        TIM3->CCR2 = m[4].pwm;

        // --- Motor 2B (index 5): IN1=B21, IN2=B22, PWM=pwmB2, STBY=STBY2 ---
        HAL_GPIO_WritePin(B21_GPIO_Port, B21_Pin, in1_state[m[5].direction]);
        HAL_GPIO_WritePin(B22_GPIO_Port, B22_Pin, in2_state[m[5].direction]);
        TIM3->CCR1 = m[5].pwm;

        // --- STBY: LOW only if BOTH motors in the pair are dir 3 ---
        HAL_GPIO_WritePin(STBY0_GPIO_Port, STBY0_Pin, (m[0].direction == 3 && m[1].direction == 3) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(STBY1_GPIO_Port, STBY1_Pin, (m[2].direction == 3 && m[3].direction == 3) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(STBY2_GPIO_Port, STBY2_Pin, (m[4].direction == 3 && m[5].direction == 3) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }

    uint32_t AD_RES_BUFFER[6] = {0};

    uint8_t enable_adc_filter = 1;

    // Spike rejection
    uint16_t spike_threshold = 30;

    // Smoothing strength (1–255)
    // higher = smoother but slower
    uint8_t smooth_k = 10;  

    // Deadband (kills small noise completely)
    uint16_t deadband = 12;

    // Optional max step (rate limiter) (sets the max speed of the motor basically)
    uint16_t max_step = 20;
    uint8_t enable_rate_limit = 0;

    uint16_t adc_filtered[6] = {0};
    uint8_t adc_initialized[6] = {0};

    uint16_t debug_raw = 0;
    uint16_t debug_prev = 0;
    // uint16_t debug_AD_RES_BUFFER = 0;
    // int debug_count  = 0;


    void read_ADC()
    {
        for (int i = 0; i < 6; i++)
        {
            uint16_t raw = (uint16_t)AD_RES_BUFFER[i];
            debug_raw = raw;

            uint16_t prev = adc_filtered[i];
            debug_prev = prev;

            if (!adc_initialized[i])
            {
                adc_filtered[i] = raw;
                motors[i].pos = raw;
                adc_initialized[i] = 1;
                continue;
            }

            if (enable_adc_filter)
            {
                // // ---- 1. Spike rejection ---- //removed temporarily because it's causing issues during initialization where it doesnt let the filtered value go above 0
                // if (raw > prev + spike_threshold || raw + spike_threshold < prev)
                // {
                //     raw = prev;
                //     // debug_count++;
                // }

                // ---- 2. Deadband (this is what reduces "range to ~0") ----
                if (raw > prev)
                {
                    if (raw - prev < deadband)
                        raw = prev;
                }
                else
                {
                    if (prev - raw < deadband)
                        raw = prev;
                }

                // ---- 3. Exponential smoothing ----
                // new = (k*prev + raw) / (k+1)
                uint16_t filtered = (prev * smooth_k + raw) / (smooth_k + 1);

                // ---- 4. Optional rate limiter ----
                if (enable_rate_limit)
                {
                    if (filtered > prev + max_step)
                        filtered = prev + max_step;
                    else if (filtered + max_step < prev)
                        filtered = prev - max_step;
                }

                adc_filtered[i] = filtered;
                motors[i].pos = filtered;
            }
            else
            {
                motors[i].pos = raw;
                adc_filtered[i] = raw;
            }
        }
    }

/*-------------------- adc filter analysis --------------------*/
    // #define VAR_SAMPLES 8
    int var_samples = 64;
    int var_to_check = 0;


    uint16_t adc_prev[6] = {0};
    uint32_t adc_diff_sum[6] = {0};
    uint16_t adc_variation[6] = {0};
    uint16_t var_count[6] = {0};   // per-channel counter

    void measure_adc_variation(uint8_t idx)
    {
        uint16_t curr = (uint16_t)AD_RES_BUFFER[idx];
        uint16_t prev = adc_prev[idx];

        uint16_t diff;
        if (curr > prev) diff = curr - prev;
        else diff = prev - curr;

        adc_diff_sum[idx] += diff;
        adc_prev[idx] = curr;

        var_count[idx]++;

        if (var_count[idx] >= var_samples)
        {
            adc_variation[idx] = adc_diff_sum[idx] / var_samples;
            adc_diff_sum[idx] = 0;
            var_count[idx] = 0;
        }
    }

    // ----- RANGE CONFIG -----
    int range_samples = 64;
    int range_to_check = 0;

    // ----- RANGE STATE -----
    uint16_t adc_range_min[6] = {65535,65535,65535,65535,65535,65535};
    uint16_t adc_range_max[6] = {0};
    uint16_t adc_range[6] = {0};
    uint16_t range_count[6] = {0};

    // ----- FUNCTION -----
    void measure_adc_range(uint8_t idx)
    {
        uint16_t curr = (uint16_t)AD_RES_BUFFER[idx];

        // track min
        if (curr < adc_range_min[idx])
        {
            adc_range_min[idx] = curr;
        }

        // track max
        if (curr > adc_range_max[idx])
        {
            adc_range_max[idx] = curr;
        }

        range_count[idx]++;

        if (range_count[idx] >= range_samples)
        {
            adc_range[idx] = adc_range_max[idx] - adc_range_min[idx];

            // reset window
            adc_range_min[idx] = 65535;
            adc_range_max[idx] = 0;
            range_count[idx] = 0;
        }
    }

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == key_Pin)
    {
        HAL_GPIO_TogglePin(usr_led_GPIO_Port, usr_led_Pin);
    }
}

// void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
// {
//     adc_flag = 1;
// }

/*----------------microrl and USB CDC----------------*/

    uint8_t TxBuffer[] = "Hello World! From STM32 USB CDC Device To Virtual COM Port\r\n";
    volatile uint8_t usb_rx_flag = 0;
    uint32_t usb_rx_len = 0;

    #define RX_BUF_SIZE 256

    uint8_t rx_buf[RX_BUF_SIZE];
    volatile uint16_t rx_head = 0;
    volatile uint16_t rx_tail = 0;

    void USB_CDC_RxHandler(uint8_t* Buf, uint32_t Len)
    {
        for (uint32_t i = 0; i < Len; i++)
        {
            uint16_t next = (rx_head + 1) % RX_BUF_SIZE;

            if (next != rx_tail)   // avoid overflow
            {
                rx_buf[rx_head] = Buf[i];
                rx_head = next;
            }
        }
    }

    char debug_argv_id[4];

    int get_motor_index(const char* id)
    {
        if      (strcmp(id, "M0A") == 0) return 0;
        else if (strcmp(id, "M0B") == 0) return 1;
        else if (strcmp(id, "M1A") == 0) return 2;
        else if (strcmp(id, "M1B") == 0) return 3;
        else if (strcmp(id, "M2A") == 0) return 4;
        else if (strcmp(id, "M2B") == 0) return 5;

        return -1;
    }

    void print(const char * str)
    {
        while (CDC_Transmit_FS((uint8_t*)str, strlen(str)) == USBD_BUSY);
    }

    volatile uint8_t sigint_flag = 0;

    void sigint(void)
    {
        memset(motors, 0, sizeof(motors));
        sigint_flag = 1;
    }

    typedef enum {
        PARAM_U8,
        PARAM_U16,
        PARAM_INT
    } ParamType;

    typedef struct {
        const char* name;
        void* ptr;
        ParamType type;
    } Param;

    Param params[] = {
        {"spike_threshold", &spike_threshold, PARAM_U16},
        {"adc_filter", &enable_adc_filter, PARAM_U8},
        {"var_samples", &var_samples, PARAM_INT},
        {"var_to_check", &var_to_check, PARAM_INT},
        {"range_samples", &range_samples, PARAM_INT},
        {"range_to_check", &range_to_check, PARAM_INT},
        {"smooth_k", &smooth_k, PARAM_U8},
        {"deadband", &deadband, PARAM_U16},
        {"max_step", &max_step, PARAM_U16},
        {"enable_rate_limit", &enable_rate_limit, PARAM_U8}
    };

    #define PARAM_COUNT (sizeof(params) / sizeof(params[0]))

    int execute(int argc, const char * const *argv)
    {
        if (argc == 0) return 0;

        // ---------------- setmotor ----------------
        if (strcmp(argv[0], "setmotor") == 0)
        {
            if (argc != 4)
            {
                print("Usage: setmotor <motor> <pwm> <dir>\n");
                return 0;
            }

            int index = get_motor_index(argv[1]);
            if (index == -1)
            {
                print("Invalid motor\n");
                return 0;
            }

            int pwm = atoi(argv[2]);
            int dir = atoi(argv[3]);

            motors[index].pwm = pwm;
            motors[index].direction = dir;

            print("OK\n");
            return 1;
        }

        // ---------------- stop ----------------
        if (strcmp(argv[0], "stop") == 0)
        {
            if (argc == 1)
            {
                memset(motors, 0, sizeof(motors));
                print("All motors stopped\n");
                return 1;
            }

            if (argc == 2)
            {
                int index = get_motor_index(argv[1]);
                if (index == -1)
                {
                    print("Invalid motor\n");
                    return 0;
                }

                motors[index].pwm = 0;
                motors[index].direction = 0;

                print("Motor stopped\n");
                return 1;
            }

            print("Usage: stop [motor]\n");
            return 0;
        }

        // ---------------- setparam ----------------
        if (strcmp(argv[0], "setparam") == 0)
        {
            if (argc != 3)
            {
                print("Usage: setparam <param> <value>\n");
                return 0;
            }

            int value = atoi(argv[2]);

            for (int i = 0; i < PARAM_COUNT; i++)
            {
                if (strcmp(argv[1], params[i].name) == 0)
                {
                    switch (params[i].type)
                        {
                            case PARAM_U8:
                                *(uint8_t*)params[i].ptr = (uint8_t)value;
                                break;

                            case PARAM_U16:
                                *(uint16_t*)params[i].ptr = (uint16_t)value;
                                break;

                            case PARAM_INT:
                                *(int*)params[i].ptr = value;
                                break;
                        }
                    print("OK\n");
                    return 1;
                }
            }

            print("Unknown parameter\n");
            return 0;
        }

        // ---------------- getparam ----------------
        if (strcmp(argv[0], "getparam") == 0)
        {
            if (argc != 2)
            {
                print("Usage: getparam <param>\n");
                return 0;
            }

            char buf[32];

            for (int i = 0; i < PARAM_COUNT; i++)
            {
                if (strcmp(argv[1], params[i].name) == 0)
                {
                    int val = 0;
                    switch (params[i].type)
                    {
                        case PARAM_U8:
                            val = *(uint8_t*)params[i].ptr;
                            break;

                        case PARAM_U16:
                            val = *(uint16_t*)params[i].ptr;
                            break;

                        case PARAM_INT:
                            val = *(int*)params[i].ptr;
                            break;
                    }
                    sprintf(buf, "%d\n", val);
                    print(buf);
                    return 1;
                }
            }

            print("Unknown parameter\n");
            return 0;
        }
        // ---------------- listparams ----------------
        if (strcmp(argv[0], "listparams") == 0)
        {
            char buf[256];
            int len = 0;

            for (int i = 0; i < PARAM_COUNT; i++)
            {
                int written = snprintf(buf + len, sizeof(buf) - len, "%s\r\n", params[i].name);

                if (written <= 0 || len + written >= sizeof(buf))
                    break;

                len += written;
            }

            print(buf);
            return 1;
        }

        // ---------------- fallback ----------------
        print("Unknown command\n");
        return 0;
    }

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */

int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  microrl_init(prl, print);
  microrl_set_execute_callback(prl, execute);
  microrl_set_sigint_callback(prl, sigint);
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);

  HAL_ADC_Start_DMA(&hadc1, AD_RES_BUFFER, 6);

  HAL_GPIO_WritePin(usr_led_GPIO_Port, usr_led_Pin, 1);
  memset(motors, 0, sizeof(motors));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Apply motor control continuously for all 6 motors
    read_ADC();
    // measure_adc_variation(var_to_check);
    measure_adc_range(range_to_check);
    motor_control(motors);

    while (rx_tail != rx_head)
    {
        uint8_t ch = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;

        microrl_insert_char(prl, ch);
    }

    if (sigint_flag)
    {
        CDC_Transmit_FS((uint8_t*)"SIGINT\n", 7);
        sigint_flag = 0;
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */
  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 6;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */
  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */
  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */
  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 255;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */
  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */
  /* USER CODE END TIM4_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */
  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 255;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */
  /* USER CODE END TIM4_Init 2 */
  HAL_TIM_MspPostInit(&htim4);

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, usr_led_Pin|A01_Pin|A02_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, B21_Pin|A22_Pin|A21_Pin|B01_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, B12_Pin|B11_Pin|A11_Pin|A12_Pin
                          |STBY0_Pin|STBY1_Pin|STBY2_Pin|B22_Pin
                          |B02_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : usr_led_Pin A01_Pin A02_Pin */
  GPIO_InitStruct.Pin = usr_led_Pin|A01_Pin|A02_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : key_Pin */
  GPIO_InitStruct.Pin = key_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(key_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : B21_Pin A22_Pin A21_Pin B01_Pin */
  GPIO_InitStruct.Pin = B21_Pin|A22_Pin|A21_Pin|B01_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : B12_Pin B11_Pin A11_Pin A12_Pin
                           STBY0_Pin STBY1_Pin STBY2_Pin B22_Pin
                           B02_Pin */
  GPIO_InitStruct.Pin = B12_Pin|B11_Pin|A11_Pin|A12_Pin
                          |STBY0_Pin|STBY1_Pin|STBY2_Pin|B22_Pin
                          |B02_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
