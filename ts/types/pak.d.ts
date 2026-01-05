declare module "pak:wifi" {
	export class WiFi {
		constructor();
		getDefaultAdapter(): WiFiAdapter;
		getAdapters(): WiFiAdapter[];
		bindSocketToAdapter(fd: number, adapter: WiFiAdapter): void;

		getAps(adapter: WiFiAdapter): WiFiAp[];
		getConnectedAp(adapter: WiFiAdapter): WiFiAp;
		connectToAp(adapter: WiFiAdapter, ap: WiFiAp): void;

		requestScan(adapter: WiFiAdapter): void;

		androidNetworkRequestDialog(adapter: WiFiAdapter, callback: Function): void;

		static WIFI_2GHZ: number;
		static WIFI_5GHZ: number;
	}

	export class WiFiAdapter {
		name: string;
		private constructor();
	}
	export class WiFiAp {
		private constructor();
	}
}

declare module "pak:bt" {
	export class Bt {
		constructor();
		getDefaultAdapter(): BluetoothAdapter;
		getAdapters(): BluetoothAdapter[];
	}

	export class BluetoothAdapter {
		private constructor();
	}
}

declare module "c:socket" {
	function socket(domain: number, type_: number, protocol: number);
	function setsockopt(fd: number, level: number, optname: number, value: ArrayBuffer);
}
