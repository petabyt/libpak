import { WiFi } from "pak:wifi";

let wifi = new WiFi();

//let adapters = wifi.getAdapters();
let adapter = wifi.getDefaultAdapter();

console.log(adapter);
