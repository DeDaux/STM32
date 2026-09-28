#include <stdint.h>

/* ---------- Baziniai adresai ---------- */
#define RCC_BASE     0x40021000UL
#define GPIOA_BASE   0x48000000UL
#define GPIOE_BASE   0x48001000UL
#define TIM1_BASE    0x40012C00UL
#define SYSCFG_BASE  0x40010000UL
#define EXTI_BASE    0x40010400UL
#define NVIC_ISER0   (*(volatile uint32_t *)0xE000E100UL)

/* RCC */
#define RCC_AHBENR   (*(volatile uint32_t *)(RCC_BASE + 0x14))
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x18))

/* GPIOA – mygtukas PA0 */
#define GPIOA_MODER  (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_PUPDR  (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))

/* GPIOE – LED */
#define GPIOE_MODER    (*(volatile uint32_t *)(GPIOE_BASE + 0x00))
#define GPIOE_OTYPER   (*(volatile uint32_t *)(GPIOE_BASE + 0x04))
#define GPIOE_OSPEEDR  (*(volatile uint32_t *)(GPIOE_BASE + 0x08))
#define GPIOE_PUPDR    (*(volatile uint32_t *)(GPIOE_BASE + 0x0C))
#define GPIOE_ODR      (*(volatile uint32_t *)(GPIOE_BASE + 0x14))
#define GPIOE_AFRH     (*(volatile uint32_t *)(GPIOE_BASE + 0x24))

/* TIM1 */
#define TIM1_CR1    (*(volatile uint32_t *)(TIM1_BASE + 0x00))
#define TIM1_EGR    (*(volatile uint32_t *)(TIM1_BASE + 0x14))
#define TIM1_CCMR1  (*(volatile uint32_t *)(TIM1_BASE + 0x18))
#define TIM1_CCER   (*(volatile uint32_t *)(TIM1_BASE + 0x20))
#define TIM1_PSC    (*(volatile uint32_t *)(TIM1_BASE + 0x28))
#define TIM1_ARR    (*(volatile uint32_t *)(TIM1_BASE + 0x2C))
#define TIM1_CCR1   (*(volatile uint32_t *)(TIM1_BASE + 0x34))
#define TIM1_CCR2   (*(volatile uint32_t *)(TIM1_BASE + 0x38))
#define TIM1_BDTR   (*(volatile uint32_t *)(TIM1_BASE + 0x44))

/* SYSCFG + EXTI */
#define SYSCFG_EXTICR1 (*(volatile uint32_t *)(SYSCFG_BASE + 0x08))
#define EXTI_IMR       (*(volatile uint32_t *)(EXTI_BASE + 0x00))
#define EXTI_RTSR      (*(volatile uint32_t *)(EXTI_BASE + 0x08))
#define EXTI_FTSR      (*(volatile uint32_t *)(EXTI_BASE + 0x0C))
#define EXTI_PR        (*(volatile uint32_t *)(EXTI_BASE + 0x14))

/* Bitai */
#define RCC_AHBENR_GPIOAEN  (1UL << 17)
#define RCC_AHBENR_GPIOEEN  (1UL << 21)
#define RCC_APB2ENR_SYSCFGEN (1UL << 0)
#define RCC_APB2ENR_TIM1EN   (1UL << 11)

#define PWM_PERIOD  1000          /* ARR – vienas PWM periodas */
#define PWM_STEP    (PWM_PERIOD / 10)  /* +10 % */
#define EXTI0_IRQn  6             /* EXTI0 numeris NVIC lentelėje */

int i = 100;   /* dabartinis duty */

void EXTI0_IRQHandler(void);
void EXTI0_Config(void);
void Init_Tim_Gpio(void);
void Init_Tim(int dutty);

void EXTI0_IRQHandler(void)
{
    /* Ar suveikė būtent linija 0 (PA0)? */
    if (EXTI_PR & (1UL << 0)) {

        /* Išvalome vėliavėlę (rašome 1). Kitaip IRQ suksis be galo. */
        EXTI_PR = (1UL << 0);

        /* PE15 (LD6 žalias) – paprastas LED, apverčiame */
        GPIOE_ODR ^= (1UL << 15);

        /* Duty +10 % periodo */
        i = i + PWM_STEP;

        /* Jei peršokome periodą – vėl nuo 0 */
        if (i > PWM_PERIOD) {
            i = 0;
        }

        /* Naujas impulso plotis abiem PWM kanalams */
        TIM1_CCR1 = (uint32_t)i;   /* PE9  TIM1_CH1 */
        TIM1_CCR2 = (uint32_t)i;   /* PE11 TIM1_CH2 */
    }
}

