#include <stdint.h>   /* Reikia, kad galėtume naudoti tipus uint32_t, int ir pan. */

/* Čia įsimename, kur procesoriaus atmintyje gyvena RCC, GPIOE ir TIM1 registrai.
   Tai ne kintamieji – tai adresai „namų“, kuriuos valdysime. */
#define RCC_BASE    0x40021000UL   /* RCC pradžios adresas – laikrodžių jungiklis */
#define GPIOE_BASE  0x48001000UL   /* GPIOE pradžios adresas – porto E kojai (čia sėdi LED) */
#define TIM1_BASE   0x40012C00UL   /* TIM1 pradžios adresas – laikmatis, kuris darys PWM */

/* Konkretūs registrai: paimame adresą ir sakome „čia 32 bitų dėžutė, kurią galima skaityti/rašyti“. */
#define RCC_AHBENR   (*(volatile uint32_t *)(RCC_BASE + 0x14))  /* Jungia GPIO prievadų laikrodžius */
#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE + 0x18))  /* Jungia TIM1 laikrodį */

#define GPIOE_MODER    (*(volatile uint32_t *)(GPIOE_BASE + 0x00)) /* Kiekvieno PE pin režimas: įėjimas/išėjimas/AF */
#define GPIOE_OTYPER   (*(volatile uint32_t *)(GPIOE_BASE + 0x04)) /* Išėjimo tipas: push-pull ar open-drain */
#define GPIOE_OSPEEDR  (*(volatile uint32_t *)(GPIOE_BASE + 0x08)) /* Išėjimo greitis */
#define GPIOE_PUPDR    (*(volatile uint32_t *)(GPIOE_BASE + 0x0C)) /* Ar reikia pull-up / pull-down rezistoriaus */
#define GPIOE_AFRH     (*(volatile uint32_t *)(GPIOE_BASE + 0x24)) /* Kuriai periferijai priklauso pinai 8–15 */

#define TIM1_CR1    (*(volatile uint32_t *)(TIM1_BASE + 0x00)) /* Pagrindinis TIM1 valdymas (start/stop) */
#define TIM1_EGR    (*(volatile uint32_t *)(TIM1_BASE + 0x14)) /* Rankinis „atnaujink registrus dabar“ signalas */
#define TIM1_CCMR1  (*(volatile uint32_t *)(TIM1_BASE + 0x18)) /* 1 ir 2 kanalų režimas (čia įjungiam PWM) */
#define TIM1_CCER   (*(volatile uint32_t *)(TIM1_BASE + 0x20)) /* Ar kanalo išėjimas paleistas į koją */
#define TIM1_PSC    (*(volatile uint32_t *)(TIM1_BASE + 0x28)) /* Daliklis – sulėtina laikmatį */
#define TIM1_ARR    (*(volatile uint32_t *)(TIM1_BASE + 0x2C)) /* Iki kelinto skaičiaus skaičiuoja (periodas) */
#define TIM1_CCR1   (*(volatile uint32_t *)(TIM1_BASE + 0x34)) /* Iki kelinto skaičiaus LED dega (impulso plotis) */
#define TIM1_BDTR   (*(volatile uint32_t *)(TIM1_BASE + 0x44)) /* TIM1 „didysis jungiklis“ išėjimams (MOE) */

/* Bitų kaukės – tai tiesiog „kurį jungikliuką registre paspausti“. */
#define RCC_AHBENR_GPIOEEN   (1UL << 21) /* 21-as bitas: įjungti GPIOE laikrodį */
#define RCC_APB2ENR_TIM1EN   (1UL << 11) /* 11-as bitas: įjungti TIM1 laikrodį */
#define TIM_EGR_UG           (1UL << 0)  /* 0 bitas: „update“ – perkrauti PSC/ARR/CCR */
#define TIM_CCMR1_OC1PE      (1UL << 3)  /* 3 bitas: CCR1 keičiasi tik periodo pabaigoje (be glitch) */
#define TIM_CCER_CC1E        (1UL << 0)  /* 0 bitas: paleisti 1 kanalo išėjimą į koją */
#define TIM_BDTR_MOE         (1UL << 15) /* 15 bitas: TIM1 visus PWM išėjimus ATRIŠTI (be šito LED nedegs) */
#define TIM_CR1_CEN          (1UL << 0)  /* 0 bitas: paleisti laikmatį skaičiuoti */
#define TIM_CR1_ARPE         (1UL << 7)  /* 7 bitas: ARR irgi su preload (švariau keičiant) */

int i = 0;           /* Dabartinis ryškumas: 0 = visai tamsu, 100 = visai šviesu */
int direction = 1;   /* +1 = ryškėja, -1 = gęsta */

/* Mažas uždelsimas. Procesorius sukasi ratu ir nieko naudingo nedaro. */
static void delay_ms(uint32_t ms)
{
    /* Išorinis ciklas: kiek milisekundžių laukti. */
    for (uint32_t t = 0; t < ms; t++) {
        /* Vidinis ciklas: ~1 ms, jei MCU eina ~8 MHz.
           Jei pas tave 72 MHz, skaičių 800 padidink iki ~7200. */
        for (volatile uint32_t n = 0; n < 800; n++) {
            __asm volatile ("nop");   /* „nieko nedaryk“ 1 taktas – kad kompiliatorius ciklo neišmestų */
        }
    }
}

