/*
 * paw3395.h  —  PixArt PAW3395DM-T6QU driver for STM32 HAL
 * Target: NUCLEO-F411RE, SPI1 Mode 3, manual NCS on PB6.
 *
 * CubeMX user-label pin map:
 *   SPI1_SCK  = PA5 (D13)
 *   SPI1_MISO = PA6 (D12)
 *   SPI1_MOSI = PA7 (D11)
 *   NCS       = PB6 (D10)
 *   NRESET    = PA9 (D8)
 *   MOTION    = PA8 (D7)
 *   LDO_EN    = PB5 (D4)
 */

#ifndef PAW3395_H
#define PAW3395_H

#include "main.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------- Register addresses ------------------------- */
#define PAW_REG_PRODUCT_ID       0x00U
#define PAW_REG_REVISION_ID      0x01U
#define PAW_REG_MOTION           0x02U
#define PAW_REG_DELTA_X_L        0x03U
#define PAW_REG_DELTA_X_H        0x04U
#define PAW_REG_DELTA_Y_L        0x05U
#define PAW_REG_DELTA_Y_H        0x06U
#define PAW_REG_SQUAL            0x07U
#define PAW_REG_RAW_DATA_SUM     0x08U
#define PAW_REG_MAX_RAW_DATA     0x09U
#define PAW_REG_MIN_RAW_DATA     0x0AU
#define PAW_REG_SHUTTER_L        0x0BU
#define PAW_REG_SHUTTER_H        0x0CU
#define PAW_REG_OBSERVATION      0x15U
#define PAW_REG_MOTION_BURST     0x16U
#define PAW_REG_POWER_UP_RESET   0x3AU
#define PAW_REG_SHUTDOWN         0x3BU
#define PAW_REG_PERFORMANCE      0x40U
#define PAW_REG_SET_RESOLUTION   0x47U
#define PAW_REG_RES_X_LOW        0x48U
#define PAW_REG_RES_X_HIGH       0x49U
#define PAW_REG_RES_Y_LOW        0x4AU
#define PAW_REG_RES_Y_HIGH       0x4BU
#define PAW_REG_ANGLE_SNAP       0x56U
#define PAW_REG_RIPPLE_CONTROL   0x5AU
#define PAW_REG_AXIS_CONTROL     0x5BU
#define PAW_REG_MOTION_CONTROL   0x5CU
#define PAW_REG_INVERSE_ID       0x5FU

#define PAW_PRODUCT_ID_EXPECTED  0x51U
#define PAW_POWER_UP_RESET_VAL   0x5AU
#define PAW3395_MOTION_BURST_SIZE 12U

/* An address/value pair used by the power-up and run-mode tables. */
typedef struct {
    uint8_t addr;
    uint8_t val;
} paw_reg_t;

/* Decoded Motion Burst result plus all raw bytes for debugging. */
typedef struct {
    uint8_t motion;
    uint8_t observation;
    int16_t dx;
    int16_t dy;
    uint8_t squal;
    uint8_t raw_sum;
    uint8_t max_raw;
    uint8_t min_raw;
    uint16_t shutter;
    uint8_t raw[PAW3395_MOTION_BURST_SIZE];
} paw3395_motion_t;

/* ------------------------------- Timing -------------------------------- */
void DWT_Delay_Init(void);
void delay_us(uint32_t us);

/* ---------------------------- Register access --------------------------- */
uint8_t paw3395_read_reg(uint8_t addr);
void paw3395_write_reg(uint8_t addr, uint8_t data);

/* --------------------------- Initialization ----------------------------- */
uint8_t paw3395_init(void);
void paw3395_power_up_sequence(void);
void paw3395_set_cpi(uint16_t cpi);
uint8_t paw3395_get_init_poll_value(void);
uint8_t paw3395_get_init_poll_attempts(void);

/* ----------------------------- Motion data ------------------------------ */
HAL_StatusTypeDef paw3395_read_motion_data(paw3395_motion_t *data);

/* Diagnostic path: reads Motion, X/Y and SQUAL as separate registers.
 * Use this first to distinguish sensor/optics problems from burst framing.
 */
HAL_StatusTypeDef paw3395_read_motion_registers(paw3395_motion_t *data);

/* Compatibility APIs retained for existing main.c code. */
void paw3395_read_motion(int16_t *dx, int16_t *dy, uint8_t *squal);
void paw3395_read_burst(int16_t *dx,
                        int16_t *dy,
                        uint8_t *squal,
                        uint8_t *motion);

#ifdef __cplusplus
}
#endif

#endif /* PAW3395_H */