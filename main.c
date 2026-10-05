#include <stdint.h>

/*
 * Laboras: LSM303DLHC (akselerometras + magnetometras) per I2C1,
 * duomenys i kompiuteri per USART1 (tas pats ST-LINK USB, 115200).
 *
 * Kaip zinoti, kad veikia:
 * 1. Sukompiliuoja be klaidu ir irasosi i plokste.
 * 2. Terminalas (115200 8N1) isveda "ACC OK" ir "MAG OK".
 *    Jei "FAIL" - I2C nepasieke sensoriaus.
 * 3. Toliau eina eilutes "A ax ay az M mx my mz".
 * 4. Plokste ant stalo: az apie 1000 arba -1000, ax ir ay arti 0.
 *    Pakreipus asys pasikeicia.
 * 5. Sukant horizontaliai, mx ir my keiciasi.
 */

#define REG32(a) (*(volatile uint32_t *)(a))

/* RCC: takto valdiklis */
#define RCC_BASE    0x40021000UL
#define RCC_CR      REG32(RCC_BASE + 0x00) /* HSI ijunkimas ir ready */
#define RCC_CFGR    REG32(RCC_BASE + 0x04) /* takto saltinis ir dalikliai */
#define RCC_AHBENR  REG32(RCC_BASE + 0x14) /* GPIO taktai */
#define RCC_APB2ENR REG32(RCC_BASE + 0x18) /* USART1 taktas */
#define RCC_APB1ENR REG32(RCC_BASE + 0x1C) /* I2C1 taktas */

/* PB6 = I2C1_SCL, PB7 = I2C1_SDA */
#define GPIOB_BASE   0x48000400UL
#define GPIOB_MODER  REG32(GPIOB_BASE + 0x00)
#define GPIOB_OTYPER REG32(GPIOB_BASE + 0x04) /* 1 = open-drain, butina I2C */
#define GPIOB_PUPDR  REG32(GPIOB_BASE + 0x0C)
#define GPIOB_AFRL   REG32(GPIOB_BASE + 0x20) /* AF numeris pinams 0..7 */

/* PC4 = USART1_TX, PC5 = USART1_RX, ST-LINK virtualus COM */
#define GPIOC_BASE  0x48000800UL
#define GPIOC_MODER REG32(GPIOC_BASE + 0x00)
#define GPIOC_AFRL  REG32(GPIOC_BASE + 0x20)

#define I2C1_BASE    0x40005400UL
#define I2C1_CR1     REG32(I2C1_BASE + 0x00)
#define I2C1_CR2     REG32(I2C1_BASE + 0x04) /* adresas, baitu skaicius, START */
#define I2C1_TIMINGR REG32(I2C1_BASE + 0x10) /* 100 kHz prie 8 MHz */
#define I2C1_ISR     REG32(I2C1_BASE + 0x18)
#define I2C1_ICR     REG32(I2C1_BASE + 0x1C)
#define I2C1_RXDR    REG32(I2C1_BASE + 0x24)
#define I2C1_TXDR    REG32(I2C1_BASE + 0x28)

#define USART1_BASE 0x40013800UL
#define USART1_CR1  REG32(USART1_BASE + 0x00)
#define USART1_BRR  REG32(USART1_BASE + 0x0C)
#define USART1_ISR  REG32(USART1_BASE + 0x1C)
#define USART1_TDR  REG32(USART1_BASE + 0x28)

#define I2C_PE      (1U << 0)  /* periferija ijungta */
#define I2C_TXIS    (1U << 1)  /* galima rasyti i TXDR */
#define I2C_RXNE    (1U << 2)  /* RXDR turi nauja baita */
#define I2C_NACKF   (1U << 4)  /* sensorius neatsake */
#define I2C_STOPF   (1U << 5)  /* transakcija baigta */
#define I2C_TC      (1U << 6)  /* baitai issiusti, STOP dar ne */
#define I2C_RD_WRN  (1U << 10) /* 1 = skaitymas */
#define I2C_START   (1U << 13)
#define I2C_AUTOEND (1U << 25) /* po N baitu pats siuncia STOP */
#define I2C_NBYTES  16

/* 7 bitu adresas pastumtas kairen: accel 0x19 -> 0x32, mag 0x1E -> 0x3C */
#define ACC_ADDR  0x32
#define MAG_ADDR  0x3C

/* Veikia, jei terminale atsiranda tekstas. */
static void uart_send(uint8_t b)
{
    while ((USART1_ISR & (1U << 7)) == 0) { }
    USART1_TDR = b;
}

static void uart_str(const char *s)
{
    while (*s) uart_send((uint8_t)*s++);
}

/* int16 su minusu. Akseleracija yra two's complement. */
static void uart_i16(int16_t v)
{
    char tmp[6];
    int n = 0;
    uint16_t x = (v < 0) ? (uint16_t)(-v) : (uint16_t)v;
    if (v < 0) uart_send('-');
    do { tmp[n++] = (char)('0' + (x % 10)); x /= 10; } while (x);
    while (n--) uart_send((uint8_t)tmp[n]);
}

/* NACK arba timeout -> 0, main tada raso FAIL. */
static int i2c_wait(uint32_t flag)
{
    uint32_t n = 200000;
    while ((I2C1_ISR & flag) == 0) {
        if (I2C1_ISR & I2C_NACKF) {
            I2C1_ICR = I2C_NACKF;
            return 0;
        }
        if (--n == 0) return 0;
    }
    return 1;
}

