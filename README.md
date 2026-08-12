# libpak (WIP)

Sandbox/framework for pairing with consumer devices through Bluetooth or WiFi.

## Goals
- Supports Bluetooth Low Energy (BLE) and Bluetooth Classic
- Supports probing WiFi access points, connect/disconnect, bind socket to interface
- Android and Linux support
- Implements new Android companion/selector dialogs (`AssociationRequest`, `WifiNetworkSpecifier`)
- QuickJS and wasm c-api bindings (WIP)

## Sandbox
- Abstracted interface for device libraries (modules) to interact with the runtime
- Allows a wide variety of UI screens, widgets, commands, and properties to be manipulated and controlled
- Covers common device routines such as Bluetooth -> WiFi connection handover
