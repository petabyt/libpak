# libpak

WIP cross-platform Bluetooth and WiFi library for pairing with consumer devices

# Roadmap
- [x] QuickJS bindings
- [x] Communicate with cheap chinese dashcam in JS (HTTP/WiFi)
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
	- [ ] BTC bindings

## Future
- WASM bindings (c-api)
  - Compile libfuji with Wasm
- Start WiFi hotspot and expose a network interface
- NFC?
- UWB?
