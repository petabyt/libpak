/**
 * PakDevice enum representing all device categories and types.
 * Standard TypeScript enum with explicit string values for direct runtime usage.
 */
export enum PakDevice {
  // Photo class
  PROFESSIONAL_CAMERA = "professional-camera",
  ACTION_CAMERA = "action-camera",
  DASHCAM = "dashcam",
  GENERIC_CAMERA = "generic-camera",
  LENS = "lens",
  WIFI_SD_CARD = "wifi-sd-card",
  DOORBELL = "doorbell",

  // Home class
  GENERIC_HOME_DEVICE = "generic-home-device",
  DESK = "desk",
  GENERIC_FURNITURE = "generic-furniture",
  PRINTER_3D = "3d-printer",

  // Accessory class
  HEADPHONES = "headphones",
  EARBUDS = "earbuds",
  SPEAKERS = "speakers",
  GENERIC_AUDIO = "generic-audio",
  SMART_GLASSES = "smart-glasses",
  SMART_TV = "smart-tv",
  SMARTWATCH = "smartwatch",
  GENERIC_MEDICAL_WEARABLE = "generic-medical-wearable",
  GENERIC_EXERCISE_MACHINE = "generic-exercise-machine",

  // Non-photo gadget class
  POWER_TOOL = "power-tool",
  GAME_CONTROLLER = "game-controller",
  DRONE = "drone",
  GENERIC_REMOTE_CONTROL = "generic-remote-control",
  SCOOTER = "scooter",
  BICYCLE = "bicycle",
  GENERIC_RIDEABLE = "generic-rideable",
  AUTOMOTIVE_INFOTAINMENT = "automotive-infotainment",
  AUTOMOTIVE_DIAGNOSTIC = "automotive-diagnostic",
  GENERIC_AUTOMOTIVE = "generic-automotive",
}