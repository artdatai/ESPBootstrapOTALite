

#include "ESP8266BootstrapLite.h"

#define SAVED_NETCONFIGS_FILE "/savednets.csv"

#define BOOTLITE_CONFIG_URI "/configs"

//ESP8266WebServer* server;

/**
*  Developer must set access point ssid and password. This is required when ESP8266 goes to softAP mode.
*/
ESP8266BootstrapLite::ESP8266BootstrapLite(char* _ap_ssid, char* _ap_password, char* _ap_token){
	this->_ap_ssid 			= _ap_ssid;
	this->_ap_password 		= _ap_password;
	this->_ap_token 		= _ap_token;
	server 					= new ESP8266WebServer(80);
	state 					= STATE_READY;
	_wc_attempts 			= 3;
}

/*
* Default destructor
*/
ESP8266BootstrapLite::~ESP8266BootstrapLite(void){}


/*
* This function initializes Serial, SPIFFS and Debug fcreds.
*/
bool ESP8266BootstrapLite::begin(){

	//ToDo : Check available size of SPIFF
	
	//Serial.begin(115200);


	DEBUG_PRINTLN("[INFO begin] Mounting SPIFFS\n");
	
	if(SPIFFS.begin()){

		DEBUG_PRINTLN("[INFO begin] Checking if configs file exists.\n");
		
		if(SPIFFS.exists(SAVED_NETCONFIGS_FILE)){
			
			File f = SPIFFS.open(SAVED_NETCONFIGS_FILE, "r");
			
			if(f && f.size() > 0){

				DEBUG_PRINTLN("[INFO begin] Wifi credentials are available. Will try to connect using these credentials.\n");

				state = STATE_WIFI_CONNECT;

				f.close();
			
			}else{
			
				DEBUG_PRINTLN("[INFO begin] No Wifi credentials are saved yet.\n");

				state = STATE_ACCESS_POINT_CONNECT;
			}
		
		}else{

			DEBUG_PRINTLN("[INFO begin] Device booting for the first time. Creating required files to save wifi credentials.\n");
			
			File f = SPIFFS.open(SAVED_NETCONFIGS_FILE ,"w");

			state = STATE_ACCESS_POINT_CONNECT;

			f.close();
		}

		return true;

	}else{

		DEBUG_PRINTLN("[ERROR begin] SPIFFS did not mount.\n");
	
	}
	return false;
}

/*
* This function disconnects Serial, SPIFF, Debug and Wifi settings and prepare ESP for reboot.
*/
void ESP8266BootstrapLite::end(bool reboot = false){

	teardownWifi();

	if(reboot){
		
		ESP.restart();
	}
}

/*
*
*/
ESPBootstrapError ESP8266BootstrapLite::bootstrap(){

	ESPBootstrapError err = NO_ERROR;

	switch(state){

		case STATE_READY: 

			begin();

			DEBUG_PRINTLN("[INFO bootsrap] current state = STATE_READ\n");
		
		break;
		
		case STATE_WIFI_CONNECT: 

			DEBUG_PRINTLN("[INFO bootsrap] current state = STATE_WIFI_CONNECT\n");
			
			err =  attemptConnectToNearbyWifi(); // Create a loop to attemp 3 times if necessary.

			if( err == ERROR_WIFI_CONNECT ) state = STATE_ACCESS_POINT_CONNECT;

			//if( err == NO_ERROR ) state = STATE_WIFI_ACTIVE;
		
		break;

		case STATE_WIFI_ACTIVE:

			DEBUG_PRINTLN("[INFO bootsrap] current state = STATE_WIFI_ACTIVE\n");

			err = NO_ERROR;

		break;
		
		case STATE_ACCESS_POINT_CONNECT: 

			err = startSoftAP();

			DEBUG_PRINTLN("[Info bootsrap] current state = STATE_ACCESS_POINT_CONNECT\n");
		
		break;
		
		case STATE_ACCESS_POINT_ACTIVE:

			DEBUG_PRINTLN("[Info bootsrap] current state = STATE_ACCESS_POINT_ACTIVE\n");
			
			while(state == STATE_ACCESS_POINT_ACTIVE){ 
				
				server->handleClient(); 
			}
		
		break;
		
		case STATE_SLEEP:

			DEBUG_PRINTLN("[Info bootsrap] current state = STATE_SLEEP\n");

			err = NO_ERROR;

		break;
	}

	return err;
}


