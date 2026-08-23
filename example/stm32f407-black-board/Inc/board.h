#pragma once

#include "stm32f407_min.h"

#define LED0_PIN     9U
#define LED1_PIN     10U

#define KEY0_PIN     4U
#define KEY1_PIN     3U
#define WKUP_PIN     0U

static inline void board_gpio_init(void)
{
    /*
     * GPIOA: WK_UP
     * GPIOE: KEY0, KEY1
     * GPIOF: LED0, LED1
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOEEN |
                    RCC_AHB1ENR_GPIOFEN;

    /*
     * Дать тактированию GPIO гарантированно включиться
     * перед обращением к регистрам портов.
     */
    (void)RCC->AHB1ENR;


    /*
     * LED0 PF9
     * LED1 PF10
     *
     * LEDs active low:
     * 1 = off
     * 0 = on
     *
     * Сначала выставляем выходы в 1,
     * чтобы светодиоды не мигнули при переключении MODER.
     */
    GPIOF->BSRR = (1UL << LED0_PIN) |
                  (1UL << LED1_PIN);

    /* Output mode: 01 */
    GPIOF->MODER &=
        ~((3UL << (LED0_PIN * 2U)) |
          (3UL << (LED1_PIN * 2U)));

    GPIOF->MODER |=
         (1UL << (LED0_PIN * 2U)) |
         (1UL << (LED1_PIN * 2U));

    /* Push-pull */
    GPIOF->OTYPER &=
        ~((1UL << LED0_PIN) |
          (1UL << LED1_PIN));

    /* Low speed */
    GPIOF->OSPEEDR &=
        ~((3UL << (LED0_PIN * 2U)) |
          (3UL << (LED1_PIN * 2U)));

    /* No pull-up / pull-down */
    GPIOF->PUPDR &=
        ~((3UL << (LED0_PIN * 2U)) |
          (3UL << (LED1_PIN * 2U)));


    /*
     * KEY0 PE4
     * KEY1 PE3
     *
     * Input mode: 00
     */
    GPIOE->MODER &=
        ~((3UL << (KEY0_PIN * 2U)) |
          (3UL << (KEY1_PIN * 2U)));

    /*
     * Pull-up.
     * Кнопки active low:
     * released = 1
     * pressed  = 0
     */
    GPIOE->PUPDR &=
        ~((3UL << (KEY0_PIN * 2U)) |
          (3UL << (KEY1_PIN * 2U)));

    GPIOE->PUPDR |=
         (1UL << (KEY0_PIN * 2U)) |
         (1UL << (KEY1_PIN * 2U));


    /*
     * WK_UP PA0
     *
     * Input mode
     */
    GPIOA->MODER &= ~(3UL << (WKUP_PIN * 2U));

    /*
     * Pull-down.
     * WK_UP active high:
     * released = 0
     * pressed  = 1
     */
    GPIOA->PUPDR &= ~(3UL << (WKUP_PIN * 2U));
    GPIOA->PUPDR |=  (2UL << (WKUP_PIN * 2U));
}

static inline void led0_on(void)
{
    GPIOF->BSRR = 1UL << (LED0_PIN + 16U);
}

static inline void led0_off(void)
{
    GPIOF->BSRR = 1UL << LED0_PIN;
}

static inline void led1_on(void)
{
    GPIOF->BSRR = 1UL << (LED1_PIN + 16U);
}

static inline void led1_off(void)
{
    GPIOF->BSRR = 1UL << LED1_PIN;
}


static inline uint32_t key0_pressed(void)
{
    return (GPIOE->IDR & (1UL << KEY0_PIN)) == 0;
}

static inline uint32_t key1_pressed(void)
{
    return (GPIOE->IDR & (1UL << KEY1_PIN)) == 0;
}

static inline uint32_t wkup_pressed(void)
{
    return (GPIOA->IDR & (1UL << WKUP_PIN)) != 0;
}