/*
 * paw3395.c  —  PixArt PAW3395DM-T6QU driver for STM32 HAL
 * SPI1 must be Mode 3 (CPOL=High, CPHA=2 Edge). See paw3395.h for pin map.
 *
 * delay_us() values are CONSERVATIVE for robust first bring-up; tighten to the
 * datasheet minimums later for speed.
 */

#include "paw3395.h"

extern SPI_HandleTypeDef hspi1;

/* ==================== Microsecond delay (DWT counter) ==================== */
void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) { }
}

/* ============================= NCS helpers ============================== */
static inline void ncs_low(void)  { HAL_GPIO_WritePin(NCS_GPIO_Port, NCS_Pin, GPIO_PIN_RESET); }
static inline void ncs_high(void) { HAL_GPIO_WritePin(NCS_GPIO_Port, NCS_Pin, GPIO_PIN_SET);   }

/* ============================ Register read ============================= */
uint8_t paw3395_read_reg(uint8_t addr)
{
    uint8_t tx_addr = (uint8_t)(addr & 0x7FU);
    uint8_t tx_dummy = 0x00U;
    uint8_t rx_discard = 0x00U;
    uint8_t rx_data = 0x00U;

    ncs_low();
    delay_us(1);

    /* Use full-duplex transfers so every received byte is drained. */
    if (HAL_SPI_TransmitReceive(&hspi1, &tx_addr, &rx_discard, 1, 100) != HAL_OK) {
        ncs_high();
        delay_us(20);
        return 0x00U;
    }

    delay_us(4);                 /* Conservative tSRAD */

    if (HAL_SPI_TransmitReceive(&hspi1, &tx_dummy, &rx_data, 1, 100) != HAL_OK) {
        ncs_high();
        delay_us(20);
        return 0x00U;
    }

    delay_us(1);
    ncs_high();
    delay_us(20);                /* Conservative spacing between accesses */

    return rx_data;
}

/* ============================ Register write ============================ */
void paw3395_write_reg(uint8_t addr, uint8_t data)
{
    uint8_t tx[2] = { (uint8_t)(addr | 0x80U), data };
    uint8_t rx[2] = { 0x00U, 0x00U };

    ncs_low();
    delay_us(1);

    /* Drain the simultaneously received bytes to avoid a stale RX/OVR state. */
    (void)HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);

    delay_us(1);
    ncs_high();
    delay_us(35);                /* Conservative spacing between accesses */
}

/* =========================== Motion Burst read ========================== *
 * Burst byte layout:
 *   [0] Motion
 *   [1] Observation
 *   [2] Delta X low
 *   [3] Delta X high
 *   [4] Delta Y low
 *   [5] Delta Y high
 *   [6] SQUAL
 *   [7] Raw Data Sum
 *   [8] Maximum Raw Data
 *   [9] Minimum Raw Data
 *  [10] Shutter low
 *  [11] Shutter high
 *
 * Important: do not write 0x00 to the Motion Burst register before every
 * read. Start each fresh burst by lowering NCS and sending address 0x16.
 */
HAL_StatusTypeDef paw3395_read_motion_data(paw3395_motion_t *data)
{
    uint8_t addr = PAW_REG_MOTION_BURST;
    uint8_t addr_rx = 0x00U;
    uint8_t dummy[PAW3395_MOTION_BURST_SIZE] = {0};
    uint8_t buf[PAW3395_MOTION_BURST_SIZE] = {0};
    HAL_StatusTypeDef status;

    if (data == NULL) {
        return HAL_ERROR;
    }

    /* Return deterministic zeros if either SPI operation fails. */
    *data = (paw3395_motion_t){0};

    ncs_low();
    delay_us(1);                 /* tNCS-SCLK */

    /* Send the read address and drain the simultaneously received byte. */
    status = HAL_SPI_TransmitReceive(&hspi1, &addr, &addr_rx, 1, 100);
    if (status != HAL_OK) {
        ncs_high();
        delay_us(1);
        return status;
    }

    delay_us(4);                 /* Conservative tSRAD_MOTBR */

    /* Clock out all 12 burst bytes while holding MOSI low. */
    status = HAL_SPI_TransmitReceive(&hspi1,
                                     dummy,
                                     buf,
                                     PAW3395_MOTION_BURST_SIZE,
                                     100);

    delay_us(1);                 /* Last SCLK edge to NCS high */
    ncs_high();
    delay_us(1);                 /* tBEXIT */

    if (status != HAL_OK) {
        return status;
    }

    for (uint32_t i = 0; i < PAW3395_MOTION_BURST_SIZE; i++) {
        data->raw[i] = buf[i];
    }

    data->motion      = buf[0];
    data->observation = buf[1];
    data->dx           = (int16_t)(((uint16_t)buf[3] << 8) | (uint16_t)buf[2]);
    data->dy           = (int16_t)(((uint16_t)buf[5] << 8) | (uint16_t)buf[4]);
    data->squal        = buf[6];
    data->raw_sum      = buf[7];
    data->max_raw      = buf[8];
    data->min_raw      = buf[9];
    data->shutter      = (uint16_t)(((uint16_t)buf[11] << 8) | (uint16_t)buf[10]);

    return HAL_OK;
}


