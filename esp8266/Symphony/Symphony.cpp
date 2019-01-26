/*
 * SymphonyCore.cpp
 *
 * This is the core framework for the Symphony Products.
 * This will connect to an AP, the SSID and PassPhrase can be configured at <this>.ip/main
 * By default, AP Mode is enabled if this cannot connect to an AP.  This can be changed in <this>.ip/ota_setup
 * 		options: 1-default (AP enable if not connected to AP)
 * 				 2-AP always enabled
 * 				 3-AP always disabled
 *
 * Usage Notes:
 * 1. ALWAYS set the product first before calling setup
 *		s.setProduct(product);
 *		s.setup();
 * 2. ALWAYS set the wsCallback.  This is for the handling of requests from clients. This is via WebSockets.
 *		s.setWsCallback(WsCallback);
 *
 *
 *TODO:
 *	Oct 06, 2018:
 *	1. modify wsEvent to handle events with 4 characters
 *	2. modify wsEvent to handle INIT events, instantiators should no longer handle this even if they have set a callback
 *
 */


#include "Symphony.h"
#include "DeviceDiscovery.h"

#define ver "1.3.2"
#define DISCOVERABLE

String Symphony::rootProperties = "";
String Symphony::hostName = "hostName";
String Symphony::myName = "myName";
Product Symphony::product;

AsyncWebServer		webServer(HTTP_PORT); // Web Server
AsyncWebSocket      ws("/ws");      // Web Socket Plugin
WiFiEventHandler    wifiConnectHandler;     // WiFi connect handler
WiFiEventHandler    wifiDisconnectHandler;  // WiFi disconnect handler

String homeHtml;

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;
String responseHTML = ""
                      "<!DOCTYPE html><html><head><title>CaptivePortal</title></head><body>"
                      "<h1>Hello World!</h1><p>This is a captive portal example. All requests will "
                      "be redirected here.</p></body></html>";

bool		reboot 	= false; // Reboot flag
bool 		isUpdateFw = false; // used to indicate if firmware update is ongoing
Filemanager	fManager = 		Filemanager();
int8_t		fwResult = 0;
long identifyTries[] = {5000, 10000, 20000};
int tries = 0;

/*
 * This is the callback handler that will be called when a Websocket event arrives.
 * This enables the instantiator to handle websocket events.
 * We are exposing the AsyncWebSocket ws to enable the instantiator to broadcast to all connected clients via the ws.textAll
 * AsyncWebSocketClient *client is exposed to enable response to the calling client.
 */
int (* WsCallback) (AsyncWebSocket ws, AsyncWebSocketClient *client, JsonObject& json);

/*
 *	wsEvent handles the transactions sent by client websockets.
 *	events can either be handled by the core, or the implementor.
 *	events recognized should be in json format and of the syntax:
 *	{"core":1, "data":""}, {"cmd":1, "data":""} or {"do":1, "data":""}
 *	where
 *	  "core" is handled by core,
 *	  "cmd" handled by implementor
 *	  "do" will be handle by both core and implementor
 *
 *	if "core", below are the recognized values
 *		0:
 *		1: client server connections command send by client,
 *			data: "INIT" - usually done during initial connect of client
 *			data: "PING" - done during the firmware update of client, client will receive websocket.onclose event if it tries to send during ESP reboot
 *						 - not needed as the client can detect the websocket.onclose event even without sending data
 *		2: commit AP, passkey and Device name
 *		3: delete file command from the fileupload client
 *
 */
void wsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void * arg, uint8_t *data, size_t len) {
	if (!isUpdateFw) {
#ifdef DEBUG_ONLY
		Serial.printf("websocket called len=%i\n",len);
#endif
		switch (type) {
			case WS_EVT_DATA: {
				AwsFrameInfo *info = static_cast<AwsFrameInfo*>(arg);
				if (info->opcode == WS_TEXT) {
//					StaticJsonBuffer<300> jsonBuffer;
					DynamicJsonBuffer jsonBuffer;
					JsonObject& json = jsonBuffer.parseObject(data);
					if (!json.success()) {
						Serial.println("core parseObject() failed");
					} else {
#ifdef DEBUG_ONLY
						json.prettyPrintTo(Serial);Serial.println();
#endif
						if (json.containsKey("do")) {
							//this is for both the core and implementor
						} else if (json.containsKey("core")) {
							//this is for the core
							int cmd = json["core"].as<int>();
							switch (cmd) {
								case CORE_INIT:
									//this is the connection handling
									if (json.containsKey("data")) {
										String d = json["data"].as<String>();
										if (d.equals("INIT")) {
											DynamicJsonBuffer jsonBuffer;
											JsonObject& reply = jsonBuffer.createObject();
											reply["cmd"] = 1;
											reply["name"] = Symphony::hostName;
											reply["msg"] = "Ready for commands.";
											reply["mac"] = Symphony::myName;
											reply["box"] = "status";	//the element to show the message
											reply["cid"] = client->id();	//the client id
											String strReply;
											reply.printTo(strReply);
											client->text(strReply);
										}
										if (d.equals("PING"))  //we are supposed to use this to ping this server, but websocket client.onclose is already triggered
											client->text("PING Successful");
									}
									break;
								case CORE_COMMIT:
									//this is the commit AP, Passkey and Device Name, then reboot is done
									if (json.containsKey("data")) {
										String cfg = json["data"].as<String>();
#ifdef DEBUG_ONLY
										Serial.printf("\nwill save config %s\n", cfg.c_str());
#endif
										fManager.saveConfig(cfg.c_str());
										reboot = true;
									}
									break;
								case CORE_DELETE://this is the delete file
									//delete path fr SPIFFS
									if (json.containsKey("data")) {
										String path = json["data"].as<String>();
#ifdef DEBUG_ONLY
										Serial.printf("\nwill delete %s\n", path.c_str());
#endif
										fManager.delFile(path);
										client->text(path + " deleted");
									}
									break;
								case CORE_VALUES://VALUES command
										client->text(Symphony::product.stringifyValues());
									break;
								case CORE_GETDEVICEINFO://GET DEVICE INFO command
									Serial.println("********************* Get device Info.");
//									DynamicJsonBuffer jsonBuffer;
//									JsonObject& jsonObj = jsonBuffer.createObject();
//
//									jsonObj["ssid"] = ssid;
//									jsonObj["pwd"] = pwd;
//									jsonObj["name"] = name;
									DynamicJsonBuffer jsonBuffer;
									JsonObject& jsonObj = jsonBuffer.parseObject(fManager.readConfig());
									if (jsonObj.success()) {
#ifdef DEBUG_ONLY
										Serial.println("*********************8 Get device Info.");
										jsonObj.prettyPrintTo(Serial);
#endif
										jsonObj["core"] = CORE_GETDEVICEINFO;
										String deviceInfo;
										jsonObj.printTo(deviceInfo);
										client->text(deviceInfo);
									}
									break;
							}
						} else if (json.containsKey("cmd")) {
							int cmd = json["cmd"].as<int>();
							if (cmd == 10) {//command from control
								//evaluate if corePin==true, execute here.  Else pass to wscallback
								attribStruct attrib = Symphony::product.getProperty(json["ssid"].as<char *>());
	#ifdef DEBUG_ONLY
								Serial.printf("got attribute %s, current value=%i, pin=%i, corePin=%s\n", attrib.ssid.c_str(), attrib.gui.value, attrib.pin, attrib.corePin?"true":"false");
	#endif
								if (attrib.corePin) {
									Symphony::product.setValue(json["ssid"].as<char *>(), json["val"].as<int>());
								} else {
									//this is for the implementor
									if (WsCallback != nullptr) {
										WsCallback(ws, client, json);
									} else {
										Serial.println("[wsEvent]No Websocket callback set!!!");
									}
								}
								String strReply;
								json.printTo(strReply);
								ws.textAll(strReply);
							} else {
								//this is for the implementor
								if (WsCallback != nullptr) {
									WsCallback(ws, client, json);
								} else {
									Serial.println("[wsEvent]No Websocket callback set!!!");
								}
							}
						}
					}
				} else {
					Serial.println(F("-- binary message --"));
				}
				break;
			}
			case WS_EVT_CONNECT:
				Serial.print(F("WS Connect - "));
				Serial.println(client->id());
				client->text("{\"box\":\"status\",\"msg\":\"Connected\"}");
				break;
			case WS_EVT_DISCONNECT:
				Serial.print(F("WS Disconnect - "));
				Serial.println(client->id());
				break;
			case WS_EVT_PONG:
				Serial.println(F("WS PONG"));
				break;
			case WS_EVT_ERROR:
				Serial.println(F("WS ERROR"));
				break;
		}
	} else {
		Serial.print(F("Cannot do websocket request since we are updating firmware.\n"));
	}
}