void EXTI0_Config(void)
{
    /* SYSCFG laikrodis – be jo EXTI0 negalima prijungti prie PA0 */
    RCC_APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* EXTICR1 bitai 3:0 = 0000 reiškia portą A, piną 0 */
    SYSCFG_EXTICR1 &= ~(0xFUL << 0);

    /* Ir pakilimo, ir nusileidimo kraštas */
    EXTI_RTSR |= (1UL << 0);
    EXTI_FTSR |= (1UL << 0);

    /* Leidžiame linijai 0 kviesti pertraukimą */
    EXTI_IMR |= (1UL << 0);

    /* NVIC: įjungiame IRQ numerį 6 (EXTI0) */
    NVIC_ISER0 = (1UL << EXTI0_IRQn);
}

void Init_Tim_Gpio(void)
{
    /* GPIOE laikrodis */
    RCC_AHBENR |= RCC_AHBENR_GPIOEEN;

    /* ----- PE9 = AF2 = TIM1_CH1 (LD3) ----- */
    GPIOE_MODER   &= ~(3UL << (9 * 2));
    GPIOE_MODER   |=  (2UL << (9 * 2));     /* AF */
    GPIOE_OSPEEDR &= ~(3UL << (9 * 2));
    GPIOE_OSPEEDR |=  (2UL << (9 * 2));     /* medium speed */
    GPIOE_OTYPER  &= ~(1UL << 9);
    GPIOE_PUPDR   &= ~(3UL << (9 * 2));
    GPIOE_AFRH    &= ~(0xFUL << ((9 - 8) * 4));
    GPIOE_AFRH    |=  (2UL   << ((9 - 8) * 4)); /* AF2 */

    /* ----- PE11 = AF2 = TIM1_CH2 (LD7) ----- */
    GPIOE_MODER   &= ~(3UL << (11 * 2));
    GPIOE_MODER   |=  (2UL << (11 * 2));
    GPIOE_OSPEEDR &= ~(3UL << (11 * 2));
    GPIOE_OSPEEDR |=  (2UL << (11 * 2));
    GPIOE_OTYPER  &= ~(1UL << 11);
    GPIOE_PUPDR   &= ~(3UL << (11 * 2));
    GPIOE_AFRH    &= ~(0xFUL << ((11 - 8) * 4));
    GPIOE_AFRH    |=  (2UL   << ((11 - 8) * 4)); /* AF2 */
}

void Init_Tim(int dutty)
{
    RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;  /* TIM1 laikrodis */

    TIM1_CR1 = 0;

    TIM1_PSC = 0x0F;          /* /16 */
    TIM1_ARR = PWM_PERIOD;    /* periodas 1000 */

    /* Preload CH1 ir CH2 */
    TIM1_CCMR1 |= (1UL << 3);   /* OC1PE */
    TIM1_CCMR1 |= (1UL << 11);  /* OC2PE */

    TIM1_CR1 |= (1UL << 7);     /* ARPE */

    /* PWM mode 1: OC1M = 110, OC2M = 110 */
    TIM1_CCMR1 &= ~((7UL << 4) | (7UL << 12));
    TIM1_CCMR1 |=  (6UL << 4);    /* CH1 PWM1 */
    TIM1_CCMR1 |=  (6UL << 12);   /* CH2 PWM1 */

    if (dutty < 0)          dutty = 0;
    if (dutty > PWM_PERIOD) dutty = PWM_PERIOD;
    TIM1_CCR1 = (uint32_t)dutty;
    TIM1_CCR2 = (uint32_t)dutty;

    TIM1_EGR = (1UL << 0);      /* UG – perkrauti registrus */

    TIM1_BDTR |= (1UL << 15);   /* MOE – privaloma TIM1 */
    TIM1_CCER |= (1UL << 0);    /* CC1E  PE9  */
    TIM1_CCER |= (1UL << 4);    /* CC2E  PE11 */
    TIM1_CR1  |= (1UL << 0);    /* CEN start */
}

int main(void)
{
    /* Mygtukas PA0 = įėjimas */
    RCC_AHBENR |= RCC_AHBENR_GPIOAEN;
    GPIOA_MODER &= ~(3UL << 0);     /* input */
    GPIOA_PUPDR &= ~(3UL << 0);     /* plokštėje jau pulldown */

    /* PE15 = paprastas LED (LD6), ne PWM */
    RCC_AHBENR |= RCC_AHBENR_GPIOEEN;
    GPIOE_MODER &= ~(3UL << (15 * 2));
    GPIOE_MODER |=  (1UL << (15 * 2));  /* output */
    GPIOE_OTYPER &= ~(1UL << 15);
    GPIOE_ODR &= ~(1UL << 15);          /* gesęs */

    Init_Tim_Gpio();   /* PE9 ir PE11 → PWM */
    Init_Tim(100);     /* start 10 % */
    EXTI0_Config();    /* PA0 pertraukimas */

    while (1) {
        /* Ilgas tuščias darbas. PWM eina hardware, mygtukas – per IRQ. */
        for (volatile uint32_t n = 0; n < 100000UL; n++) {
            __asm volatile ("nop");
        }
    }
}
