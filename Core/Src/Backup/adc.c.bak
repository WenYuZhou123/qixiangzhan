/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "weather_data.h"
#include <string.h>

volatile uint16_t g_weather_adc_raw[3] = {0U, 0U, 0U};
static uint8_t s_weather_adc_calibrated = 0U;
static uint8_t s_weather_adc_cal_warn = 0U;

#define WEATHER_ADC_AVG_SAMPLES        20U
#define WEATHER_ADC_DISCARD_SAMPLES    1U
#define WEATHER_ADC_SAMPLE_DELAY_MS    5U
#define WEATHER_ADC_SAMPLE_TIME        ADC_SAMPLETIME_64CYCLES_5
#define WEATHER_ADC_RAIN_INDEX         2U
#define WEATHER_ADC_RAIN_CHANNEL       ADC_CHANNEL_8

static void WeatherADC_SetModeText(const char *text)
{
  if (text == NULL)
  {
    text = "RADC";
  }

  strncpy(g_weather_data.adc_mode_text, text, sizeof(g_weather_data.adc_mode_text) - 1U);
  g_weather_data.adc_mode_text[sizeof(g_weather_data.adc_mode_text) - 1U] = '\0';
}

/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  s_weather_adc_calibrated = 0U;
  s_weather_adc_cal_warn = 0U;

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_16B;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = WEATHER_ADC_RAIN_CHANNEL;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = WEATHER_ADC_SAMPLE_TIME;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInitStruct.PLL2.PLL2M = 4;
    PeriphClkInitStruct.PLL2.PLL2N = 9;
    PeriphClkInitStruct.PLL2.PLL2P = 2;
    PeriphClkInitStruct.PLL2.PLL2Q = 2;
    PeriphClkInitStruct.PLL2.PLL2R = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
    PeriphClkInitStruct.PLL2.PLL2FRACN = 3072;
    PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC1 clock enable */
    __HAL_RCC_ADC12_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PC5     ------> ADC1_INP8
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_ADC12_CLK_DISABLE();

    /**ADC1 GPIO Configuration
    PC5     ------> ADC1_INP8
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5);

  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

static HAL_StatusTypeDef WeatherADC_CalibrateIfNeeded(void)
{
  if (s_weather_adc_calibrated != 0U)
  {
    return HAL_OK;
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK)
  {
    s_weather_adc_cal_warn = 1U;
  }

  s_weather_adc_calibrated = 1U;
  HAL_Delay(1U);
  return HAL_OK;
}

static HAL_StatusTypeDef WeatherADC_ReadRainAverage(uint16_t *value_out)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t accum = 0U;
  uint32_t sample_index = 0U;

  if (value_out == NULL)
  {
    return HAL_ERROR;
  }

  if (WeatherADC_CalibrateIfNeeded() != HAL_OK)
  {
    return HAL_ERROR;
  }

  (void)HAL_ADC_Stop(&hadc1);

  sConfig.Channel = WEATHER_ADC_RAIN_CHANNEL;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = WEATHER_ADC_SAMPLE_TIME;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;

  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    return HAL_ERROR;
  }

  for (sample_index = 0U; sample_index < WEATHER_ADC_DISCARD_SAMPLES; sample_index++)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return HAL_ERROR;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 100U) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return HAL_ERROR;
    }

    (void)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
  }

  for (sample_index = 0U; sample_index < WEATHER_ADC_AVG_SAMPLES; sample_index++)
  {
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      return HAL_ERROR;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 100U) != HAL_OK)
    {
      (void)HAL_ADC_Stop(&hadc1);
      return HAL_ERROR;
    }

    accum += HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);

    if ((sample_index + 1U) < WEATHER_ADC_AVG_SAMPLES)
    {
      HAL_Delay(WEATHER_ADC_SAMPLE_DELAY_MS);
    }
  }

  *value_out = (uint16_t)(accum / WEATHER_ADC_AVG_SAMPLES);
  return HAL_OK;
}

HAL_StatusTypeDef WeatherADC_Start(void)
{
  (void)HAL_ADC_Stop(&hadc1);
  s_weather_adc_calibrated = 0U;
  s_weather_adc_cal_warn = 0U;
  return WeatherADC_RefreshFallback();
}

HAL_StatusTypeDef WeatherADC_RefreshFallback(void)
{
  uint16_t rain_raw = 0U;

  if (WeatherADC_ReadRainAverage(&rain_raw) != HAL_OK)
  {
    WeatherADC_SetModeText("C8_ERR");
    return HAL_ERROR;
  }

  g_weather_adc_raw[0] = 0U;
  g_weather_adc_raw[1] = 0U;
  g_weather_adc_raw[WEATHER_ADC_RAIN_INDEX] = rain_raw;
  WeatherADC_SetModeText((s_weather_adc_cal_warn != 0U) ? "R_NOCAL" : "RADC");
  return HAL_OK;
}

uint16_t WeatherADC_ReadRaw(uint32_t index)
{
  if (index >= 3U)
  {
    return 0U;
  }

  return g_weather_adc_raw[index];
}

/* USER CODE END 1 */
