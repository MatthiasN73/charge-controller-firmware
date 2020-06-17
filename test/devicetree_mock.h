/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

/*
 * Generated defines (from devicetree_unfixed.h)
 */

#define DT_N_S_adc_inputs_S_temp_bat_EXISTS 1

/*
 * Macros copied from devicetree.h
 */

#define DT_CAT(node_id, prop_suffix) node_id##prop_suffix

#define DT_NODE_EXISTS(name) DT_CAT(name, _EXISTS)

/*
 * Simplified mocks for certain macros from devicetree.h
 */

#define DT_PATH(node) DT_N_S_##node

#define DT_CHILD(parent, child) DT_N_S_adc_inputs_S_##child

#define DT_FOREACH_CHILD(dummy, fn) \
    fn(DT_N_S_adc_inputs_S_v_low) \
    fn(DT_N_S_adc_inputs_S_v_high) \
    fn(DT_N_S_adc_inputs_S_v_pwm) \
    fn(DT_N_S_adc_inputs_S_i_dcdc) \
    fn(DT_N_S_adc_inputs_S_i_load) \
    fn(DT_N_S_adc_inputs_S_i_pwm) \
    fn(DT_N_S_adc_inputs_S_temp_bat) \
    fn(DT_N_S_adc_inputs_S_vref_mcu) \
    fn(DT_N_S_adc_inputs_S_temp_mcu)
