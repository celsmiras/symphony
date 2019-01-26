/*
 * DeviceDiscovery.h
 *
 * Enables the Symphony core to do simple device discovery.  Uses multicast to manage the ip addresses.
 * multicast ip is 239.1.2.3 port is 1234
 *
 *	json format
 *	{"mode":<int>, "name":<String>, "ip":<String>, "data":<array>}
 *	modes:
 *		1 : identify request
 *			- sent to identify itself to the other symphony devices
 *				* mandatory: "name": device name
 *				* mandatory: "ip":ip address [octet0, octet1, octet2, octet3]
 *			- if no response is received after 3 tries, will identify self if qualified as server
 *		10: identify response
 *			- response sent by the server, if there already is a server
 *				* mandatory: "data": server ip address of all the active devices
 *
 *  	2 : description request
 *  		- requests for more details on the device
 *  			* mandatory: "ip": ip address of target device
 *  	12: description response
 *  		- response sent by target device
 *  			* optional: "url": could contain url of device
 *  			* optional: "services": array that could contain services provided by device
 *  				[{"svc":<String>}]
 *
 *  	3 : notification request
 *  		- notification sent
 *  			* mandatory:
 *  	13: notification response
 *
 *
 *	transaction flow:
 *	identify
 *		client							server
 *		identify(ip,name)----------->saveClient(ip,name)
 *		end<-------------------------identify(server ip)
 *
 *	description
 *		client											targetDevice
 *		description(ip of target device)----------->getDescription(ip)
 *		end<----------------------------------------sendDescription(jsonArray)
 *
 *	notification
 *		TODO
 *
 *  Created on: Nov 3, 2018
 *      Author: cels
 */
#include <ESPAsyncUDP.h>

#ifndef DEVICEDISCOVERY_H_
#define DEVICEDISCOVERY_H_

#define DEBUG_
#define MAX_ID_COUNT 3

#define ID_REQ 1
#define ID_RESP 11
#define DESC_REQ 2
#define  DESC_RESP 12
#define  NOTI_REQ 3
#define  NOTI_RESP 13

AsyncUDP udp;
DynamicJsonBuffer gJsonBuffer;
JsonObject& gJson = gJsonBuffer.createObject();
JsonArray& devices = gJsonBuffer.createArray();	//container for the device info
JsonArray& gServerIP = gJson.createNestedArray("serverIp");
bool isDiscoveryServer = false;
String deviceName;
uint8_t idCounter = 0;
long timeMillis = millis();
long timerIntervalMillis = 120000;  //2-min timer interval

/**
 * The helper object that simplifies the handling of devices discovered.
 *
 * This creates JsonArray& devices;
 *
 * [
 * 	{"name":"test1","i":0,"ip":[192,168,0,103]},
 * 	{"name":"test2","i":0,"ip":[192,168,0,103]}
 * ]
 *
 *
 */
class DeviceInfo {
	public:
		DeviceInfo();
		void add(String name, IPAddress ip);
		void addServer(String name, IPAddress ip);
		void selectServer();
		void refresh();
		boolean exists(IPAddress findIP);
	private:
		long timeMs;
};
DeviceInfo::DeviceInfo() {
	timeMs = millis();
}
/*
 * adds a device object to JsonArray& devices;
 */
void DeviceInfo::add(String name, IPAddress ip) {
	JsonObject& device = devices.createNestedObject();
	device["name"] = name;
	device["time"] = millis();
	device["active"] = true;
	JsonArray& theIp = device.createNestedArray("ip");
	theIp.add(ip[0]);
	theIp.add(ip[1]);
	theIp.add(ip[2]);
	theIp.add(ip[3]);
}
/*
 * This is called if no server is responding to sendIdentify requests (idCounter > MAX_ID_COUNT)
 * This will select the device with the lowest active ip address as the server.
 */
void DeviceInfo::selectServer() {
#ifdef DEBUG_
		Serial.printf("[selectServer] count:%i, will nominate server:\n", idCounter);
#endif
		int lowest = 500;
		//we loop in the devices, to evaluate if our ip's 4th octet is the lowest from the active devices. If it is, then we will be the server
		for (int i = 0; i < devices.size(); i++) {
			Serial.printf("[selectServer]devices IP: %u.%u.%u.%u, i:%i, me:%s, serverIp: %u.%u.%u.%u\n",
				devices[i]["ip"][0].as<int>(),
				devices[i]["ip"][1].as<int>(),
				devices[i]["ip"][2].as<int>(),
				devices[i]["ip"][3].as<int>(),i, WiFi.localIP().toString().c_str(),
				gServerIP[0].as<int>(),
				gServerIP[1].as<int>(),
				gServerIP[2].as<int>(),
				gServerIP[3].as<int>()
				);
			if (devices[i]["ip"][3].as<int>() <= lowest && devices[i]["ip"][3].as<int>() != gServerIP[3].as<int>() && devices[i]["active"].as<bool>()) {
				lowest = devices[i]["ip"][3].as<int>();
			}
		}
		if (lowest == WiFi.localIP()[3] ) {
			Serial.println("Ayos!");
			isDiscoveryServer = true;
			gServerIP[0] = WiFi.localIP()[0];
			gServerIP[1] = WiFi.localIP()[1];
			gServerIP[2] = WiFi.localIP()[2];
			gServerIP[3] = WiFi.localIP()[3];
		} else {
			isDiscoveryServer = false;		//we might need to relinquish server mode, this happens if many devices start at almost the same time (within 30s)
		}
}
/*
 * adds a server object to JsonArray& devices;
 */
