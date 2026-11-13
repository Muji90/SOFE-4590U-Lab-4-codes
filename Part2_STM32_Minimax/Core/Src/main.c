/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   SOFE 4590U Lab 4 - STM32 Minimax AI Game
 *
 * STM32F411VETx
 *
 * Features:
 * - Deterministic Minimax AI
 * - Subtraction game (1, 2 or 3)
 * - Single-button human input (PA0)
 * - HD44780 16x2 LCD (4-bit mode)
 * - DWT CPU execution-time measurement
 * - Benchmarks at 20, 10 and 5 remaining
 * - Game-over display and restart
 ******************************************************************************
 */
/* USER CODE END Header */

#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>
#include "game_logic.h"
#include "ai_timing.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* USER CODE BEGIN PD */
#define NUMBER_OF_TESTS 3
#define BUTTON_DEBOUNCE_MS 180U
#define MOVE_SELECTION_MS 1500U
/* USER CODE END PD */

/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* USER CODE BEGIN PV */

/* Game state */
volatile int game_remaining = GAME_START_NUMBER;
volatile int human_move = 0;
volatile int ai_selected_move = 0;

/* AI timing */
volatile uint32_t ai_execution_cycles = 0;
volatile uint32_t ai_execution_us = 0;

/* Benchmark results */
volatile int test_remaining[NUMBER_OF_TESTS] =
{
    20, 10, 5
};

volatile int test_moves[NUMBER_OF_TESTS] = {0};

volatile uint32_t test_cycles[NUMBER_OF_TESTS] = {0};

volatile uint32_t test_time_us[NUMBER_OF_TESTS] = {0};

volatile int tests_completed = 0;

/* Button input */
static volatile uint32_t button_count = 0;
static volatile uint32_t first_press_tick = 0;
static volatile uint32_t last_press_tick = 0;
static volatile uint8_t selection_active = 0;

/* LCD text buffers */
static char lcd_line1[17];
static char lcd_line2[17];

/* USER CODE END PV */

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

/* USER CODE BEGIN PFP */
static void Delay_us(uint32_t us);
static void LCD_Init(void);
static void LCD_Show(const char *line1,
                     const char *line2);
static int Button_ReadMove(void);
static void Run_Benchmarks(void);
static void Run_Game(void);
static void Game_Over(int human_won);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/*
 * Microsecond delay using processor cycles.
 * DWT must be initialized first.
 */
static void Delay_us(uint32_t us)
{
    uint32_t cycles_per_us =
        HAL_RCC_GetHCLKFreq() / 1000000U;

    if (cycles_per_us == 0U)
    {
        return;
    }

    while (us > 0U)
    {
        uint32_t start = DWT->CYCCNT;

        while ((uint32_t)(DWT->CYCCNT - start)
               < cycles_per_us)
        {
            __NOP();
        }

        us--;
    }
}

/*
 * LCD low-level control.
 * Pins are defined by CubeMX in main.h.
 */

static void LCD_EnablePulse(void)
{
    HAL_GPIO_WritePin(
        E_GPIO_Port, E_Pin, GPIO_PIN_SET
    );

    Delay_us(2);

    HAL_GPIO_WritePin(
        E_GPIO_Port, E_Pin, GPIO_PIN_RESET
    );

    Delay_us(100);
}

static void LCD_WriteNibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(
        D4_GPIO_Port, D4_Pin,
        (nibble & 0x01U) ?
        GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        D5_GPIO_Port, D5_Pin,
        (nibble & 0x02U) ?
        GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        D6_GPIO_Port, D6_Pin,
        (nibble & 0x04U) ?
        GPIO_PIN_SET : GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        D7_GPIO_Port, D7_Pin,
        (nibble & 0x08U) ?
        GPIO_PIN_SET : GPIO_PIN_RESET
    );

    LCD_EnablePulse();
}

