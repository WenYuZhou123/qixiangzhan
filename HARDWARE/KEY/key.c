#include "key.h"

#define KEY_GPIO_PORT    GPIOC
#define KEY_GPIO_PIN     GPIO_PIN_13

uint8_t Key_Scan(void)
{
    if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN) == GPIO_PIN_RESET)
    {
        HAL_Delay(20);  // Ïû¶¶

        if (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN) == GPIO_PIN_RESET)
        {
            while (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN) == GPIO_PIN_RESET);
            return 1;
        }
    }

    return 0;
}