void DeviceInfo::addServer(String name, IPAddress ip) {

}
/*
 * Loops into JsonArray& devices and determines if a device is still alive by looking at the ["i"] element.
 * if ["i"] > 7, device is still alive
 */
void DeviceInfo::refresh() {
	if (millis() - timeMs >= timerIntervalMillis * 10) {	//refresh rate is 10 * timerIntervalMillis
		timeMs = millis();
#ifdef DEBUG_
		Serial.println("\n********************** DeviceInfo::refresh start");
#endif
		devices.printTo(Serial);
		for (int i = 0; i < devices.size(); i++) {
			//if time > 3* timerIntervalMillis & ip is not ip of this device, mark it as inactive
			if (millis() - devices[i]["time"].as<long>() > 3 * timerIntervalMillis && devices[i]["ip"][3] != WiFi.localIP()[3]) {
				devices[i]["active"] = false;
			}
		}
#ifdef DEBUG_
		Serial.println("\n********************** DeviceInfo::refresh end");
#endif
	}
}
/*
 * Finds the findIP from the JsonArray& devices;
 * returns 	true if found, and increments i
 * 			false if not found
 */
boolean DeviceInfo::exists(IPAddress findIP) {
	boolean isFound = false;
#ifdef DEBUG_
	Serial.println("\n********************** DeviceInfo::exists start");
#endif
	devices.printTo(Serial);
	for (int i = 0; i < devices.size(); i++) {
#ifdef DEBUG_
		JsonArray& iArray = devices[i]["ip"];
		Serial.printf("\nDevice IP: %u.%u.%u.%u, find IP:%u.%u.%u.%u, i:%i\n",
				iArray[0].as<int>(), iArray[1].as<int>(), iArray[2].as<int>(), iArray[3].as<int>(),
//			devices[i]["ip"][0].as<int>(), devices[i]["ip"][1].as<int>(), devices[i]["ip"][2].as<int>(), devices[i]["ip"][3].as<int>(),
			findIP[0],findIP[1], findIP[2], findIP[3],
			i);
#endif
		if (devices[i]["ip"][3].as<int>() == findIP[3]) {
			isFound = true;
			devices[i]["active"] = true;
			devices[i]["time"] = millis();
			break;
		}
	}
#ifdef DEBUG_
	Serial.println("\n********************** DeviceInfo::exists end");
#endif
	return isFound;
}

DeviceInfo deviceInfo = DeviceInfo();

/**
 *  handles the packets that arrive
 *  mode
 *  	1 : init
 *  	2 : response to init
 */
