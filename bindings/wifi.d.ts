declare module "pak:wifi" {
	export class WiFi {
		constructor();

		getDefaultAdapter(): WiFiAdapter;
		getAdapters(): WiFiAdapter[];
	}

	export class WiFiAdapter {
		private constructor();
	}
}
