import { WiFi } from "pak:wifi";
import * as net from "c:socket";
import { BufferWriter } from "../ts/dist/buffer.js";

function makeCInt(val) {
	let yes = new ArrayBuffer(4);
	new DataView(yes).setInt32(0, val, true);
	return yes;
}

// http://192.168.169.1/app/getdeviceattr
// http://192.168.169.1:80/app/settimezone?timezone=-5
// http://192.168.169.1:80/app/setsystime?date=20251230141226
// http://192.168.169.1:80/app/capability
//   {"result":0,"info":{"value":"300001100"}}
// http://192.168.169.1:80/app/getproductinfo
// http://192.168.169.1:80/app/getdeviceattr
//   {"result":0,"info":{"uuid":"0CC119531A9E","otaver":"sar-Linux-v1.0alpha3","softver":"20250102.112318","hwver":"sar-Linux-v1.0alpha3","ssid":"V300-2QX9PI9Z","bssid":"0CC119531A9E","camnum":1,"curcamid":0,"wifireboot":1}}
// http://192.168.169.1:80/app/getmediainfo
//   {"result":0,"info":{"rtsp":"rtsp://192.168.169.1","transport":"tcp","port":5000}}
// http://192.168.169.1:80/app/getparamitems?param=all
//   {"result":0,"info":[{"name":"switchcam","items":["front","back"],"index":[0,1]},{"name":"mic","items":["off","on"],"index":[0,1]},{"name":"osd","items":["off","on"],"index":[0,1]},{"name":"rec_resolution","items":["1080P","1296P"],"index":[1,2]},{"name":"rec_split_duration","items":["1MIN","2MIN","3MIN"],"index":[0,1,2]},{"name":"speaker","items":["off","low","middle","high"],"index":[0,1,2,3]},{"name":"light_fre","items":["50HZ","60HZ"],"index":[0,1]},{"name":"timelapse_rate","items":["off","on"],"index":[0,1]},{"name":"rec","items":["off","on"],"index":[0,1]}]}
// http://192.168.169.1:80/app/getadasitems?param=all
//   {result: 98}
// ...
// http://192.168.169.1:80/app/enterrecorder
//   {"result":0,"info":"enter_recorder success"}
// http://192.168.169.1:80/app/setparamvalue?param=rec&value=0
//   {"result":0,"info":"set success"}
// http://192.168.169.1:80/app/playback?param=enter
//  {"result":0,"info":"set success"}
// http://192.168.169.1:80/app/getfilelist?folder=loop&start=0&end=99
//  {"result":0,"info":[{"folder":"loop","count":55,"files":[{"name":"/mnt/card/video_front/20251230_141244_f.ts","duration":-1,"size":31421,"createtime":1767103964,"createtimestr":"20251230141244","type":2},{"name":"/mnt/card/video_front/20251230_141144_f.ts","duration":-1,"size":96570,"createtime":1767103904,"createtimestr":"20251230141144","type":2},{"name":"/mnt/card/video_front/20251230_001349_f.ts","duration":-1,"size":35584,"createtime":1767053629,"createtimestr":"20251230001349","type":2},{"name":"/mnt/card/video_front/20251230_001249_f.ts","duration":-1,"size":96131,"createtime":1767053569,"createtimestr":"20251230001249","type":2},{"name":"/mnt/card/video_front/20251230_001149_f.ts","duration":-1,"size":96111,"createtime":1767053509,"createtimestr":"20251230001149","type":2},{"name":"/mnt/card/video_front/20251230_001049_f.ts","duration":-1,"size":96133,"createtime":1767053449,"createtimestr":"20251230001049","type":2},{"name":"/mnt/card/video_front/20251230_000949_f.ts","duration":-1,"size":96121,"createtime":1767053389,"createtimestr":"20251230000949","type":2},{"name":"/mnt/card/video_front/20251230_000849_f.ts","duration":-1,"size":96126,"createtime":1767053329,"createtimestr":"20251230000849","type":2},{"name":"/mnt/card/video_front/20251230_000749_f.ts","duration":-1,"size":96118,"createtime":1767053269,"createtimestr":"20251230000749","type":2},{"name":"/mnt/card/video_front/20251230_000649_f.ts","duration":-1,"size":96129,"createtime":1767053209,"createtimestr":"20251230000649","type":2},{"name":"/mnt/card/video_front/20251230_000549_f.ts","duration":-1,"size":96119,"createtime":1767053149,"createtimestr":"20251230000549","type":2},{"name":"/mnt/card/video_front/20251230_000449_f.ts","duration":-1,"size":96112,"createtime":1767053089,"createtimestr":"20251230000449","type":2},{"name":"/mnt/card/video_front/20251230_000349_f.ts","duration":-1,"size":96113,"createtime":1767053029,"createtimestr":"20251230000349","type":2},{"name":"/mnt/card/video_front/20251230_000249_f.ts","duration":-1,"size":96130,"createtime":1767052969,"createtimestr":"20251230000249","type":2},{"name":"/mnt/card/video_front/20251230_000149_f.ts","duration":-1,"size":96120,"createtime":1767052909,"createtimestr":"20251230000149","type":2},{"name":"/mnt/card/video_front/20251230_000049_f.ts","duration":-1,"size":96124,"createtime":1767052849,"createtimestr":"20251230000049","type":2},{"name":"/mnt/card/video_front/20251229_235949_f.ts","duration":-1,"size":96139,"createtime":1767052789,"createtimestr":"20251229235949","type":2},{"name":"/mnt/card/video_front/20251229_235849_f.ts","duration":-1,"size":96182,"createtime":1767052729,"createtimestr":"20251229235849","type":2},{"name":"/mnt/card/video_front/20251229_235749_f.ts","duration":-1,"size":96157,"createtime":1767052669,"createtimestr":"20251229235749","type":2},{"name":"/mnt/card/video_front/20251229_235649_f.ts","duration":-1,"size":96163,"createtime":1767052609,"createtimestr":"20251229235649","type":2},{"name":"/mnt/card/video_front/20251229_235549_f.ts","duration":-1,"size":96157,"createtime":1767052549,"createtimestr":"20251229235549","type":2},{"name":"/mnt/card/video_front/20251229_235449_f.ts","duration":-1,"size":96122,"createtime":1767052489,"createtimestr":"20251229235449","type":2},{"name":"/mnt/card/video_front/20251229_235349_f.ts","duration":-1,"size":96147,"createtime":1767052429,"createtimestr":"20251229235349","type":2},{"name":"/mnt/card/video_front/20251229_235249_f.ts","duration":-1,"size":96101,"createtime":1767052369,"createtimestr":"20251229235249","type":2},{"name":"/mnt/card/video_front/20251229_235149_f.ts","duration":-1,"size":96366,"createtime":1767052309,"createtimestr":"20251229235149","type":2},{"name":"/mnt/card/video_front/20251229_234515_f.ts","duration":-1,"size":26112,"createtime":1767051915,"createtimestr":"20251229234515","type":2},{"name":"/mnt/card/video_front/20251229_234415_f.ts","duration":-1,"size":96132,"createtime":1767051855,"createtimestr":"20251229234415","type":2},{"name":"/mnt/card/video_front/20251229_234315_f.ts","duration":-1,"size":96167,"createtime":1767051795,"createtimestr":"20251229234315","type":2},{"name":"/mnt/card/video_front/20251229_234215_f.ts","duration":-1,"size":96102,"createtime":1767051735,"createtimestr":"20251229234215","type":2},{"name":"/mnt/card/video_front/20251229_234115_f.ts","duration":-1,"size":96162,"createtime":1767051675,"createtimestr":"20251229234115","type":2},{"name":"/mnt/card/video_front/20251229_234015_f.ts","duration":-1,"size":96144,"createtime":1767051615,"createtimestr":"20251229234015","type":2},{"name":"/mnt/card/video_front/20251229_233915_f.ts","duration":-1,"size":96142,"createtime":1767051555,"createtimestr":"20251229233915","type":2},{"name":"/mnt/card/video_front/20251229_233815_f.ts","duration":-1,"size":96105,"createtime":1767051495,"createtimestr":"20251229233815","type":2},{"name":"/mnt/card/video_front/20251229_233715_f.ts","duration":-1,"size":96359,"createtime":1767051435,"createtimestr":"20251229233715","type":2},{"name":"/mnt/card/video_front/20251229_182648_f.ts","duration":-1,"size":83968,"createtime":1767032808,"createtimestr":"20251229182648","type":2},{"name":"/mnt/card/video_front/20251229_182548_f.ts","duration":-1,"size":96064,"createtime":1767032748,"createtimestr":"20251229182548","type":2},{"name":"/mnt/card/video_front/20251229_182448_f.ts","duration":-1,"size":96194,"createtime":1767032688,"createtimestr":"20251229182448","type":2},{"name":"/mnt/card/video_front/20251229_182348_f.ts","duration":-1,"size":96142,"createtime":1767032628,"createtimestr":"20251229182348","type":2},{"name":"/mnt/card/video_front/20251229_182248_f.ts","duration":-1,"size":96179,"createtime":1767032568,"createtimestr":"20251229182248","type":2},{"name":"/mnt/card/video_front/20251229_182148_f.ts","duration":-1,"size":96225,"createtime":1767032508,"createtimestr":"20251229182148","type":2},{"name":"/mnt/card/video_front/20251229_182048_f.ts","duration":-1,"size":96303,"createtime":1767032448,"createtimestr":"20251229182048","type":2},{"name":"/mnt/card/video_front/20251229_181948_f.ts","duration":-1,"size":95728,"createtime":1767032388,"createtimestr":"20251229181948","type":2},{"name":"/mnt/card/video_front/20251229_181848_f.ts","duration":-1,"size":96318,"createtime":1767032328,"createtimestr":"20251229181848","type":2},{"name":"/mnt/card/video_front/20251229_181748_f.ts","duration":-1,"size":96146,"createtime":1767032268,"createtimestr":"20251229181748","type":2},{"name":"/mnt/card/video_front/20251229_181648_f.ts","duration":-1,"size":96167,"createtime":1767032208,"createtimestr":"20251229181648","type":2},{"name":"/mnt/card/video_front/20251229_181548_f.ts","duration":-1,"size":96114,"createtime":1767032148,"createtimestr":"20251229181548","type":2},{"name":"/mnt/card/video_front/20251229_181448_f.ts","duration":-1,"size":96186,"createtime":1767032088,"createtimestr":"20251229181448","type":2},{"name":"/mnt/card/video_front/20251229_181348_f.ts","duration":-1,"size":96168,"createtime":1767032028,"createtimestr":"20251229181348","type":2},{"name":"/mnt/card/video_front/20251229_181248_f.ts","duration":-1,"size":96152,"createtime":1767031968,"createtimestr":"20251229181248","type":2},{"name":"/mnt/card/video_front/20251229_181148_f.ts","duration":-1,"size":96187,"createtime":1767031908,"createtimestr":"20251229181148","type":2},{"name":"/mnt/card/video_front/20251229_181048_f.ts","duration":-1,"size":96328,"createtime":1767031848,"createtimestr":"20251229181048","type":2},{"name":"/mnt/card/video_front/20251229_180948_f.ts","duration":-1,"size":95975,"createtime":1767031788,"createtimestr":"20251229180948","type":2},{"name":"/mnt/card/video_front/20251229_180848_f.ts","duration":-1,"size":96176,"createtime":1767031728,"createtimestr":"20251229180848","type":2},{"name":"/mnt/card/video_front/20251229_180748_f.ts","duration":-1,"size":96168,"createtime":1767031668,"createtimestr":"20251229180748","type":2},{"name":"/mnt/card/video_front/20251229_180648_f.ts","duration":-1,"size":96097,"createtime":1767031608,"createtimestr":"20251229180648","type":2}]}]}

