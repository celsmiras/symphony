/**
 * This is the core for the Symphony Home
 *
 *
 */

#ifndef SYMPHONY_H_
#define SYMPHONY_H_

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

#include "FileManager.h"
#include "html.h"
#include "Product.h"

#define DEBUG_ONLY
//#define SHOW_FLASH
#define FW_VERSION 1252
#define HTTP_PORT 80      /* Default web server port */
#define CONNECT_TIMEOUT 15000   /* 15 seconds */
#define REBOOT_DELAY    500     /* Delay for rebooting once reboot flag is set */

#define  CORE_INIT 1
#define  CORE_COMMIT 2
#define  CORE_DELETE 3
#define  CORE_GETDEVICEINFO 4
#define  CORE_VALUES 20
#define  CORE_CONTROL 7

class Symphony {
	public:

		//constructor
		Symphony();

	    //public attributes
	    static String rootProperties;//the properties of the form {typ:'Rad',lbl:'RED',val:'0007', grp:'g2'}
	    static String hostName;//the hostName of this device
	    static String myName;//the name of this device that includes the mac address
	    static Product product;

	    //public methods
	    void setup(String theHostName);
	    bool loop();
	    //lets the instantiator if this Symphony object assign a callback handler
	    void on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction handler);
	    void serveStatic(const char* uri, fs::FS& fs, const char* path);
	    void setWsCallback(int (* WsCallback) (AsyncWebSocket ws, AsyncWebSocketClient *client, JsonObject& json));
	    void textAll(JsonObject& message);
	    static void setRootProperties(String s);
	    void setProduct(Product p);
	    void doReboot();

	private:
	    String ap_ssid, ap_passphrase = "12345678";
	    IPAddress apIP = IPAddress (192, 168, 7, 1);
	    String ssid = "bahay", pwd = "carlopiadredcels";
	    int wifiMaxConnCount=50;  //max counter when connecting to wifi AP, corresponds to 10secs
	    long restartTimer = 0;  //the restart timer in millis.  this will restart every maxrestartTimer if wifi is not connected.
	    const long maxRestartTimer = 120000; //the max millis before restart.  2 mins
//	    String hostName = "Symphony";
	    void connectToWifi(String theHostName);
	    void createMyName(String theHostName);
	    void setupAP();
};

/**
 * utility class to parse the data sent during control transactions
 */
class WsData
{
	public:
		WsData(String data);
		String getDeviceName();
		String getSSID();
		String getValue();
	private:
		String deviceName;
		String ssid;
		String value;
};

#endif /* SYMPHONY_H_ */