/*
*
*/
ESPBootstrapError ESP8266BootstrapLite::attemptConnectToNearbyWifi(){
	
	DEBUG_PRINTLN("[INFO NearbyWifi] Attempting to connect to wifi in vicinity with credentials stored on ESP.\n");

	teardownWifi();

	WiFi.mode(WIFI_STA);
	
	int numOfNetworks = WiFi.scanNetworks();

	bool found = false;
	String ssid = "\0";
	String pass = "\0";

	if(numOfNetworks == 0){
		
		DEBUG_PRINTLN("[INFO NearbyWifi] No networks found in the vicinity.\n");
		
		//state = STATE_ACCESS_POINT_CONNECT;
		
		return ERROR_WIFI_CONNECT;
	}

	DEBUG_PRINTLN("[INFO NearbyWifi] Networks found in the vicinity.\n");

	File fcreds = SPIFFS.open(SAVED_NETCONFIGS_FILE, "r");

	if(!fcreds){
		
		DEBUG_PRINTLN("[ERROR NearbyWifi] Failed to open saved networks files.\n");
		
		//state = STATE_ACCESS_POINT_CONNECT;
		
		return ERROR_WIFI_CONNECT;
	}

	while(fcreds.available()){

		//ssid and password are stored in separate lines to avoid complexity of the program
		ssid = fcreds.readStringUntil('\n');
		pass = fcreds.readStringUntil('\n');

		ssid = ssid.substring(0, ssid.length()-1);
		pass = pass.substring(0, pass.length()-1);

		DEBUG_PRINTLN("Length of ssid = %d", ssid.length());

		for(int i = 0 ; i < numOfNetworks; i++){

			DEBUG_PRINTLN("Checking if saved_ssid : %s == ssid : %s. length = %d.\n", &ssid[0], &((WiFi.SSID(i))[0]), WiFi.SSID(i).length() );

			if(ssid == WiFi.SSID(i)){
				found = true;
				break;
			}
		}

		if(found) break;
	}

	if(found){
		
		DEBUG_PRINTLN("[INFO NearbyWifi] Trying to connect to SSID = %s .\n", &ssid[0]);
		
		return connectToWifi(ssid, pass); // Beware, need to check if the string contains "\n" at the end
	}
	
	DEBUG_PRINTLN("[INFO NearbyWifi] No match found in stored networks.\n");
	
	//state = STATE_ACCESS_POINT_CONNECT;
	
	return ERROR_WIFI_CONNECT;

}



/*
*
*/
void ESP8266BootstrapLite::handleConfig(){

	String ssid, password, token;

	if(server->hasArg("token")){

		token = server->arg("token");

		if(token == _ap_token) {
			
			if(server->hasArg("ssid") && server->hasArg("password")){
			    
			    ssid = server->arg("ssid");
			    
			    password = server->arg("password");
			    
			    DEBUG_PRINTLN("[INFO ap_handler]Received Args");

			    server->send(200, "text/plain", "Got configuration params");

			    delay(BOOTLITE_DELAY_TIME);

			    ESPBootstrapError err = connectToWifi(ssid, password);

			    if(err == NO_ERROR){

			    	storeWifiConfInSPIFF(ssid, password);

			    } else {

			    	DEBUG_PRINTLN("[ERROR ap_handler] Failed to connect to Wifi with ssid %s.\n", &ssid[0]);

			    	state = STATE_ACCESS_POINT_CONNECT;
			    }

			 } else {

			 	DEBUG_PRINTLN("[ERROR ap_handler] ssid or password field not found.\n");

			 	state = STATE_ACCESS_POINT_CONNECT;

			 	server->send(400, "text/plain", "Bad request");

			 }

		} else {

			DEBUG_PRINTLN("[ERROR ap_handler] Secure token didn't match. Please try to send the request again.\n");

			state = STATE_ACCESS_POINT_CONNECT;
			
			server->send(400, "text/plain", "Bad request");
		}
	} else {
		
		DEBUG_PRINTLN("[ERROR ap_handler] Secure token not found as parameter. Please try to send the request again.\n");

		state = STATE_ACCESS_POINT_CONNECT;
		
		server->send(400, "text/plain", "Bad request");

	}

}

/*
*
*/
void ESP8266BootstrapLite::handleNotFound(){
  
  String message = "Resource Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += ( server->method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";

  for( uint8_t i = 0; i < server->args(); i++ ) {

    message += " " + server->argName ( i ) + ": " + server->arg ( i ) + "\n";

  }

  server->send ( 404, "text/plain", message );
}

