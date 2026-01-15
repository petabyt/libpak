# libpak

WIP cross-platform Bluetooth and WiFi library for pairing with consumer devices

# Roadmap
- [x] QuickJS bindings
- [x] Communicate with cheap chinese dashcam in JS (HTTP/WiFi)
- [ ] Communicate with bluetooth classic devices
- **Linux:**
	- [x] Basic NetworkManager APIs
		- [x] Query adapters
		- [x] Scan for access points
		- [x] Connect to an access point
	- [ ] BlueZ BLE support
	- [ ] BlueZ Bluetooth Classic support
- **Android:**
	- [ ] Companion APIs for BLE/BTC/WiFi
	- [ ] NetworkRequest APIs
	- [ ] BLE bindings
	- [ ] BTC bindings

## Future
- Module system
- WASM bindings (c-api)
- Start WiFi hotspot and expose a network interface
- NFC?
- UWB?