void onWifiConnect(const WiFiEventStationModeGotIP &event) {
    Serial.print(F("Connected with IP: "));
    Serial.println(WiFi.localIP());

    // Setup mDNS / DNS-SD
    char chipId[7] = { 0 };
    snprintf(chipId, sizeof(chipId), "%06x", ESP.getChipId());
    MDNS.setInstanceName("staticHostname");
    if (MDNS.begin("static.Hostname")) {
        MDNS.addService("http", "tcp", HTTP_PORT);
    } else {
        Serial.println(F("*** Error setting up mDNS responder ***"));
    }
}
void onWiFiDisconnect(const WiFiEventStationModeDisconnected &event) {
    Serial.println(F("*** WiFi Disconnected ***"));

    // Pause MQTT reconnect while WiFi is reconnecting
//    mqttTicker.detach();
//    wifiTicker.once(2, connectWifi);
}

/***
 * Control Section
 * 		This is where we set the html display for controlling the device
 */
void showControl(AsyncWebServerRequest *request) {
	request->send(200, "text/html", homeHtml);
	Serial.println("control page displayed");
}
/*
 *  Returns the properties in the form:
 *  {typ:'Rad',lbl:'RED',val:'0007', grp:'g2'}
 *  {typ:'Rad',lbl:'GREEN',val:'0007', grp:'g2'},
 *  {typ:'Btn',lbl:'STOP',val:'0009', grp:'g2'},
 *  {typ:'Rng',lbl:'Hue',val:'0011',min:'0',max:'360', grp:'g2'}
 */
void showProperties(AsyncWebServerRequest *request) {
	request->send(200, "text/html", Symphony::rootProperties);
	Serial.println("**************************** showProperties");
}
/**
 * File upload Section
 */
void showFileUpload(AsyncWebServerRequest *request) {
	request->send(200, "text/html", UPLOAD_HTML);//shows file upload html from the PROGMEM defined in html.h, we are doing this because initial loading of firmware does not have the SPIFFS files
	Serial.println("**************************** showFileUpload");
}
/*
 * handles the upload of upload File
 */
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) { // upload a new file to the SPIFFS
	Serial.printf("handleFileUpload filename=%s\n", filename.c_str());
	int8_t result = fManager.uploadFile(filename, index, data, len, final);
	Serial.printf("handleFileUpload done result=%i\n", result);
}
void doneFileUpload(AsyncWebServerRequest *request) {
	request->send(200, "text/html", "{\"resp\":\"203\",\"msg\":\"File Upload Done.\"}");
	Serial.println("**************************** doneFileUpload");
}
void handleGetFiles(AsyncWebServerRequest *request) {
	String str = fManager.getFiles();
	request->send(200, "text/html", str);
}

/**
 * Firmware update Section
 */
void showFWUpdate(AsyncWebServerRequest *request) {
	request->send(SPIFFS, "/admin.html", "text/html"); //we are showing admin.html in SPIFFS
//	request->send(200, "text/html", UPLOAD_HTML1);
	Serial.println("**************************** showFWUpdate");
}

void doneFWUpdate(AsyncWebServerRequest *request) {
	request->send(200, "text/html", "Firmware Update Done.");
	Serial.printf("**************************** doneFWUpdate %i\n", fwResult);
	reboot = true;
}
void handleFirmWareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
	isUpdateFw = true;
	delay(50);	//this is to enable the update to do SPIFF write before getting another data to write
	fwResult = fManager.updateFirmware(filename, index, data, len, final);
//	Serial.printf("**************************** handleFirmWareUpload %i\n", fwResult);
}
/****
 * Initialize the webserver
 */
void initWebServer() {

	webServer.on("/", showControl);
	webServer.serveStatic("/admin", SPIFFS, "/admin.html");
	webServer.serveStatic("/img.jpg", SPIFFS, "/img.jpg");
	webServer.serveStatic("/symphony.css", SPIFFS, "/symphony.css");
//	webServer.serveStatic("/control.css", SPIFFS, "/control.css");
	webServer.serveStatic("/symphony.js", SPIFFS, "/symphony.js");
	webServer.on("/firmware", HTTP_GET, showFWUpdate);  //show the firmware update page
	webServer.on("/updateFirmware", HTTP_POST, doneFWUpdate, handleFirmWareUpload); //handle the firmware update request
	webServer.on("/files", HTTP_GET, showFileUpload);  //show the file upload page
	webServer.on("/uploadFile", HTTP_POST, doneFileUpload, handleFileUpload); //handle the file update request
	webServer.on("/getFiles", HTTP_GET, handleGetFiles);  //show the Files in SPIFFS
	webServer.on("/properties.html", showProperties);
	webServer.serveStatic("/test.html", SPIFFS, "/test.html");
	webServer.onNotFound([](AsyncWebServerRequest *request) {
		request->send(404, "text/plain", "Page not found");
	});
}
/*
 * Constructor
 */
