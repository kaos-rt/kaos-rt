#pragma once

#include <stdint.h>


#define PERIPH_BASE         0x40000000UL
#define APB1PERIPH_BASE     PERIPH_BASE
#define AHB1PERIPH_BASE     (PERIPH_BASE + 0x00020000UL)

#define TIM2_BASE           (APB1PERIPH_BASE + 0x0000UL)
#define RCC_BASE            (AHB1PERIPH_BASE + 0x3800UL)


typedef struct
{
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
} TIM_TypeDef;


typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    uint32_t RESERVED0;
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    uint32_t RESERVED2;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} RCC_TypeDef;


#define TIM2    ((TIM_TypeDef *)TIM2_BASE)
#define RCC     ((RCC_TypeDef *)RCC_BASE)


#define RCC_APB1ENR_TIM2EN      (1UL << 0)
#define TIM_CR1_CEN             (1UL << 0)
#define TIM_EGR_UG              (1UL << 0)

typedef struct
{
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;


#define GPIOA_BASE   0x40020000UL
#define GPIOE_BASE   0x40021000UL
#define GPIOF_BASE   0x40021400UL

#define RCC          ((RCC_TypeDef *)RCC_BASE)

#define GPIOA        ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOE        ((GPIO_TypeDef *)GPIOE_BASE)
#define GPIOF        ((GPIO_TypeDef *)GPIOF_BASE)


#define RCC_AHB1ENR_GPIOAEN   (1UL << 0)
#define RCC_AHB1ENR_GPIOEEN   (1UL << 4)
#define RCC_AHB1ENR_GPIOFEN   (1UL << 5)


static inline void irq_disable(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}


static inline void irq_enable(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}
