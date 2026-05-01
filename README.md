# libpak

Cross-platform Bluetooth and WiFi library for pairing with consumer devices

Features:
- Supports Android and Linux (BlueZ/NetworkManager)
- Bluetooth Low Energy (BLE) and Bluetooth Classic
- WiFi API allow probing APs, connect/disconnect, and socket binding
- QuickJS bindings

# Roadmap
- [x] QuickJS bindings
- [x] Communicate with WiFi devices (HTTP)
- [x] Communicate with bluetooth classic devices
- **Linux:**
	- [x] Basic NetworkManager APIs
		- [x] Query adapters
		- [x] Scan for access points
		- [x] Connect to an access point
	- [x] BlueZ Bluetooth Classic support
	- [ ] BlueZ BLE support
	- [ ] BlueZ scanning and pairing
	- [ ] BlueZ advertising
- **Android:**
    - [x] USB bindings
	- [x] NetworkRequest APIs
    - [ ] Customizable permission requester
	- [ ] BLE bindings
	- [x] BTC bindings

## Future
- WASM bindings (c-api)
  - Compile libfuji with Wasm
- Start WiFi hotspot and expose a network interface
- NFC?
- UWB?
