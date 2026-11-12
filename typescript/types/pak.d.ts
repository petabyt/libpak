declare module "pak:runtime" {
	export interface ObjectHandle {
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

	type MaybeAsync = void | Promise<void>;

	export class Module {
		onTryConnectWiFi(adapter: import("pak:wifi").WiFiAdapter, saved: ArrayBuffer, job: number): MaybeAsync;
		onTryConnectBluetooth(device: import("pak:bt").BluetoothDevice, saved: ArrayBuffer, job: number): MaybeAsync;
		onFindConnection(job: number): MaybeAsync;
		onDisconnect(): MaybeAsync;
		onSwitchScreen(oldScreen: number, newScreen: number, job: number): MaybeAsync;
		onRequestFileThumbnail(job: number, handle: ObjectHandle): MaybeAsync;
		onRequestFileMetadata(job: number, handle: ObjectHandle): MaybeAsync;
		onRequestFileContents(job: number, handle: ObjectHandle): MaybeAsync;
		onIdleTick(sinceLastTick: number): MaybeAsync;

		static export(mod: Module): void;

		setProgressBar(job: number, n: number): void;
		debugLog(value: string): void;
		globalLog(value: string): void;
		setProperty(key: string, value: string | number): void;
		setScreenSupported(screen: number, supported: boolean): void;
		addWidget(widget: Widget): void;
		addFileMetadata(handle: ObjectHandle, md: FileMetadata): void;
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
	export function connect(fd: number, addr: ArrayBuffer): number;
	export function accept(fd: number, addr: ArrayBuffer): number;
	export function getsockname(fd: number, addr: ArrayBuffer): number;
	export function getpeername(fd: number, addr: ArrayBuffer): number;
	export function recvfrom(fd: number, buf: ArrayBuffer, len: number, flags: number, srcAddr: ArrayBuffer, addrlen: ArrayBuffer): number;
	export function sendto(fd: number, buf: ArrayBuffer, len: number, flags: number, destAddr: ArrayBuffer, addrlen: number): number;
	export function read(fd: number, buf: ArrayBuffer, len: number, tmout: number): number;
	export function write(fd: number, buf: ArrayBuffer, len: number): number;
	export function close(fd: number): number;
	export function getaddrinfo(node: string, service: string, hints: ArrayBuffer, res: ArrayBuffer): number;
	export function freeaddrinfo(res: ArrayBuffer): void;
	export function createSockAddrIn(string: addr, port: number): ArrayBuffer;
	export function createInt(v: number): ArrayBuffer;

	declare const SOL_SOCKET: number;
	declare const SO_ACCEPTCONN: number;
	declare const SO_BROADCAST: number;
	declare const SO_DEBUG: number;
	declare const SO_DONTROUTE: number;
	declare const SO_ERROR: number;
	declare const SO_KEEPALIVE: number;
	declare const SO_LINGER: number;
	declare const SO_OOBINLINE: number;
	declare const SO_RCVBUF: number;
	declare const SO_RCVLOWAT: number;
	declare const SO_RCVTIMEO: number;
	declare const SO_REUSEADDR: number;
	declare const SO_REUSEPORT: number;
	declare const SO_SNDBUF: number;
	declare const SO_SNDLOWAT: number;
	declare const SO_SNDTIMEO: number;
	declare const SO_TYPE: number;
	declare const AF_UNSPEC: number;
	declare const AF_INET: number;
	declare const AF_INET6: number;
	declare const AF_UNIX: number;
	declare const AF_LOCAL: number;
	declare const PF_INET: number;
	declare const PF_INET6: number;
	declare const PF_UNIX: number;
	declare const SOCK_STREAM: number;
	declare const SOCK_DGRAM: number;
	declare const SOCK_RAW: number;
	declare const SOCK_SEQPACKET: number;
	declare const SOCK_RDM: number;
	declare const IPPROTO_IP: number;
	declare const IPPROTO_TCP: number;
	declare const IPPROTO_UDP: number;
	declare const IPPROTO_ICMP: number;
	declare const IPPROTO_IPV6: number;
	declare const TCP_NODELAY: number;
	declare const TCP_MAXSEG: number;
	declare const TCP_KEEPIDLE: number;
	declare const TCP_KEEPINTVL: number;
	declare const TCP_KEEPCNT: number;
	declare const TCP_INFO: number;
	declare const TCP_CORK: number;
	declare const TCP_QUICKACK: number;
	declare const IP_TTL: number;
	declare const IP_TOS: number;
	declare const IP_MULTICAST_IF: number;
	declare const IP_MULTICAST_TTL: number;
	declare const IP_MULTICAST_LOOP: number;
	declare const IP_ADD_MEMBERSHIP: number;
	declare const IP_DROP_MEMBERSHIP: number;
	declare const IPV6_V6ONLY: number;
	declare const IPV6_UNICAST_HOPS: number;
	declare const IPV6_MULTICAST_IF: number;
	declare const IPV6_MULTICAST_HOPS: number;
	declare const IPV6_MULTICAST_LOOP: number;
	declare const IPV6_JOIN_GROUP: number;
	declare const IPV6_LEAVE_GROUP: number;
	declare const MSG_OOB: number;
	declare const MSG_PEEK: number;
	declare const MSG_DONTROUTE: number;
	declare const MSG_CTRUNC: number;
	declare const MSG_TRUNC: number;
	declare const MSG_DONTWAIT: number;
	declare const MSG_NOSIGNAL: number;
	declare const SHUT_RD: number;
	declare const SHUT_WR: number;
	declare const SHUT_RDWR: number;
	declare const O_NONBLOCK: number;
	declare const O_CLOEXEC: number;
	declare const F_GETFL: number;
	declare const F_SETFL: number;
	declare const F_GETFD: number;
	declare const F_SETFD: number;
	declare const FD_CLOEXEC: number;
	declare const EINPROGRESS: number;
	declare const EWOULDBLOCK: number;
	declare const EAGAIN: number;
	declare const ECONNRESET: number;
	declare const ECONNREFUSED: number;
	declare const ETIMEDOUT: number;
	declare const EPIPE: number;
	declare const ENOTCONN: number;
	declare const EISCONN: number;
	declare const EADDRINUSE: number;
	declare const EADDRNOTAVAIL: number;
	declare const ENETDOWN: number;
	declare const ENETUNREACH: number;
	declare const EHOSTUNREAC: number;
}