/* 1 užduotis: įjungti reikalingą periferiją per RCC. */
static void rcc_enable(void)
{
    RCC_AHBENR  |= RCC_AHBENR_GPIOEEN;  /* Duodame laikrodį portui E, kitaip GPIOE registrai „negyvi“ */
    RCC_APB2ENR |= RCC_APB2ENR_TIM1EN;  /* Duodame laikrodį TIM1, kitaip laikmatis neveiks */
}

/* 2 užduotis: GPIO nustatyti taip, kad kojas PE9 priklausytų taimerio PWM, o ne rankiniam ON/OFF. */
static void gpio_led_af_tim1(void)
{
    /* PE9 yra LD3 (raudonas diodas ant Discovery).
       Kiekvienam pinui MODER turi 2 bitus, todėl slenkame 9*2. */

    GPIOE_MODER   &= ~(3UL << (9 * 2)); /* Pirmiausia išvalome PE9 2 bitus (nustatome į 00) */
    GPIOE_MODER   |=  (2UL << (9 * 2)); /* Tada rašome 10 = Alternate Function (ne paprastas GPIO) */

    GPIOE_OTYPER  &= ~(1UL << 9);       /* 0 = push-pull: pinas moka ir įjungti, ir išjungti tvirtai */

    GPIOE_OSPEEDR |=  (3UL << (9 * 2)); /* 11 = didelis greitis – PWM kraštams geriau */

    GPIOE_PUPDR   &= ~(3UL << (9 * 2)); /* 00 = jokių vidinių pull-up/pull-down */

    /* AFRH valdo pinus 8–15. PE9 yra antras nibble (bitai 7:4).
       Skaičius 2 reiškia AF2, o AF2 ant PE9 yra TIM1_CH1. */
    GPIOE_AFRH    &= ~(0xFUL << ((9 - 8) * 4)); /* Išvalome 4 AF bitus PE9 */
    GPIOE_AFRH    |=  (2UL   << ((9 - 8) * 4)); /* Įrašome AF2 = TIM1 */
}

/* 3 užduotis: TIM1 paleisti PWM režimu. Periodas FIKSUOTAS, keisis tik plotis. */
static void timer_pwm_init(void)
{
    TIM1_CR1   = 0;     /* Išjungiame viską, pradedame nuo švaraus laikmačio */

    /* Takto daliklis. Formulė: timer_Hz = SYSCLK / (PSC+1).
       8 MHz / (7+1) = 1 MHz. Vienas tiko = 1 mikrosekundė. */
    TIM1_PSC   = 7;

    /* Periodas. Skaičiuoja 0,1,2,...99 ir vėl iš 0.
       1 MHz / 100 = 10 kHz PWM – akis mirgėjimo nemato. */
    TIM1_ARR   = 99;

    TIM1_CCR1  = 0;     /* Pradžioje LED gesęs: dega 0 tikų iš 100 */

    /* CCMR1 1 kanalui:
       OC1M = 110 (bitai 6:4) = PWM mode 1
         reiškia: kol CNT < CCR1 – išėjimas 1 (LED dega),
                  kai CNT >= CCR1 – išėjimas 0 (LED gęsta).
       OC1PE = 1 – naują CCR1 paima tik periodo pabaigoje. */
    TIM1_CCMR1 = (6UL << 4) | TIM_CCMR1_OC1PE;

    TIM1_CCER  = TIM_CCER_CC1E;  /* Prijungiame 1 kanalą prie fizinio pino */

    TIM1_BDTR  = TIM_BDTR_MOE;   /* TIM1 be šito bitelio PWM į koją NEEINA. Labai dažna klaida. */

    TIM1_EGR   = TIM_EGR_UG;     /* Vieną kartą „stumtelime“ naujas PSC/ARR/CCR reikšmes į darbą */

    TIM1_CR1   = TIM_CR1_ARPE | TIM_CR1_CEN; /* ARPE = ARR su preload, CEN = start skaičiuoti */
}

/* 4 užduotis: pakeisti impulso plotį (duty). Periodo NELIEČIAME. */
static void TimerFunction(int duty)
{
    if (duty < 0)   duty = 0;     /* Apsauga: mažiau už 0 nebūna */
    if (duty > 100) duty = 100;   /* Apsauga: daugiau už 100 nebūna */
    TIM1_CCR1 = (uint32_t)duty;   /* Kiek tikų iš 100 LED dega. 50 ≈ pusiau šviesu */
}

int main(void)
{
    rcc_enable();        /* Įjungiame GPIOE ir TIM1 laikrodžius */
    gpio_led_af_tim1();  /* PE9 atiduodame taimerio PWM */
    timer_pwm_init();    /* Paleidžiame PWM su periodu 100 */

    i = 0;               /* Pradedame nuo tamsos */
    direction = 1;       /* Pirma kryptis – ryškėti */

    /* 4+5 užduotis: begalinis ciklas keičia plotį, todėl diodas lėtai ryškėja ir gęsta */
    while (1) {
        TimerFunction(i); /* Nustatome dabartinį ryškumą */
        delay_ms(15);     /* Trumpa pauzė, kad akis spėtų pamatyti kitimą */

        i += direction;   /* Žengiame vienu laipteliu į priekį arba atgal */

        if (i >= 100) {   /* Pasiekėme maksimumą */
            i = 100;      /* Nepereiti per 100 */
            direction = -1; /* Dabar gęsta */
        } else if (i <= 0) { /* Pasiekėme minimumą */
            i = 0;        /* Nepereiti po 0 */
            direction = 1;  /* Dabar vėl ryškėja */
        }
    }
}