Symphony::Symphony(){

}

/*
 * Initiates the Symphony module
 */
void Symphony::setup(String theHostName) {
//	Serial.begin(115200);	//implementing objects should set the baud rate
//	wifi_set_sleep_type(NONE_SLEEP_T);
#ifdef SHOW_FLASH
	  uint32_t realSize = ESP.getFlashChipRealSize();
	  uint32_t ideSize = ESP.getFlashChipSize();
	  FlashMode_t ideMode = ESP.getFlashChipMode();
	  Serial.printf("\n[init]Sketch size: %u\n", ESP.getSketchSize());
	  Serial.printf("[init]Free Sketch size: %u\n", ESP.getFreeSketchSpace());
	  Serial.printf("[init]Flash real id:   %08X\n", ESP.getFlashChipId());
	  Serial.printf("[init]Flash real size: %u\n", realSize);
	  Serial.printf("[init]Flash ide  size: %u\n", ideSize);
	  Serial.printf("[init]Flash ide speed: %u\n", ESP.getFlashChipSpeed());
	  Serial.printf("[init]Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
	  if(ideSize != realSize) {
		  Serial.printf("[init]Flash Chip configuration wrong!\n");
	  } else {
		  Serial.printf("[init]Flash Chip configuration ok.\n");
	  }
		Serial.printf("\n Core version %s\n\n", ver);
		Serial.println();
		Serial.println("****************** Start core setup ******************");
#endif
	// Setup WiFi Handler
	wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
	delay(100);
	connectToWifi(theHostName);		//we are connecting to the wifi AP
	homeHtml = CONTROL_HTML1;
	homeHtml.replace("$AAA$", hostName);
	Serial.printf("Hostname is %s.local\n", hostName.c_str());
	// Handle OTA update from asynchronous callbacks
	Update.runAsync(true);
	// Setup WebSockets
	ws.onEvent(wsEvent);
	webServer.addHandler(&ws);
	initWebServer();

	wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWiFiDisconnect);
	webServer.begin();

	Serial.print(F("- Web Server started on port "));
	Serial.println(HTTP_PORT);
	Serial.printf("\n************END Core Setup Version %s,  boot:%i***************\n", ver, reboot);
}

/*
 * The loop method
 * 		returns true if firmware update is not ongoing
 * 		false otherwise
 */
bool Symphony::loop() {
	//DO NOT PUT A delay() AS IT CAUSES ERROR DURING OTA
	// Reboot handler
	if (reboot) {
		Serial.println("Rebooting...");
		delay(REBOOT_DELAY);
		ESP.restart();
	} else {
		//we process other items if it is not reboot mode
		dnsServer.processNextRequest();
	}
	if (!isUpdateFw) {
#ifdef DISCOVERABLE
		if (tries < 3) {
			sendIdentify(identifyTries[tries]);
			tries++;
		} else
			sendIdentify();	//this device sends discovery identify mode every 2mins, use sendIdentify(ms) if you want to override interval
#endif
		return true;
	} else {
		return false;
	}
}

/*
 * this enables the instantiator to set handler methods for the webserver
 */
void Symphony::on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction handler) {
	webServer.on(uri, method, handler);
}

void Symphony::serveStatic(const char* uri, fs::FS& fs, const char* path) {
	webServer.serveStatic(uri, fs, path);
}
/*
 *
 * This callback is called by the webSocketEvent function.  We can do manipulation of pins here.
 *
 */
void Symphony::setWsCallback(int (* Callback) (AsyncWebSocket ws, AsyncWebSocketClient *client, JsonObject& json)) {
	WsCallback = Callback;
}

/**
 * Private methods below
 */