static void LCD_WriteByte(uint8_t value,
                          uint8_t is_data)
{
    HAL_GPIO_WritePin(
        RS_GPIO_Port, RS_Pin,
        is_data ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    LCD_WriteNibble(value >> 4);
    LCD_WriteNibble(value & 0x0FU);
}

static void LCD_Command(uint8_t command)
{
    LCD_WriteByte(command, 0);

    if (command == 0x01U || command == 0x02U)
    {
        HAL_Delay(2);
    }
}

static void LCD_Data(uint8_t data)
{
    LCD_WriteByte(data, 1);
}

static void LCD_Init(void)
{
    HAL_GPIO_WritePin(
        RS_GPIO_Port, RS_Pin, GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        E_GPIO_Port, E_Pin, GPIO_PIN_RESET
    );

    HAL_Delay(50);

    LCD_WriteNibble(0x03);
    HAL_Delay(5);

    LCD_WriteNibble(0x03);
    Delay_us(200);

    LCD_WriteNibble(0x03);
    Delay_us(200);

    LCD_WriteNibble(0x02);
    Delay_us(200);

    LCD_Command(0x28);  /* 4-bit, 2 lines */
    LCD_Command(0x0C);  /* Display on */
    LCD_Command(0x06);  /* Increment cursor */
    LCD_Command(0x01);  /* Clear display */
}

static void LCD_PrintLine(uint8_t row,
                          const char *text)
{
    LCD_Command(row == 0U ? 0x80U : 0xC0U);

    uint8_t finished = 0U;

    for (int i = 0; i < 16; i++)
    {
        if (!finished && *text == '\0')
        {
            finished = 1U;
        }

        if (finished)
        {
            LCD_Data(' ');
        }
        else
        {
            LCD_Data((uint8_t)*text);
            text++;
        }
    }
}

static void LCD_Show(const char *line1,
                     const char *line2)
{
    LCD_PrintLine(0, line1);
    LCD_PrintLine(1, line2);
}

/*
 * Single-button move selection.
 *
 * Press once  -> subtract 1
 * Press twice -> subtract 2
 * Press 3x    -> subtract 3
 *
 * The move is confirmed after 1.5 seconds
 * from the first registered press.
 */
static int Button_ReadMove(void)
{
    button_count = 0;
    first_press_tick = 0;
    last_press_tick = 0;

    selection_active = 1U;

    /* Wait for first press */
    while (button_count == 0U)
    {
        HAL_Delay(10);
    }

    uint32_t start = first_press_tick;

    while ((uint32_t)(HAL_GetTick() - start)
           < MOVE_SELECTION_MS)
    {
        HAL_Delay(10);
    }

    selection_active = 0U;

    int result = (int)button_count;

    if (result < 1)
    {
        result = 1;
    }

    if (result > GAME_MAX_MOVE)
    {
        result = GAME_MAX_MOVE;
    }

    return result;
}

/*
 * Performance tests:
 * 20, 10 and 5 remaining.
 *
 * Results are accessible from the debugger.
 */
static void Run_Benchmarks(void)
{
    tests_completed = 0;

    for (int i = 0; i < NUMBER_OF_TESTS; i++)
    {
        uint32_t start_cycles =
            AI_Timer_Start();

        int move =
            find_best_move(test_remaining[i]);

        uint32_t elapsed_cycles =
            AI_Timer_GetCycles(start_cycles);

        test_moves[i] = move;
        test_cycles[i] = elapsed_cycles;

        test_time_us[i] =
            AI_Timer_CyclesToUs(
                elapsed_cycles,
                HAL_RCC_GetHCLKFreq()
            );

        tests_completed++;
    }
}

/*
 * Show the winner and restart instruction.
 */
static void Game_Over(int human_won)
{
    if (human_won)
    {
        LCD_Show(
            "Human Wins!",
            "Game Over"
        );
    }
    else
    {
        LCD_Show(
            "AI Wins!",
            "Game Over"
        );
    }

    HAL_Delay(2000);

    LCD_Show(
        "Game Finished",
        "Press to Restart"
    );

    (void)Button_ReadMove();
}

/*
 * Main subtraction game.
 */
static void Run_Game(void)
{
    game_remaining = GAME_START_NUMBER;

    LCD_Show(
        "Minimax AI Game",
        "Starting..."
    );

    HAL_Delay(1500);

    while (game_remaining > 0)
    {
        /* Human player */
        snprintf(
            lcd_line1,
            sizeof(lcd_line1),
            "Remaining:%d",
            game_remaining
        );

        LCD_Show(
            lcd_line1,
            "Press 1-3 times"
        );

        human_move = Button_ReadMove();

        if (human_move > game_remaining)
        {
            LCD_Show(
                "Invalid Move",
                "Please Try Again"
            );

            HAL_Delay(1200);
            continue;
        }

        game_remaining -= human_move;

        if (game_remaining == 0)
        {
            Game_Over(LAST_MOVE_WINS ? 1 : 0);
            return;
        }

        /* AI player */
        LCD_Show(
            "AI Thinking...",
            "Please Wait"
        );

        uint32_t start_cycles =
            AI_Timer_Start();

        ai_selected_move =
            find_best_move(game_remaining);

        ai_execution_cycles =
            AI_Timer_GetCycles(start_cycles);

        ai_execution_us =
            AI_Timer_CyclesToUs(
                ai_execution_cycles,
                HAL_RCC_GetHCLKFreq()
            );

        game_remaining -= ai_selected_move;

        snprintf(
            lcd_line1,
            sizeof(lcd_line1),
            "AI:-%d Left:%d",
            ai_selected_move,
            game_remaining
        );

        snprintf(
            lcd_line2,
            sizeof(lcd_line2),
            "Time:%lu us",
            (unsigned long)ai_execution_us
        );

        LCD_Show(
            lcd_line1,
            lcd_line2
        );

        /* Display time for observation/photos */
        HAL_Delay(5000);

        if (game_remaining == 0)
        {
            Game_Over(LAST_MOVE_WINS ? 0 : 1);
            return;
        }
    }
}

/* USER CODE END 0 */

/**
 * @brief Application entry point.
 */
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */
    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */
    /* USER CODE END SysInit */

    MX_GPIO_Init();

    /* USER CODE BEGIN 2 */

    AI_Timer_Init();

    LCD_Init();

    LCD_Show(
        "SOFE 4590U Lab4",
        "STM32 Minimax AI"
    );

    HAL_Delay(2000);

    Run_Benchmarks();

    /* USER CODE END 2 */

    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        Run_Game();
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)
        != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV1;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
        &RCC_ClkInitStruct,
        FLASH_LATENCY_0
    ) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief GPIO Initialization Function
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* Configure LCD output levels */
    HAL_GPIO_WritePin(
        GPIOB,
        D4_Pin | D5_Pin,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        GPIOD,
        RS_Pin | D6_Pin | E_Pin | D7_Pin,
        GPIO_PIN_RESET
    );

    /* PA0 button: rising-edge interrupt */
    GPIO_InitStruct.Pin = BT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(
        BT_GPIO_Port,
        &GPIO_InitStruct
    );

    /* LCD GPIOB outputs */
    GPIO_InitStruct.Pin = D4_Pin | D5_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );

    /* LCD GPIOD outputs */
    GPIO_InitStruct.Pin =
        RS_Pin | D6_Pin | E_Pin | D7_Pin;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOD,
        &GPIO_InitStruct
    );

    /* EXTI interrupt initialization */
    HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/*
 * PA0 interrupt callback.
 *
 * Requires the CubeMX-generated EXTI0_IRQHandler
 * to call HAL_GPIO_EXTI_IRQHandler(BT_Pin).
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != BT_Pin)
    {
        return;
    }

    if (!selection_active)
    {
        return;
    }

    uint32_t now = HAL_GetTick();

    if (button_count == 0U)
    {
        button_count = 1U;
        first_press_tick = now;
        last_press_tick = now;
    }
    else if ((uint32_t)(now - last_press_tick)
             >= BUTTON_DEBOUNCE_MS)
    {
        if (button_count < GAME_MAX_MOVE)
        {
            button_count++;
        }

        last_press_tick = now;
    }
}

/* USER CODE END 4 */

/**
 * @brief Error handler
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    __disable_irq();

    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    (void)file;
    (void)line;
    /* USER CODE END 6 */
}
#endif
