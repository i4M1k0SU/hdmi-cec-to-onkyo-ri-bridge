#pragma once

// CEC bus timing (µs) — HDMI CEC 1.4 / 2.0 spec

// Start bit
#define CEC_T_START_LOW    3700
#define CEC_T_START_HIGH    800

// Data bit "0": long LOW, short HIGH
#define CEC_T_BIT0_LOW     1500
#define CEC_T_BIT0_HIGH     900

// Data bit "1": short LOW, long HIGH
#define CEC_T_BIT1_LOW      600
#define CEC_T_BIT1_HIGH    1800

// Nominal bit period
#define CEC_T_BIT_TOTAL    2400