/* ================= Individual-register diagnostic read ================== *
 * Reading Motion first latches one motion snapshot. The following reads then
 * retrieve the corresponding signed X/Y deltas and SQUAL without relying on
 * Motion Burst framing. This is intentionally slower, but ideal for bring-up.
 */
HAL_StatusTypeDef paw3395_read_motion_registers(paw3395_motion_t *data)
{
    uint8_t xl;
    uint8_t xh;
    uint8_t yl;
    uint8_t yh;

    if (data == NULL) {
        return HAL_ERROR;
    }

    *data = (paw3395_motion_t){0};

    data->motion = paw3395_read_reg(PAW_REG_MOTION);
    xl = paw3395_read_reg(PAW_REG_DELTA_X_L);
    xh = paw3395_read_reg(PAW_REG_DELTA_X_H);
    yl = paw3395_read_reg(PAW_REG_DELTA_Y_L);
    yh = paw3395_read_reg(PAW_REG_DELTA_Y_H);
    data->squal = paw3395_read_reg(PAW_REG_SQUAL);
    data->observation = paw3395_read_reg(PAW_REG_OBSERVATION);

    data->dx = (int16_t)(((uint16_t)xh << 8) | (uint16_t)xl);
    data->dy = (int16_t)(((uint16_t)yh << 8) | (uint16_t)yl);

    /* Mirror the normal burst byte positions so one print routine can be used
     * for either acquisition method.
     */
    data->raw[0] = data->motion;
    data->raw[1] = data->observation;
    data->raw[2] = xl;
    data->raw[3] = xh;
    data->raw[4] = yl;
    data->raw[5] = yh;
    data->raw[6] = data->squal;

    return HAL_OK;
}

/* =================== Motion read compatibility wrappers ================= */
void paw3395_read_motion(int16_t *dx, int16_t *dy, uint8_t *squal)
{
    paw3395_motion_t sample;

    if ((dx == NULL) || (dy == NULL) || (squal == NULL)) {
        return;
    }

    /* Use the slower individual-register path by default during bring-up.
     * Once burst output is verified, applications can call
     * paw3395_read_motion_data() or paw3395_read_burst() directly.
     */
    if (paw3395_read_motion_registers(&sample) != HAL_OK) {
        *dx = 0;
        *dy = 0;
        *squal = 0;
        return;
    }

    *dx = sample.dx;
    *dy = sample.dy;
    *squal = sample.squal;
}

void paw3395_read_burst(int16_t *dx,
                        int16_t *dy,
                        uint8_t *squal,
                        uint8_t *motion)
{
    paw3395_motion_t sample;

    if ((dx == NULL) || (dy == NULL) ||
        (squal == NULL) || (motion == NULL)) {
        return;
    }

    if (paw3395_read_motion_data(&sample) != HAL_OK) {
        *dx = 0;
        *dy = 0;
        *squal = 0;
        *motion = 0;
        return;
    }

    *dx = sample.dx;
    *dy = sample.dy;
    *squal = sample.squal;
    *motion = sample.motion;
}