/*
*
*/
ESPBootstrapError ESP8266BootstrapLite::startSoftAP() {

	DEBUG_PRINTLN("[INFO soft_ap] Starting Access point mode.\n");

	state = STATE_ACCESS_POINT_ACTIVE;

	teardownWifi();

	WiFi.mode(WIFI_AP);

	WiFi.softAP( _ap_ssid, _ap_password );

	DEBUG_PRINTLN("[INFO soft_ap] Access Point IP address is : %s.\n", &((WiFi.softAPIP().toString())[0]));

	server->on(BOOTLITE_CONFIG_URI, HTTP_GET, [this](){ handleConfig(); });

	server->onNotFound([this](){ handleNotFound(); });

	server->begin();

	DEBUG_PRINTLN("[soft_ap] Returning from function.\n");

	return NO_ERROR;

}

/*
*
*/
void ESP8266BootstrapLite::storeWifiConfInSPIFF(String ssid, String password) const{

	//ToDo : Check available size of SPIFF

	if(SPIFFS.begin()){

		if(SPIFFS.exists(SAVED_NETCONFIGS_FILE)){

			File f = SPIFFS.open(SAVED_NETCONFIGS_FILE, "w+");

			if(f){
				
				bool found = false;

				while(f.available()){

					if(ssid == f.readStringUntil('\n')){
						
						f.println(password);

						DEBUG_PRINTLN("[INFO store_wifi] SSID %s already exists. Overwriting password for ssid.\n", &ssid[0]);
						
						found = true;
						
						break;
					
					}else{

						String pass = f.readStringUntil('\n');
					}

				}
				
				if(!found){
					
					f.println(ssid);
					f.println(password);

					DEBUG_PRINTLN("[INFO store_wifi] configuration saved.\n");
				}

				f.close();
				
			}else{

				DEBUG_PRINTLN("[INFO store_wifi] File error. Cannot store the Wifi credentials.\n");
			}
		} else{

			DEBUG_PRINTLN("[INFO store_wifi] Config file not found.\n");
		}

	} else {

		DEBUG_PRINTLN("[INFO store_wifi] SPIFFS did not mount correctly. Cannot store the Wifi credentials.\n");
	}
}

/*
*
*/
ESPBootstrapError ESP8266BootstrapLite::connectToWifi(String ssid, String password){

	
	if( (ssid == "\0" || ssid.length() == 0) || (password == "\0" || password.length() == 0) ){ 

		DEBUG_PRINTLN("[ERROR wifi_connect] Either ssid or password is empty.\n");

		return ERROR_WIFI_CONNECT;
	}

	teardownWifi();

	WiFi.mode(WIFI_STA);

	WiFi.begin(&ssid[0], &password[0]);

	DEBUG_PRINTLN("[INFO wifi_connect] Connecting to Wifi network with ssid = %s.\n", &ssid[0]);

	while(WiFi.status() != WL_CONNECTED){

		bool _connected = false;

		delay(BOOTLITE_DELAY_TIME * 5);

		switch(WiFi.status()){

			case WL_CONNECTED: 

				_connected = true;

			break;

			case WL_NO_SSID_AVAIL:

				DEBUG_PRINTLN("[ERROR wifi_connect] No SSID available. Please try again.\n");

				return ERROR_WIFI_CONNECT;

			break;

			case WL_CONNECT_FAILED:

				DEBUG_PRINTLN("[ERROR wifi_connect] Incorrect password. Please try again.\n");

				return ERROR_WIFI_CONNECT;

			break;

			case WL_DISCONNECTED:

				DEBUG_PRINTLN("[ERROR wifi_connect] Could not connect to Wifi. Either password is incorrect or Wifi router is OFF/Out-of-Range.\n");

				return ERROR_WIFI_CONNECT;

			break;

			default:

				DEBUG_PRINTLN(".");

			break;

		}

		if(_connected) break;
	}

	DEBUG_PRINTLN("[INFO wifi_connect] Connection successful.\n");

	DEBUG_PRINTLN("[INFO wifi_connect] IPAddress assigned to this device is %s.\n", &((WiFi.localIP().toString())[0]));

	state = STATE_WIFI_ACTIVE;

	return NO_ERROR;
}


/*
*
*/
//char* ESP8266BootstrapLite::getApSSID() const{ return _ap_ssid; }
	
/*
*
*/	
//char* ESP8266BootstrapLite::getApPassword() const{ return _ap_password; }


/*
*
*/	
ESPBootstrapState ESP8266BootstrapLite::getState() const{ return state; }


/*
*
*/	
void ESP8266BootstrapLite::setState(ESPBootstrapState state){ this->state = state; }


/*
*
*/
void ESP8266BootstrapLite::teardownWifi(){

	DEBUG_PRINTLN("[INFO teardown] Shutting down wifi and restarting it.\n");

	delay(BOOTLITE_DELAY_TIME);

	if(WiFi.isConnected()){

	    WiFi.softAPdisconnect();

	    WiFi.disconnect();

	    delay(BOOTLITE_DELAY_TIME);
  	}
}

