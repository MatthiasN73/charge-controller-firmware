/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DCDC_H
#define DCDC_H

/** @file
 *
 * @brief DC/DC buck/boost control functions
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus

#include "power_port.h"

/** DC/DC basic operation mode
 *
 * Defines which type of device is connected to the high side and low side ports
 */
enum DcdcOperationMode
{
    MODE_MPPT_BUCK,     ///< solar panel at high side port, battery / load at low side port (typical MPPT)
    MODE_MPPT_BOOST,    ///< battery at high side port, solar panel at low side (e.g. e-bike charging)
    MODE_NANOGRID       ///< accept input power (if available and need for charging) or provide output power
                        ///< (if no other power source on the grid and battery charged) on the high side port
                        ///< and dis/charge battery on the low side port, battery voltage must be lower than
                        ///< nano grid voltage.
};

/** DC/DC control state
 *
 * Allows to determine the current control state (off, CC, CV and MPPT)
 */
enum DcdcControlState
{
    DCDC_STATE_OFF,     ///< DC/DC switched off (low input power available or actively disabled)
    DCDC_STATE_MPPT,    ///< Maximum Power Point Tracking
    DCDC_STATE_CC,      ///< Constant-Current control
    DCDC_STATE_CV,      ///< Constant-Voltage control
    DCDC_STATE_DERATING ///< Hardware-limits (current or temperature) reached
};

/** DC/DC class
 *
 * Contains all data belonging to the DC/DC sub-component of the PCB, incl.
 * actual measurements and calibration parameters.
 */
class Dcdc
{
public:
    /** Initialize DC/DC and DC/DC port structs
     *
     * See http://libre.solar/docs/dcdc_control for detailed information
     *
     * @param hv_side High voltage terminal (e.g. solar input for MPPT buck)
     * @param lv_side Low voltage terminal (e.g. battery output for MPPT buck)
     * @param mode Operation mode (buck, boost or nanogrid)
     */
    Dcdc(PowerPort *hv_side, PowerPort* lv_side, DcdcOperationMode mode);

    /** Check for valid start conditions of the DC/DC converter
     *
     * @returns 1 for buck mode, -1 for boost mode and 0 for invalid conditions
     */
    int check_start_conditions();

    /** Main control function for the DC/DC converter
     *
     * If DC/DC is off, this function checks start conditions and starts conversion if possible.
     */
    void control();

    /** Test mode for DC/DC, ramping up to 50% duty cycle
     */
    void test();

    /** Fast emergency stop function
     *
     * May be called from an ISR which detected overvoltage / overcurrent conditions
     */
    void emergency_stop();

    /** Prevent overcharging of battery in case of shorted HS MOSFET
     *
     * This function switches the LS MOSFET continuously on to blow the battery input fuse. The
     * reason for self destruction should be logged and stored to EEPROM prior to calling this
     * function, as the charge controller power supply will be cut after the fuse is destroyed.
     */
    void fuse_destruction();

    DcdcOperationMode mode;     ///< DC/DC mode (buck, boost or nanogrid)
    bool enable;                ///< Can be used to disable the DC/DC power stage
    uint16_t state;             ///< Control state (off / MPPT / CC / CV)

    // actual measurements
    PowerPort *hvs;             ///< Pointer to DC bus at high voltage side
    PowerPort *lvs;             ///< Pointer to DC bus at low voltage (inductor) side
    float temp_mosfets;         ///< MOSFET temperature measurement (if existing)

    // current state
    float power_prev;           ///< Stores previous conversion power (set via dcdc_control)
    int32_t pwm_delta;          ///< Direction of PWM change for MPPT
    int32_t off_timestamp;      ///< Last time the DC/DC was switched off
    int32_t power_good_timestamp;   ///< Last time the DC/DC reached above minimum output power

    // maximum allowed values
    float ls_current_max;       ///< Maximum low-side (inductor) current
    float hs_voltage_max;       ///< Maximum high-side voltage
    float ls_voltage_max;       ///< Maximum low-side voltage
    float ls_voltage_min;       ///< Minimum low-side voltage, e.g. for driver supply
    float output_power_min;     ///< Minimum output power (if lower, DC/DC is switched off)

    // calibration parameters
    uint32_t restart_interval;  ///< Restart interval (s): When should we retry to start
                                ///< charging after low output power cut-off?

private:
    /**
     * MPPT perturb & observe control
     *
     * Calculates the duty cycle change depending on operating mode and actual measurements and
     * changes the half bridge PWM signal accordingly
     *
     * @returns 0 if everything is fine, error number otherwise
     */
    int perturb_observe_controller();
};

#endif // __cplusplus


#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_CUSTOM_DCDC_CONTROLLER

/**
 * Low-level control function
 *
 * Implement this function e.g. for cycle-by-cylce current limitation.
 *
 * It is called from the DMA after each new current reading, i.e. it runs in ISR context with
 * high frequency and must be VERY fast!
 */
void dcdc_low_level_controller();

#endif // CONFIG_CUSTOM_DCDC_CONTROLLER

#ifdef __cplusplus
}
#endif

#endif /* DCDC_H */