/* ======================= Power-up init sequence ======================== *
 *
 * Adapted from Krillleeee/paw3395-module, src/paw3395_priv.c and
 * src/paw3395.c. The reference implementation is licensed under MIT:
 *
 *   Copyright (c) 2022 The ZMK Contributors
 *   SPDX-License-Identifier: MIT
 *
 * The first 137 writes must be followed by a timed poll of register 0x6C.
 * The final five writes are applied only after that poll and its conditional
 * fallback have completed.
 */
#define PAW_INIT_STAGE1_COUNT       137U
#define PAW_INIT_POLL_REG           0x6CU
#define PAW_INIT_POLL_READY_VALUE   0x80U
#define PAW_INIT_POLL_ATTEMPTS      60U

static const paw_reg_t paw3395_init_regs[] = {
    { 0x7F, 0x07 },
    { 0x40, 0x41 },
    { 0x7F, 0x00 },
    { 0x40, 0x80 },
    { 0x7F, 0x0E },
    { 0x55, 0x0D },
    { 0x56, 0x1B },
    { 0x57, 0xE8 },
    { 0x58, 0xD5 },
    { 0x7F, 0x14 },
    { 0x42, 0xBC },
    { 0x43, 0x74 },
    { 0x4B, 0x20 },
    { 0x4D, 0x00 },
    { 0x53, 0x0E },
    { 0x7F, 0x05 },
    { 0x44, 0x04 },
    { 0x4D, 0x06 },
    { 0x51, 0x40 },
    { 0x53, 0x40 },
    { 0x55, 0xCA },
    { 0x5A, 0xE8 },
    { 0x5B, 0xEA },
    { 0x61, 0x31 },
    { 0x62, 0x64 },
    { 0x6D, 0xB8 },
    { 0x6E, 0x0F },
    { 0x70, 0x02 },
    { 0x4A, 0x2A },
    { 0x60, 0x26 },
    { 0x7F, 0x06 },
    { 0x6C, 0x70 },
    { 0x6D, 0x60 },
    { 0x6E, 0x04 },
    { 0x53, 0x02 },
    { 0x55, 0x11 },
    { 0x7A, 0x01 },
    { 0x7D, 0x51 },
    { 0x7F, 0x07 },
    { 0x41, 0x10 },
    { 0x42, 0x32 },
    { 0x43, 0x00 },
    { 0x7F, 0x08 },
    { 0x71, 0x4F },
    { 0x7F, 0x09 },
    { 0x62, 0x1F },
    { 0x63, 0x1F },
    { 0x65, 0x03 },
    { 0x66, 0x03 },
    { 0x67, 0x1F },
    { 0x68, 0x1F },
    { 0x69, 0x03 },
    { 0x6A, 0x03 },
    { 0x6C, 0x1F },
    { 0x6D, 0x1F },
    { 0x51, 0x04 },
    { 0x53, 0x20 },
    { 0x54, 0x20 },
    { 0x71, 0x0C },
    { 0x72, 0x07 },
    { 0x73, 0x07 },
    { 0x7F, 0x0A },
    { 0x4A, 0x14 },
    { 0x4C, 0x14 },
    { 0x55, 0x19 },
    { 0x7F, 0x14 },
    { 0x4B, 0x30 },
    { 0x4C, 0x03 },
    { 0x61, 0x0B },
    { 0x62, 0x0A },
    { 0x63, 0x02 },
    { 0x7F, 0x15 },
    { 0x4C, 0x02 },
    { 0x56, 0x02 },
    { 0x41, 0x91 },
    { 0x4D, 0x0A },
    { 0x7F, 0x0C },
    { 0x4A, 0x10 },
    { 0x4B, 0x0C },
    { 0x4C, 0x40 },
    { 0x41, 0x25 },
    { 0x55, 0x18 },
    { 0x56, 0x14 },
    { 0x49, 0x0A },
    { 0x42, 0x00 },
    { 0x43, 0x2D },
    { 0x44, 0x0C },
    { 0x54, 0x1A },
    { 0x5A, 0x0D },
    { 0x5F, 0x1E },
    { 0x5B, 0x05 },
    { 0x5E, 0x0F },
    { 0x7F, 0x0D },
    { 0x48, 0xDD },
    { 0x4F, 0x03 },
    { 0x52, 0x49 },
    { 0x51, 0x00 },
    { 0x54, 0x5B },
    { 0x53, 0x00 },
    { 0x56, 0x64 },
    { 0x55, 0x00 },
    { 0x58, 0xA5 },
    { 0x57, 0x02 },
    { 0x5A, 0x29 },
    { 0x5B, 0x47 },
    { 0x5C, 0x81 },
    { 0x5D, 0x40 },
    { 0x71, 0xDC },
    { 0x70, 0x07 },
    { 0x73, 0x00 },
    { 0x72, 0x08 },
    { 0x75, 0xDC },
    { 0x74, 0x07 },
    { 0x77, 0x00 },
    { 0x76, 0x08 },
    { 0x7F, 0x10 },
    { 0x4C, 0xD0 },
    { 0x7F, 0x00 },
    { 0x4F, 0x63 },
    { 0x4E, 0x00 },
    { 0x52, 0x63 },
    { 0x51, 0x00 },
    { 0x54, 0x54 },
    { 0x5A, 0x10 },
    { 0x77, 0x4F },
    { 0x47, 0x01 },
    { 0x5B, 0x40 },
    { 0x64, 0x60 },
    { 0x65, 0x06 },
    { 0x66, 0x13 },
    { 0x67, 0x0F },
    { 0x78, 0x01 },
    { 0x79, 0x9C },
    { 0x40, 0x00 },
    { 0x55, 0x02 },
    { 0x23, 0x70 },
    { 0x22, 0x01 },

    /* Stage 3: written after the 0x6C poll/fallback step. */
    { 0x22, 0x00 },
    { 0x55, 0x00 },
    { 0x7F, 0x07 },
    { 0x40, 0x40 },
    { 0x7F, 0x00 },
};

