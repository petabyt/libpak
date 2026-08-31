declare module "pak:runtime" {
	export interface FileHandle {
		index: number;
	}

	export interface FileMetadata {
		filename: string;
		mimeType: string;
	}

	export interface BaseWidget {
		name: string;
		title: string;
	}
	export type WidgetTypeProps =
		| { type: "button" }
		| { type: "bool"; value: boolean }
		| { type: "slider"; value: number; min: number; max: number };
	export type Widget = BaseWidget & WidgetTypeProps;

	export class Module {
		onTryConnectWiFi(adapter: import("pak:wifi").WiFiAdapter, saved: ArrayBuffer, job: number): void;
		onFindConnection(job: number): void;
		onDisconnect(): void;
		onSwitchScreen(oldScreen: number, newScreen: number, job: number): void;
		onRequestFileThumbnail(job: number, handle: FileHandle): void;
		onRequestFileMetadata(job: number, handle: FileHandle): void;
		onRequestFileContents(job: number, handle: FileHandle): void;
		onIdleTick(sinceLastTick: number): void;

		static export(mod: Module): void;

		setProgressBar(job: number, n: number): void;
		debugLog(value: string): void;
		globalLog(value: string): void;
		setProperty(key: string, value: string | number): void;
		setScreenSupported(screen: number, supported: boolean): void;
		addWidget(widget: Widget): void;
		addFileMetadata(handle: FileHandle, md: FileMetadata): void;
	}
}

declare module "pak:wifi" {
	export const WIFI_2GHZ: number;
	export const WIFI_5GHZ: number;

	export interface WiFiApFilter {
		ssidPattern?: string;
		bssid?: string;
		bssidMask?: string;
		password?: string;
		band?: number;
		hidden?: boolean;
	}

	export class WiFi {
		constructor();
		getDefaultAdapter(): WiFiAdapter | null;
		getAdapters(): WiFiAdapter[];
		bindSocketToAdapter(fd: number, adapter: WiFiAdapter): void;
		requestConnection(spec: WiFiApFilter, callback: (adapter: WiFiAdapter) => void): void;
	}

	export class WiFiAdapter {
		name: string;
		private constructor();

		getAps(): WiFiAp[];
		getConnectedAp(): WiFiAp | null;
		connectToAp(ap: WiFiAp, password?: string): void;
		requestScan(): void;
	}

	export class WiFiAp {
		ssid: string;
		bssid: string;
		band: number;
		private constructor();
	}
}

declare module "pak:bt" {
	export class Bluetooth {
		constructor();
		getDefaultAdapter(): BluetoothAdapter | null;
		getAdapters(): BluetoothAdapter[];
	}

	export class BluetoothAdapter {
		private constructor();
		getDevices(): BluetoothDevice[];
	}

	export class BluetoothDevice {
		name: string;
		macAddress: string;
		private constructor();

		isClassic(): boolean;
		isConnected(): boolean;
		isBonded(): boolean;

		connect(): Promise<void>;
		disconnect(): Promise<void>;
		createBond(): Promise<void>;

		getManufacturerData(index: number): ArrayBuffer;
		connectToServiceChannel?(uuid: string): BluetoothSocket;
		getGattServices(): GattService[];
		getGattService(uuid: string): GattService;
	}

	export class BluetoothSocket {
		read(length: number, buffer: ArrayBuffer): number;
		write(length: number, buffer: ArrayBuffer): number;
		close(): void;
	}

	export class GattService {
		uuid: string;
		private constructor();

		getCharacteristics(): GattCharacteristic[];
		getCharacteristic(uuid: string): GattCharacteristic;
	}

	export class GattCharacteristic {
		uuid: string;
		private constructor();

		getDescriptors(): GattDescriptor[];
		getDescriptor(uuid: string): GattDescriptor;
		getValue(): Promise<ArrayBuffer>;
		setValue(data: ArrayBuffer): Promise<void>;
	}

	export class GattDescriptor {
		uuid: string;
		private constructor();
	}
}

declare module "c:socket" {
	export function socket(domain: number, type: number, protocol: number): number;
	export function setsockopt(fd: number, level: number, optname: number, value: ArrayBuffer): number;
	export function getsockopt(fd: number, level: number, optname: number, value: ArrayBuffer): number;
	export function bind(fd: number, addr: ArrayBuffer, addrlen: number): number;
	export function listen(fd: number, backlog: number): number;
	export function connect(fd: number, addr: ArrayBuffer, addrlen: number): number;
	export function accept(fd: number, addr: ArrayBuffer, addrlen: ArrayBuffer): number;
	export function getsockname(fd: number, addr: ArrayBuffer, addrlen: ArrayBuffer): number;
	export function getpeername(fd: number, addr: ArrayBuffer, addrlen: ArrayBuffer): number;
	export function recvfrom(fd: number, buf: ArrayBuffer, len: number, flags: number, srcAddr: ArrayBuffer, addrlen: ArrayBuffer): number;
	export function sendto(fd: number, buf: ArrayBuffer, len: number, flags: number, destAddr: ArrayBuffer, addrlen: number): number;
	export function getaddrinfo(node: string, service: string, hints: ArrayBuffer, res: ArrayBuffer): number;
	export function freeaddrinfo(res: ArrayBuffer): void;
	export function gai_strerror(code: number): string;
}