class Veement {
	ip = "192.168.169.1";
	userAgent = "PakUserAgent";
	fd;
	constructor() {
		
	}
	onTryConnectWiFi(wifiAdapter, job) {
		let fd = net.socket(net.AF_INET, net.SOCK_STREAM, 0);

		WiFi.bindSocketToAdapter(adapter, fd);

		let yes = makeCInt(1);
		let rc = net.setsockopt(fd, net.IPPROTO_TCP, net.TCP_NODELAY, yes);
		if (rc) throw "setsockopt";

		let addr = net.createSockAddrIn(this.ip, 80);

		if (net.connect(fd, addr) != 0) throw "connect";

		this.fd = fd;
	}
	onDisconnect() {
		net.close(this.fd);
	}
	setTime() {
		let timestamp =
			d.getFullYear() +
			String(d.getMonth() + 1).padStart(2, "0") +
			String(d.getDate()).padStart(2, "0") +
			String(d.getHours()).padStart(2, "0") +
			String(d.getMinutes()).padStart(2, "0") +
			String(d.getSeconds()).padStart(2, "0");
		this.httpRequest("/app/settimezone?timezone=" + String(-5));
		this.httpRequest("/app/setsystime?date=" + String(timestamp));
	}
	onIdleTick(sinceLastTick) {
		
	}
	httpRequest(path) {
		let writer = new BufferWriter();
		writer.addString("GET " + path + " HTTP/1.1\r\n");
		writer.addString("Accept-Encoding: gzip\r\n");
		writer.addString("Connection: close\r\n");
		writer.addString("User-Agent: " + this.userAgent + "\r\n");
		writer.addString("Host: " + this.ip + "\r\n");
		let rc = net.write(this.fd, writer.buffer, writer.off);
		if (rc < 0) {
			throw "write(): " + String(rc);
		}
		net.read(this.fd, writer.buffer, writer.buffer.length);
		return rc;
	}
};


let v = new Veement();