static const paw_reg_t paw3395_init_fallback_regs[] = {
    { 0x7F, 0x14 },
    { 0x6C, 0x00 },
    { 0x7F, 0x00 },
};

/* High-performance run-mode setup from the same upstream driver. */
static const paw_reg_t paw3395_hp_mode_regs[] = {
    { 0x7F, 0x05 },
    { 0x51, 0x40 },
    { 0x53, 0x40 },
    { 0x61, 0x31 },
    { 0x6E, 0x0F },
    { 0x7F, 0x07 },
    { 0x42, 0x32 },
    { 0x43, 0x00 },
    { 0x7F, 0x0D },
    { 0x51, 0x00 },
    { 0x52, 0x49 },
    { 0x53, 0x00 },
    { 0x54, 0x5B },
    { 0x55, 0x00 },
    { 0x56, 0x64 },
    { 0x57, 0x02 },
    { 0x58, 0xA5 },
    { 0x7F, 0x00 },
    { 0x54, 0x54 },
    { 0x78, 0x01 },
    { 0x79, 0x9C },
};

static uint8_t paw3395_last_init_poll_value = 0x00;
static uint8_t paw3395_last_init_poll_attempts = 0;

#define PAW_INIT_REG_COUNT \
    (sizeof(paw3395_init_regs) / sizeof(paw3395_init_regs[0]))
#define PAW_INIT_FALLBACK_REG_COUNT \
    (sizeof(paw3395_init_fallback_regs) / sizeof(paw3395_init_fallback_regs[0]))
#define PAW_HP_MODE_REG_COUNT \
    (sizeof(paw3395_hp_mode_regs) / sizeof(paw3395_hp_mode_regs[0]))

_Static_assert(PAW_INIT_REG_COUNT == 142U,
               "PAW3395 init sequence must contain 142 normal writes");
_Static_assert(PAW_INIT_STAGE1_COUNT == 137U,
               "PAW3395 stage-1 boundary must remain at 137 writes");

static void paw3395_write_table(const paw_reg_t *table, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        paw3395_write_reg(table[i].addr, table[i].val);
    }
}

