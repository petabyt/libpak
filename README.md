# libpak

(WIP) Cross-platform Bluetooth and WiFi library for pairing with consumer devices

## Features
- Supports Android and Linux (BlueZ/NetworkManager)
- APIs for using Android companion/selector dialogs (`AssociationRequest`, `WifiNetworkSpecifier`)
- Supports Bluetooth Low Energy (BLE) and Bluetooth Classic
- WiFi API supports probing APs, connect/disconnect, and socket binding
- QuickJS bindings
- (WIP) wasm c-api bindings

## Why
All the cross-platform Bluetooth libraries I found had drawbacks:
- Difficult to embed reliabily
- Too many dependencies
- No Bluetooth classic support

## Future
- Start WiFi hotspot and expose a network interface
