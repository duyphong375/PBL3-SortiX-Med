#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * =============================================================================
 * SortiX Application Configuration & Protocol Definitions
 * =============================================================================
 */

// Device Identity (Unified across MQTT, HTTP, and SortiX-Dashboard)
#define SORTIX_DEVICE_ID                    "sorter_01"

// Fallback Policies on MQTT Disconnection / Vision AI Timeout
#define FALLBACK_POLICY_PAUSE_AND_HOLD      0   // Safe: Stop conveyor immediately, flash alert LED
#define FALLBACK_POLICY_DIVERT_TO_DEFAULT   1   // Run: Let item pass to default bin (Bin 3) as diverted_default

#ifndef SORTIX_FALLBACK_POLICY
#define SORTIX_FALLBACK_POLICY              FALLBACK_POLICY_PAUSE_AND_HOLD
#endif

// =============================================================================
// MQTT Topics (100% Synced with SortiX-Dashboard DEFAULT_MQTT_TOPICS)
// =============================================================================

// Inbound (Subscriptions)
#define SORTIX_TOPIC_VISION                 "sorter/sorter_01/vision"
#define SORTIX_TOPIC_CONTROL                "sorter/sorter_01/control"
#define SORTIX_TOPIC_ESTOP_RELEASE          "conveyor/safety/estop/release"
#define SORTIX_TOPIC_CONFIG_SET             "sorter/sorter_01/config/set"

// Outbound (Publications)
#define SORTIX_TOPIC_TELEMETRY              "sorter/sorter_01/telemetry"
#define SORTIX_TOPIC_STATUS                 "sorter/sorter_01/status"
#define SORTIX_TOPIC_CONFIG_STATUS          "sorter/sorter_01/config/status"
#define SORTIX_TOPIC_CLASSIFICATION         "sorter/sorter_01/classification"
#define SORTIX_TOPIC_HEARTBEAT              "conveyor/heartbeat"
#define SORTIX_TOPIC_JAM                    "conveyor/sensor/jam"
#define SORTIX_TOPIC_TEMP_WARN              "conveyor/telemetry/temp"

// Shared Secret for Control Command Authentication (START, STOP, ESTOP_RELEASE, CONFIG_SET)
// Commands from MQTT without a matching auth_token will be strictly rejected.
#define SORTIX_CONTROL_SECRET               "sortix_sec_control_2026"

// =============================================================================
// Security Notice Regarding Hardcoded Default Credentials:
// WARNING: The following Wi-Fi SSID, Wi-Fi password, SoftAP password and MQTT Broker
// URI are development defaults for initial laboratory setup. In production, these
// MUST be provisioned dynamically via NVS, captive portal, or encrypted provisioning.
// Never expose a device using default passwords to a public network or public MQTT broker!
// =============================================================================
#define DEFAULT_WIFI_SSID                   "SortiX-Lab"
#define DEFAULT_WIFI_PASS                   "sortix1234"
#define DEFAULT_MQTT_BROKER_URI             "mqtt://192.168.1.100:1883"
#define DEFAULT_HTTP_TELEMETRY_URL          "http://192.168.1.100:5000/api/telemetry"

#define SOFTAP_SSID                         "SortiX-Setup-AP"
#define SOFTAP_PASS                         "sortix1234"

// =============================================================================
// Mechanical & Calibration Hypotheses (MUST BE CALIBRATED ON REAL HARDWARE)
// =============================================================================
// Note: Motor JGB37-520 12V 66RPM (Gearbox ~90:1, 11 PPR on motor shaft).
// PCNT quadrature 4x mode yields ~3960 counts/rev on output shaft.
// These distances and thresholds are initial unverified hypotheses.
#define CALIB_ENCODER_COUNTS_PER_REV        3960
#define CALIB_ENCODER_COUNTS_PER_MM         31.5f       // ~31.5 counts per mm of conveyor travel
#define CONVEYOR_NOMINAL_SPEED_PCT          60          // 0-100 scale as enforced by TelemetrySchema

#define DIST_S1_TO_SERVO1_MM                200.0f      // Distance from IR S1 to Servo 1 (Station 1)
#define DIST_S1_TO_SERVO2_MM                400.0f      // Distance from IR S1 to Servo 2 (Station 2)
#define DIST_S1_TO_END_MM                   540.0f      // Distance from IR S1 to End of Belt (Bin 3)
#define SERVO_TRIGGER_WINDOW_MM             15.0f       // Margin around target position to trigger arm

// Timeouts & Safety Intervals
#define VISION_TIMEOUT_MS                   1000        // Time to wait for YOLOv8 result before fallback
#define JAM_STARTUP_GRACE_MS                1000        // Ignore stall checks during initial motor spinup
#define JAM_DETECTION_WINDOW_MS             1800        // If zero encoder delta within 1.8s while running -> JAM
#define CPU_TEMP_ALERT_THRESHOLD_C          75.0f       // On-chip SoC temperature warning threshold
#define TELEMETRY_PUBLISH_INTERVAL_MS       1500        // MQTT Telemetry publish interval (1-2s)
#define HEARTBEAT_INTERVAL_MS               2000        // Heartbeat publish interval (2s)
#define HTTP_TELEMETRY_INTERVAL_MS          5000        // Native HTTP POST telemetry interval (5s)

#define MAX_TRACKED_ITEMS                   15          // Maximum items in tracking FIFO ring buffer