void packetArrived(AsyncUDPPacket packet) {
	//we received a data, let us determine if we are the server
	DynamicJsonBuffer jsonParsedBuffer;
	JsonObject & json = jsonParsedBuffer.parseObject(packet.data());
#ifdef DEBUG_
	Serial.printf("\n[packetArrived] start, i am %s devices.size:%i\nGlobal json is:\n", isDiscoveryServer ? "Server" : "Client", devices.size());
	Serial.println("packet.data parsed:");
	json.printTo(Serial);
	Serial.println();
#endif
	if (json.containsKey("mode")) {
		uint8_t mode = json["mode"];
		if (mode == ID_REQ) {
/*
 * TODO
 *   need to remove the ip from the ipData array if a client is not sending identify command for 3 consecutive runs
 */

			//a client sent an identify command
#ifdef DEBUG_
			Serial.println("**************** start ID_REQ MODE ***********************");
#endif
			if (json.containsKey("ip")) {
				if (deviceInfo.exists(packet.remoteIP())) {


				} else {
					//device is new, we add it to deviceInfo
					deviceInfo.add(json["name"], packet.remoteIP());
				}
			}
			if (isDiscoveryServer) {
				//send reply since i am the server
				DynamicJsonBuffer jsonBuffer;
				JsonObject& reply = jsonBuffer.createObject();
				reply["mode"] = ID_RESP;
				JsonArray& serverIP = reply.createNestedArray("serverIp");
				serverIP.add(WiFi.localIP()[0]);
				serverIP.add(WiFi.localIP()[1]);
				serverIP.add(WiFi.localIP()[2]);
				serverIP.add(WiFi.localIP()[3]);

				idCounter = 0;
				String replyStr;
				reply.printTo(replyStr);
				packet.printf(replyStr.c_str());
#ifdef DEBUG_
				Serial.println("I am server, will reply with:");\
				reply.printTo(Serial);
				Serial.println("\ndone reply");
#endif
			}
#ifdef DEBUG_
			Serial.println("**************** end ID_REQ MODE ***********************");
#endif
		} else if (mode == ID_RESP) {
#ifdef DEBUG_
			Serial.println("**************** start ID_RESP MODE ***********************");
#endif
			idCounter = 0;
			if (json.containsKey("serverIp")) {
				//this is the response from the server
				gServerIP[0] = packet.remoteIP()[0];
				gServerIP[1] = packet.remoteIP()[1];
				gServerIP[2] = packet.remoteIP()[2];
				gServerIP[3] = packet.remoteIP()[3];
			}
#ifdef DEBUG_
			Serial.println("Got server response.");
			gServerIP.printTo(Serial);
			Serial.println();
			Serial.println("**************** end ID_RESP MODE ***********************");
#endif
		} else if (mode == NOTI_REQ) {
			if (isDiscoveryServer) {
				DynamicJsonBuffer jsonBuffer;
				JsonObject& reply = jsonBuffer.createObject();
				reply["mode"] = NOTI_RESP;
//				reply["info"] = gJson;
				reply["info"] = devices;
				reply["server"] = gServerIP;
				String replyStr;
				reply.printTo(replyStr);
				packet.printf(replyStr.c_str());
#ifdef DEBUG_
				reply.printTo(Serial);
				Serial.println();
#endif
			}
		}
	}
#ifdef DEBUG_
	Serial.println("[packetArrived] end\n");
#endif
}
/*
 * put this in the loop function of arduino code.
 * sendIndentify
 * 	timerInterval should not be too small as it will flood the network
 *
 */
void sendIdentify() {
	if (WiFi.status() == WL_CONNECTED ) {
		deviceInfo.refresh();
		if (millis() - timeMillis >= timerIntervalMillis) {	//send multicast every timerIntervalMillis ms
			timeMillis = millis();
			if (!isDiscoveryServer)
				idCounter++;
			DynamicJsonBuffer jsonIDBuffer;
			JsonObject& tmpJson = jsonIDBuffer.createObject();
			tmpJson["mode"] = 1;
			tmpJson["name"] = deviceName;
			JsonArray& data = tmpJson.createNestedArray("ip");
			data.add(WiFi.localIP()[0]);
			data.add(WiFi.localIP()[1]);
			data.add(WiFi.localIP()[2]);
			data.add(WiFi.localIP()[3]);
#ifdef DEBUG_
			Serial.printf("[sendIdentify %s] count:%i, will send data:\n", isDiscoveryServer ? "Server" : "Client", idCounter);
			tmpJson.printTo(Serial);
			Serial.printf("\n[sendIdentify %s] data sent.\n", isDiscoveryServer ? "Server" : "Client");
#endif
			String dataToSend;
			tmpJson.printTo(dataToSend);
			udp.printf(dataToSend.c_str());		//send an init command to the udp server
		}
		if (idCounter > MAX_ID_COUNT) {
			//count for identify message already exceeded
#ifdef DEBUG_
			Serial.printf("[sendIdentify] count:%i, Server not responding, will evaluate if i can be server:\n", idCounter);
			idCounter = 0;
#endif
			//nominateServer();
			deviceInfo.selectServer();
		}
	}
}
/**
 * the overloaded function
 */
void sendIdentify(long intervalMillis) {
	timerIntervalMillis = intervalMillis;
	sendIdentify();
}
/*
 * Starts a UDP server to listen to Symphony devices and stores their IP addresses.
 * this will enable "auto-discovery" from the udp server 239.1.2.3 port 1234
 */
void startDiscovery(String name) {
	Serial.println("[startDiscovery] start");
	deviceName = name;
	if (udp.listenMulticast(IPAddress(239, 1, 2, 3), 1234)) {
		udp.onPacket(packetArrived);
		long timeMillis = millis();
		sendIdentify();
		deviceInfo.add(deviceName, WiFi.localIP());
#ifdef DEBUG_
		Serial.println("[startDiscovery]");
		gJson.printTo(Serial);
#endif
		gServerIP.add(0);
		gServerIP.add(0);
		gServerIP.add(0);
		gServerIP.add(0);
	}
	Serial.println("\n[startDiscovery] end");
}


#endif /* DEVICEDISCOVERY_H_ */