/* START, registro adresas, reiksme, STOP. */
static int i2c_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    I2C1_CR2 = addr | (2U << I2C_NBYTES) | I2C_AUTOEND | I2C_START;
    if (!i2c_wait(I2C_TXIS)) return 0;
    I2C1_TXDR = reg;
    if (!i2c_wait(I2C_TXIS)) return 0;
    I2C1_TXDR = val;
    if (!i2c_wait(I2C_STOPF)) return 0;
    I2C1_ICR = I2C_STOPF;
    return 1;
}

/* Registro adresas be STOP, tada repeated START ir len baitu. */
static int i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    I2C1_CR2 = addr | (1U << I2C_NBYTES) | I2C_START;
    if (!i2c_wait(I2C_TXIS)) return 0;
    I2C1_TXDR = reg;
    if (!i2c_wait(I2C_TC)) return 0;

    I2C1_CR2 = addr | ((uint32_t)len << I2C_NBYTES)
             | I2C_AUTOEND | I2C_START | I2C_RD_WRN;
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_wait(I2C_RXNE)) return 0;
        buf[i] = (uint8_t)I2C1_RXDR;
    }
    if (!i2c_wait(I2C_STOPF)) return 0;
    I2C1_ICR = I2C_STOPF;
    return 1;
}

static void clock_gpio_init(void)
{
    /* HSI 8 MHz, dalikliai 1. UART bodai skaiciuoti nuo sito takto. */
    RCC_CR |= 1U;
    while ((RCC_CR & 2U) == 0) { }
    RCC_CFGR &= ~((3U << 0) | (0xFU << 4) | (7U << 8) | (7U << 11));

    RCC_AHBENR |= (1U << 18) | (1U << 19); /* GPIOB, GPIOC */
    RCC_APB1ENR |= (1U << 21);             /* I2C1 */
    RCC_APB2ENR |= (1U << 14);             /* USART1 */

    /* PB6, PB7: AF, open-drain, pull-up, AF4 = I2C1 */
    GPIOB_MODER &= ~((3U << 12) | (3U << 14));
    GPIOB_MODER |= (2U << 12) | (2U << 14);
    GPIOB_OTYPER |= (1U << 6) | (1U << 7);
    GPIOB_PUPDR &= ~((3U << 12) | (3U << 14));
    GPIOB_PUPDR |= (1U << 12) | (1U << 14);
    GPIOB_AFRL &= ~((0xFU << 24) | (0xFU << 28));
    GPIOB_AFRL |= (4U << 24) | (4U << 28);

    /* PC4, PC5: AF7 = USART1 */
    GPIOC_MODER &= ~((3U << 8) | (3U << 10));
    GPIOC_MODER |= (2U << 8) | (2U << 10);
    GPIOC_AFRL &= ~((0xFU << 16) | (0xFU << 20));
    GPIOC_AFRL |= (7U << 16) | (7U << 20);

    /* ST timing 100 kHz, kai I2C taktas 8 MHz */
    I2C1_CR1 &= ~I2C_PE;
    I2C1_TIMINGR = 0x00201D2B;
    I2C1_CR1 |= I2C_PE;

    USART1_CR1 &= ~1U;
    USART1_BRR = 8000000 / 115200;
    USART1_CR1 = (1U << 3) | 1U; /* TE + UE */
}

int main(void)
{
    uint8_t acc[6];
    uint8_t mag[6];

    clock_gpio_init();

    /* 0x57 = 100 Hz, X/Y/Z on. Be sito accel yra power-down ir grazina 0. */
    if (!i2c_write(ACC_ADDR, 0x20, 0x57)) uart_str("ACC FAIL\r\n");
    else uart_str("ACC OK\r\n");

    /* 15 Hz, continuous. Default magnetometras miega. */
    i2c_write(MAG_ADDR, 0x00, 0x10);
    if (!i2c_write(MAG_ADDR, 0x02, 0x00)) uart_str("MAG FAIL\r\n");
    else uart_str("MAG OK\r\n");

    while (1) {
        /* 0x80 = auto-increment. Eile XL XH YL YH ZL ZH, little-endian. */
        i2c_read(ACC_ADDR, 0x28 | 0x80, acc, 6);
        /* Eile XH XL ZH ZL YH YL, big-endian. Z yra pries Y. */
        i2c_read(MAG_ADDR, 0x03, mag, 6);

        int16_t ax = (int16_t)((acc[1] << 8) | acc[0]);
        int16_t ay = (int16_t)((acc[3] << 8) | acc[2]);
        int16_t az = (int16_t)((acc[5] << 8) | acc[4]);
        int16_t mx = (int16_t)((mag[0] << 8) | mag[1]);
        int16_t mz = (int16_t)((mag[2] << 8) | mag[3]);
        int16_t my = (int16_t)((mag[4] << 8) | mag[5]);

        uart_str("A ");
        uart_i16(ax); uart_send(' ');
        uart_i16(ay); uart_send(' ');
        uart_i16(az);
        uart_str(" M ");
        uart_i16(mx); uart_send(' ');
        uart_i16(my); uart_send(' ');
        uart_i16(mz);
        uart_str("\r\n");

        for (volatile uint32_t d = 0; d < 400000; d++) { }
    }
}