void Symphony::setupAP() {
	Serial.println(F("Failed to connect as wifi client, going to softAP."));
	ap_ssid = "AP_"+hostName;
	WiFi.mode(WIFI_AP);
	/* Soft AP network parameters */
	IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
#ifdef DEBUG_ONLY
    Serial.printf("AP:%s, pk:%s, ip:%s\n",ap_ssid.c_str(),ap_passphrase.c_str(),apIP.toString().c_str());
#endif
    WiFi.softAP(ap_ssid.c_str(), ap_passphrase.c_str());

    /* Setup the DNS server redirecting all the domains to the apIP */
	dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
	dnsServer.start(DNS_PORT, "*", apIP);
	webServer.serveStatic("/generate_204", SPIFFS, "/admin.html");
	webServer.serveStatic("/fwlink", SPIFFS, "/admin.html");

	webServer.serveStatic("/", SPIFFS, "/admin.html");
	webServer.serveStatic("/wifi", SPIFFS, "/admin.html");
	webServer.serveStatic("/0wifi", SPIFFS, "/admin.html");
	webServer.serveStatic("/wifisave", SPIFFS, "/admin.html");
	webServer.serveStatic("/i", SPIFFS, "/admin.html");
	webServer.serveStatic("/r", SPIFFS, "/admin.html");
//	webServer.on("/generate_204", HTTP_ANY, [](AsyncWebServerRequest *request) {
//		request->send(200, "text/html", "Captive Portal");
//		});
//	webServer.on("/fwlink", HTTP_ANY, [](AsyncWebServerRequest *request) {
//		request->send(200, "text/html", "Captive Portal");
//		});
}
/**
 * Connect to the AP using the passkey
 */
void Symphony::connectToWifi(String theHostName) {
	// Switch to station mode and disconnect just in case
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	//sets the wifi login
	DynamicJsonBuffer jsonBuffer;
	JsonObject& json = jsonBuffer.parseObject(fManager.readConfig());
	if (!json.success()) {
		Serial.println("connectToWifi parseObject() failed");
	} else {
		if (json.containsKey("ssid")) {
			//ssid key is found, this means pwd and name are also there
			ssid = json["ssid"].as<String>();
			pwd = json["pwd"].as<String>();
			myName = json["name"].as<String>();
		}
#ifdef DEBUG_ONLY
		json.prettyPrintTo(Serial);
		Serial.printf("\nssid:%s pwd:%s\n", ssid.c_str(), pwd.c_str());
#endif
	}
	createMyName(theHostName);
	WiFi.begin(ssid.c_str(), pwd.c_str());
	int i = 0;
	//tries to connect to the AP
	while (WiFi.status() != WL_CONNECTED && i <= wifiMaxConnCount) {
		i++;
		delay(200);
	    Serial.print(".");
	}
	if (WiFi.status() != WL_CONNECTED) {
		setupAP();
	} else {
#ifdef DISCOVERABLE
	    startDiscovery(hostName);
#endif
	}
}
/*
 * this creates the unique name of this device based on its mac address
 * hostname will be myName, which is from config file read during connectToWifi
 * if there is no config file, it will be theHostName
 */
void Symphony::createMyName(String theHostName) {
	if (myName.length() == 0)
		myName = theHostName;
	hostName = myName;
	hostName.toLowerCase();
	myName += "_";
	// Generate device name based on MAC address
	uint8_t mac[6];
	WiFi.macAddress(mac);
	//	we generate the name based on the MAC values
	for (int i = 0; i < 6; ++i) {
		myName += String(mac[i], 16);
	}
}
/**
 * sets the product details of this device
 */
void Symphony::setProduct(Product p) {
	product = p;
	setRootProperties(product.stringify());
}
/**
 * sends broadcast message to all connected websocket clients
 */
void Symphony::textAll(JsonObject& message){
	size_t len = message.measureLength();
	AsyncWebSocketMessageBuffer * buffer = ws.makeBuffer(len);
	if (buffer) {
		message.printTo((char *)buffer->get(), len + 1);
		ws.textAll(buffer);
	}
}
/**
 * Sets the properties that will be displayed as root HTML as interpreted by js.loadDoc();
 */
void Symphony::doReboot() {
	reboot = true;
}
/**
 * Sets the properties that will be displayed as root HTML as interpreted by js.loadDoc();
 */
void Symphony::setRootProperties(String s) {
	Symphony::rootProperties = s;
}
/***********************************************************************
*					Utility classes
************************************************************************/
/*
 * This is the class for the holder of data received by Websocket
 * Data is of the format <ssid>=<value>
 *    ex 0006=1
 */
WsData::WsData(String data){
	deviceName = data.substring(0, data.indexOf(':'));
	value = data.substring(data.indexOf('=') + 1);
	ssid = data.substring(data.indexOf(':') + 1, data.indexOf('='));
}

String WsData::getDeviceName() {
	return deviceName;
}
String WsData::getSSID() {
	return ssid;
}
String WsData::getValue() {
	return value;
}
