// Hằng số dùng chung toàn hệ thống SortiX Dashboard

export const DEFAULT_MQTT_TOPICS = {
  CONFIG_SET: "sorter/sorter_01/config/set",
  CONFIG_STATUS: "sorter/sorter_01/config/status",
  STATUS: "sorter/sorter_01/status",
  VISION: "sorter/sorter_01/vision",
  TELEMETRY: "sorter/sorter_01/telemetry",
  CONTROL: "sorter/sorter_01/control",
  ALERTS: "sorter/sorter_01/alerts",
  ESTOP: "conveyor/safety/estop",
  ESTOP_RELEASE: "conveyor/safety/estop/release",
  ESTOP_ACK: "conveyor/safety/estop/ack",
  JAM: "conveyor/sensor/jam",
  BIN_STATUS: "conveyor/storage/bin_status",
  TEMP: "conveyor/telemetry/temp",
  HEARTBEAT: "conveyor/heartbeat",
} as const;

export const SYSTEM_SAFETY_STATES = {
  OPERATIONAL: "OPERATIONAL",
  SYSTEM_LOCKED: "SYSTEM_LOCKED",
  HALTED: "HALTED",
  UNKNOWN: "UNKNOWN",
  UNAVAILABLE: "UNAVAILABLE",
  STALE: "STALE",
} as const;

export const DEFAULT_BINS = [
  { id: 1, name: "Khay 1 - Kim tiêm (Sharps Waste)", defaultBrand: "Kim tiêm" },
  { id: 2, name: "Khay 2 - Kéo y tế (Surgical Scissors)", defaultBrand: "Kéo" },
  { id: 3, name: "Khay 3 - Bông y tế (Medical Cotton)", defaultBrand: "Bông" },
] as const;

export const CLASSIFICATION_STATUSES = {
  SUCCESS: "success",
  DIVERTED_DEFAULT: "diverted_default",
  REJECTED: "rejected",
  JAMMED: "jammed",
} as const;

export const USER_ROLES = {
  ADMIN: "admin",
  OPERATOR: "operator",
  VIEWER: "viewer",
} as const;
