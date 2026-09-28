/**
 * STM32F3 Discovery
 * 1. Turn ALL 8 LEDs ON
 * 2. Watch the blue USER button
 * 3. If you press the button, turn ALL LEDs OFF
 */

#include <stdint.h>   /* uint32_t = a 32-bit number */

#define RCC_BASE      0x40021000UL   /* clock office address */
#define GPIOA_BASE    0x48000000UL   /* port A address — button */
#define GPIOE_BASE    0x48001000UL   /* port E address — LEDs */

#define RCC_AHBENR    (*(volatile uint32_t *)(RCC_BASE + 0x14))   /* clock on/off page */

#define GPIOA_MODER   (*(volatile uint32_t *)(GPIOA_BASE + 0x00))  /* in or out? */
#define GPIOA_PUPDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))  /* pull up or down? */
#define GPIOA_IDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x10))  /* voltage right now */

#define GPIOE_MODER   (*(volatile uint32_t *)(GPIOE_BASE + 0x00))  /* in or out? */
#define GPIOE_OTYPER  (*(volatile uint32_t *)(GPIOE_BASE + 0x04))  /* push-pull or open-drain? */
#define GPIOE_OSPEEDR (*(volatile uint32_t *)(GPIOE_BASE + 0x08))  /* how fast? */
#define GPIOE_PUPDR   (*(volatile uint32_t *)(GPIOE_BASE + 0x0C))  /* pull up or down? */
#define GPIOE_BSRR    (*(volatile uint32_t *)(GPIOE_BASE + 0x18))  /* turn pin ON or OFF */

#define RCC_AHBENR_GPIOAEN  (1UL << 17)  /* bit 17 wakes port A */
#define RCC_AHBENR_GPIOEEN  (1UL << 21)  /* bit 21 wakes port E */

#define LED_PINS_MASK  (0xFFu << 8)  /* bits 8..15 = eight LED pins */
#define USER_BTN_PIN   (1u << 0)     /* bit 0 = PA0 = blue USER button */

/* Wait. A button bounces. Looking too fast sees fake extra presses. */
static void delay(volatile uint32_t count)
{
    while (count--) {   /* count down to 0 */
        /* do nothing — waste time */
    }
}

/* Homework 1: turn clocks on with RCC */
static void rcc_init(void)
{
    RCC_AHBENR |= RCC_AHBENR_GPIOAEN;  /* wake port A (button) */
    RCC_AHBENR |= RCC_AHBENR_GPIOEEN;  /* wake port E (LEDs) */
    (void)RCC_AHBENR;                  /* read back so the clock really starts */
}

/* Homework 2: LED pins = OUTPUTS */
static void gpio_leds_init(void)
{
    /* 2 bits per pin in MODER: 00=in 01=out 10=special 11=analog
       pins 8..15 use bits 16..31. 0x5555 = 01 repeated 8 times */
    GPIOE_MODER &= ~(0xFFFFu << 16);   /* erase old mode of pins 8..15 */
    GPIOE_MODER |=  (0x5555u << 16);   /* write "output" on every LED pin */

    GPIOE_OTYPER &= ~LED_PINS_MASK;    /* 0 = push-pull: force HIGH and LOW */

    GPIOE_OSPEEDR &= ~(0xFFFFu << 16); /* 00 = slow. LED does not need speed */

    GPIOE_PUPDR &= ~(0xFFFFu << 16);   /* 00 = no pull. We drive the LED */
}

/* Homework 3: button pin = INPUT */
static void gpio_button_init(void)
{
    GPIOA_MODER &= ~(3u << 0);  /* wipe 2 mode bits of pin 0 → 00 = input */

    GPIOA_PUPDR &= ~(3u << 0);  /* wipe 2 pull bits of pin 0 */
    GPIOA_PUPDR |=  (2u << 0);  /* 10 = pull-down: stays 0 until press */
}

/* Light every LED. BSRR bits 0..15: write 1 → pin becomes 1. */
static void leds_on(void)
{
    GPIOE_BSRR = LED_PINS_MASK;  /* PE8..PE15 = 1 → LEDs ON */
}

/* Dark every LED. BSRR bits 16..31: write 1 → pin becomes 0. */
static void leds_off(void)
{
    GPIOE_BSRR = (LED_PINS_MASK << 16);  /* PE8..PE15 = 0 → LEDs OFF */
}

/* Homework 4: read button. 1 = pressed, 0 = not.
   Press connects PA0 to 3.3 V, so bit 0 becomes 1. */
static uint32_t user_button_pressed(void)
{
    return (GPIOA_IDR & USER_BTN_PIN) != 0u;  /* is bit 0 a 1? */
}

int main(void)
{
    rcc_init();          /* clocks ON */
    gpio_leds_init();    /* LED pins = outputs */
    gpio_button_init();  /* button pin = input */
    leds_on();           /* start with ALL lights ON */

    while (1) {   /* never stop — keep watching the button */
        if (user_button_pressed()) {             /* blue button down? */
            delay(200000);                       /* wait, bounce dies */
            if (user_button_pressed()) {         /* still down? real press */
                leds_off();                      /* Homework 5: LEDs OFF */
                while (user_button_pressed()) {  /* wait until finger lets go */
                    /* do nothing */
                }
                delay(200000);                   /* wait after release too */
            }
        }
        /* button not pressed → do nothing */
    }
}
