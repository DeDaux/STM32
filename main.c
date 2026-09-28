#include <stdint.h>

/* STM32F303 baziniai adresai */
#define RCC_BASE    0x40021000UL
#define GPIOE_BASE  0x48001000UL
#define TIM1_BASE   0x40012C00UL

#define RCC_AHBENR   (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOE_MODER    (*(volatile uint32_t *)(GPIOE_BASE + 0x00))
#define GPIOE_OTYPER   (*(volatile uint32_t *)(GPIOE_BASE + 0x04))
#define GPIOE_OSPEEDR  (*(volatile uint32_t *)(GPIOE_BASE + 0x08))
#define GPIOE_PUPDR    (*(volatile uint32_t *)(GPIOE_BASE + 0x0C))
#define GPIOE_AFRH     (*(volatile uint32_t *)(GPIOE_BASE + 0x24))

#define TIM1_CR1    (*(volatile uint32_t *)(TIM1_BASE + 0x00))
#define TIM1_EGR    (*(volatile uint32_t *)(TIM1_BASE + 0x14))
#define TIM1_CCMR1  (*(volatile uint32_t *)(TIM1_BASE + 0x18))
#define TIM1_CCER   (*(volatile uint32_t *)(TIM1_BASE + 0x20))
#define TIM1_PSC    (*(volatile uint32_t *)(TIM1_BASE + 0x28))
#define TIM1_ARR    (*(volatile uint32_t *)(TIM1_BASE + 0x2C))
#define TIM1_CCR1   (*(volatile uint32_t *)(TIM1_BASE + 0x34))
#define TIM1_BDTR   (*(volatile uint32_t *)(TIM1_BASE + 0x44))

#define RCC_AHBENR_GPIOEEN   (1UL << 21)
#define RCC_APB2ENR_TIM1EN   (1UL << 11)
#define TIM_EGR_UG           (1UL << 0)
#define TIM_CCMR1_OC1PE      (1UL << 3)
#define TIM_CCER_CC1E        (1UL << 0)
#define TIM_BDTR_MOE         (1UL << 15)
#define TIM_CR1_CEN          (1UL << 0)
#define TIM_CR1_ARPE         (1UL << 7)

int i = 0;
int direction = 1;

static void delay_ms(uint32_t ms)
{
    /* Apytikslis ~1 ms @ 8 MHz HSI. Jei SYSCLK 72 MHz — padidink 800 iki ~7200. */
    for (uint32_t t = 0; t < ms; t++) {
        for (volatile uint32_t n = 0; n < 800; n++) {
            __asm volatile ("nop");
        }
    }
}

static void rcc_enable(void)
{
    RCC_AHBENR  |= RCC_AHBENR_GPIOEEN;  /* GPIOE */
    RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;  /* TIM1  */
}

static void gpio_led_af_tim1(void)
{
    /* PE9 = AF, push-pull, AF2 = TIM1_CH1 (LD3 raudonas) */
    GPIOE_MODER   &= ~(3UL << (9 * 2));
    GPIOE_MODER   |=  (2UL << (9 * 2));   /* Alternate function */

    GPIOE_OTYPER  &= ~(1UL << 9);
    GPIOE_OSPEEDR |=  (3UL << (9 * 2));
    GPIOE_PUPDR   &= ~(3UL << (9 * 2));

    GPIOE_AFRH    &= ~(0xFUL << ((9 - 8) * 4));
    GPIOE_AFRH    |=  (2UL   << ((9 - 8) * 4));  /* AF2 */
}

static void timer_pwm_init(void)
{
    TIM1_CR1   = 0;
    TIM1_PSC   = 7;     /* 8 MHz / 8 = 1 MHz. Jei 72 MHz: PSC = 71 */
    TIM1_ARR   = 99;    /* fiksuotas periodas, 100 žingsnių */
    TIM1_CCR1  = 0;

    /* PWM mode 1 (OC1M = 110), preload */
    TIM1_CCMR1 = (6UL << 4) | TIM_CCMR1_OC1PE;

    TIM1_CCER  = TIM_CCER_CC1E;
    TIM1_BDTR  = TIM_BDTR_MOE;   /* privaloma TIM1 */
    TIM1_EGR   = TIM_EGR_UG;
    TIM1_CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

static void TimerFunction(int duty)
{
    if (duty < 0)   duty = 0;
    if (duty > 100) duty = 100;
    TIM1_CCR1 = (uint32_t)duty;
}

int main(void)
{
    rcc_enable();
    gpio_led_af_tim1();
    timer_pwm_init();

    i = 0;
    direction = 1;

    while (1) {
        TimerFunction(i);
        delay_ms(15);

        i += direction;
        if (i >= 100) {
            i = 100;
            direction = -1;
        } else if (i <= 0) {
            i = 0;
            direction = 1;
        }
    }
}
