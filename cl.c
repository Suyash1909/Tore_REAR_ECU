#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// --- Configuration based on your checklist ---
#define MAX_ADC_VAL 4095.0f
#define SYSTEM_VOLTAGE 5.0f      // Change to 3.3f if using a 3.3V external ADC
#define DEVIATION_LIMIT 0.5f     // 10% of 5V
#define IMPLAUSIBILITY_TIME_MS 100

// Define the state machine for the APPS
typedef enum {
    APPS_NORMAL,
    APPS_SAFE_STATE
} APPS_State;

// Global variables to track state
APPS_State current_state = APPS_NORMAL;
long long implausibility_start_time = 0;
bool is_implausible = false;

// Helper function to get OS-level monotonic time in milliseconds
long long get_current_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

/**
 * Main APPS processing function to be called continuously in your main loop.
 * Takes the raw 12-bit ADC values and returns a torque request percentage (0.0 to 1.0).
 */
float calculate_apps_torque(uint16_t adc_s1, uint16_t adc_s2) {
    
    // If we are already in the safe state, keep torque at 0.
    // (You will need a separate mechanism/button to reset this state later).
    if (current_state == APPS_SAFE_STATE) {
        return 0.0f;
    }

    // 3 & 4) Convert raw ADC values to Voltages
    float v1_raw = ((float)adc_s1 / MAX_ADC_VAL) * SYSTEM_VOLTAGE;
    float v2_raw = ((float)adc_s2 / MAX_ADC_VAL) * SYSTEM_VOLTAGE;

    // 6) Check for 10% deviation. 
    // S1 ascends (0->5V), S2 descends (5->0V), so their sum should ALWAYS be 5V.
    float voltage_sum = v1_raw + v2_raw;
    float deviation = fabs(voltage_sum - SYSTEM_VOLTAGE);

    // 7) Implausibility Logic
    if (deviation > DEVIATION_LIMIT) {
        if (!is_implausible) {
            // The deviation just started. Mark the time.
            is_implausible = true;
            implausibility_start_time = get_current_time_ms();
        } else {
            // It has been implausible for a while. Check if it exceeds 100ms.
            long long elapsed_time = get_current_time_ms() - implausibility_start_time;
            
            if (elapsed_time > IMPLAUSIBILITY_TIME_MS) {
                // 8) Trigger Safe State
                current_state = APPS_SAFE_STATE;
                printf("APPS FAULT: 100ms Implausibility exceeded. Torque disabled.\n");
                return 0.0f; 
            }
        }
    } else {
        // The deviation disappeared before 100ms elapsed. Reset the timer.
        is_implausible = false;
    }

    // If no fault is triggered, calculate the final requested torque percentage.
    // S1 is direct (0 to 5V). We can divide by max voltage to get 0.0 to 1.0.
    float requested_torque_pct = v1_raw / SYSTEM_VOLTAGE;
    
    // Safety clamp just in case noise pushes it slightly above 1.0 or below 0.0
    if (requested_torque_pct > 1.0f) requested_torque_pct = 1.0f;
    if (requested_torque_pct < 0.0f) requested_torque_pct = 0.0f;

    return requested_torque_pct;
}