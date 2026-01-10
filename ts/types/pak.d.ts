declare module "pak:wifi" {
	export class WiFi {
		constructor();
		getDefaultAdapter(): WiFiAdapter;
		getAdapters(): WiFiAdapter[];
		bindSocketToAdapter(fd: number, adapter: WiFiAdapter): void;

		requestConnection(spec: WiFiApFilter, callback: (adapter: WiFiAdapter) => void);

		static WIFI_2GHZ: number;
		static WIFI_5GHZ: number;
	}

	interface WiFiApFilter {
		ssidPattern?: string;
		bssid?: string;
		bssid_mask?: string;
		password?: string;
		band?: number;
		hidden?: boolean;
	}

	export class WiFiAdapter {
		name: string;
		private constructor();

		getAps(): WiFiAp[];
		getConnectedAp(): WiFiAp;
		connectToAp(ap: WiFiAp): void;
		requestScan(): void;
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
	function socket(domain: number, type_: number, protocol: number): void;
	function setsockopt(fd: number, level: number, optname: number, value: ArrayBuffer): void;
}