void paw3395_power_up_sequence(void)
{
    uint8_t status = 0x00;
    paw3395_last_init_poll_value = 0x00;
    paw3395_last_init_poll_attempts = 0;

    /* Stage 1: write the first 137 register/value pairs. */
    paw3395_write_table(paw3395_init_regs, PAW_INIT_STAGE1_COUNT);

    /* Stage 2: wait 1 ms before each read and poll at most 60 times. */
    for (uint32_t attempt = 0; attempt < PAW_INIT_POLL_ATTEMPTS; attempt++) {
        HAL_Delay(1);
        status = paw3395_read_reg(PAW_INIT_POLL_REG);
        paw3395_last_init_poll_value = status;
        paw3395_last_init_poll_attempts = (uint8_t)(attempt + 1U);
        if (status == PAW_INIT_POLL_READY_VALUE) {
            break;
        }
    }

    /* Apply the upstream fallback when the ready value never appears. */
    if (status != PAW_INIT_POLL_READY_VALUE) {
        paw3395_write_table(paw3395_init_fallback_regs,
                            PAW_INIT_FALLBACK_REG_COUNT);
    }

    /* Stage 3: write the final five register/value pairs. */
    paw3395_write_table(&paw3395_init_regs[PAW_INIT_STAGE1_COUNT],
                        PAW_INIT_REG_COUNT - PAW_INIT_STAGE1_COUNT);
}

void paw3395_set_cpi(uint16_t cpi)
{
    if (cpi < 50U) {
        cpi = 50U;
    } else if (cpi > 26000U) {
        cpi = 26000U;
    }

    /* Round down to the nearest supported 50-CPI step. */
    cpi = (uint16_t)((cpi / 50U) * 50U);
    uint16_t value = (uint16_t)((cpi / 50U) - 1U);
    uint8_t low = (uint8_t)(value & 0xFFU);
    uint8_t high = (uint8_t)(value >> 8);

    /* Match the reference driver: program and latch X, then Y. */
    paw3395_write_reg(PAW_REG_RES_X_LOW, low);
    paw3395_write_reg(PAW_REG_RES_X_HIGH, high);
    paw3395_write_reg(PAW_REG_SET_RESOLUTION, 0x01U);

    paw3395_write_reg(PAW_REG_RES_Y_LOW, low);
    paw3395_write_reg(PAW_REG_RES_Y_HIGH, high);
    paw3395_write_reg(PAW_REG_SET_RESOLUTION, 0x01U);
}

uint8_t paw3395_get_init_poll_value(void)
{
    return paw3395_last_init_poll_value;
}

uint8_t paw3395_get_init_poll_attempts(void)
{
    return paw3395_last_init_poll_attempts;
}

static void paw3395_configure_slide_test(void)
{
    paw3395_write_table(paw3395_hp_mode_regs, PAW_HP_MODE_REG_COUNT);

    /* Select HP mode (bits 1:0 = 00) and disable rest modes for testing. */
    uint8_t performance = paw3395_read_reg(PAW_REG_PERFORMANCE);
    performance &= (uint8_t)~0x03U;
    performance |= 0x80U;
    paw3395_write_reg(PAW_REG_PERFORMANCE, performance);

    /* A moderate CPI makes hand-sliding easy to observe without saturation. */
    paw3395_set_cpi(1600U);
}

/* ============================ Init / bring-up =========================== */
uint8_t paw3395_init(void)
{
    /* Reset the sensor SPI port before issuing the software reset. */
    ncs_high();
    delay_us(2);
    ncs_low();
    delay_us(2);
    ncs_high();
    delay_us(2);

    paw3395_write_reg(PAW_REG_POWER_UP_RESET, PAW_POWER_UP_RESET_VAL);
    HAL_Delay(50);

    /* Product ID proves the basic SPI path before the long register upload. */
    uint8_t id = paw3395_read_reg(PAW_REG_PRODUCT_ID);
    if (id != PAW_PRODUCT_ID_EXPECTED) {
        return id;
    }

    paw3395_power_up_sequence();

    /* The reference driver waits before mode/CPI configuration. */
    HAL_Delay(10);
    paw3395_configure_slide_test();
    HAL_Delay(1);

    /* Required after initialization regardless of the Motion bit state. */
    for (uint8_t reg = PAW_REG_MOTION; reg <= PAW_REG_DELTA_Y_H; reg++) {
        (void)paw3395_read_reg(reg);
    }

    return paw3395_read_reg(PAW_REG_PRODUCT_ID);
}
