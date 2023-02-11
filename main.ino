 #define FILESYSTEM SPIFFS
// You only need to format the filesystem once
#define FORMAT_FILESYSTEM false
#define MDNS_DEVICE_NAME "audio-switch"
#define SERVICE_NAME "audio-switch"
#define SERVICE_PROTOCOL "udp"
#define SERVICE_PORT 5600

// Base lib
#include <arduino.h>

// Wifi
#include <WiFi.h>

// Web server libs (DNS, async server, json ...)
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Storage libs
#include <Preferences.h>
#include <SPIFFS.h>
#include <FS.h>

// RTC and timing libs
 #include <time.h>
 
// ----------- SETTINGS ---------------------------------------------------------------------------------------------------------------------------------------
const char * ssid = "your_wif_SSID";
const char * password = "your_password";
const char * host = "audio-switch"; // here the DNS name, you can edit it when you want.

const long  gmtOffset_sec = 3600; // Used to setup GMT Time, in seconds. By default it's setup to GMT+1. Only used for night off / on cycle
const int   daylightOffset_sec = 3600;

// ---------- PINOUT SETTINGS -----------------------------------------------------------------------------------------------------------------------------------------
#define SPEAKER1 12
#define SPEAKER2 14
#define SPEAKER3 27
#define SPEAKER4 26

#define INPUT1 25
#define INPUT2 33
#define INPUT3 32
#define INPUT4 18 // stay unused but can be implemented (this option need some hardware modifications)

// No limitation to pins usage, only the ESP32 limitation (ex : internal pull-up resistor)

// ---------- VARIABLES -----------------------------------------------------------------------------------------------------------------------------------------
typedef struct state {
    bool state_array[38];
    bool off_hour[24];
    
    String IN1_name;
    String IN2_name;
    String IN3_name;
    String IN4_name;

    String OUT1_name;
    String OUT2_name;
    String OUT3_name;
    String OUT4_name;

    bool HasBeenEdited;
    bool Mode;
    bool mtime;
    bool mtime2;

    bool NeedGoOn;
    bool NeedGoOff;
};

state board_state; // initializing the structure, globally. Will be store in EEPROM. 

const char * input_parameter1 = "id";
const char * input_parameter2 = "state";
const char * input_parameter3 = "name";
const char * input_parameter4 = "s";

String event_buffer;
char event_send[1024]; 
String HTTP_RES;
int HTTP_RES_ID;

hw_timer_t * timer = NULL;

const char* ntpServer = "pool.ntp.org";
struct tm timeinfo;



// --------- Alliases -----------------------------------------------------------------------------------------------------------------------------------------
Preferences pref;
#define DBG_OUTPUT_PORT Serial

AsyncWebServer server(80);
AsyncEventSource events("/sse");
File fsUploadFile;

//--------- FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------

void pins_init() {

    pinMode(SPEAKER1, OUTPUT);
    pinMode(SPEAKER2, OUTPUT);
    pinMode(SPEAKER3, OUTPUT);
    pinMode(SPEAKER4, OUTPUT);

    pinMode(INPUT1, OUTPUT);
    pinMode(INPUT2, OUTPUT);
    pinMode(INPUT3, OUTPUT);
    pinMode(INPUT4, OUTPUT);

    digitalWrite(SPEAKER1, LOW);
    digitalWrite(SPEAKER2, LOW);
    digitalWrite(SPEAKER3, LOW);
    digitalWrite(SPEAKER4, LOW);

    digitalWrite(INPUT1, LOW);
    digitalWrite(INPUT2, LOW);
    digitalWrite(INPUT3, LOW);
    digitalWrite(INPUT4, LOW);
}
String SetSpeakerOn(int ID) // activate a speaker and set in the board_state structure the actual state
{
    uint8_t ID_t;
    char buffer_name[10];
    if (ID == SPEAKER1) {ID_t = 1;}
    else if (ID == SPEAKER2) {ID_t = 2;}
    else if (ID == SPEAKER3) {ID_t = 3;}
    else if (ID == SPEAKER4) {ID_t = 4;}
    sprintf(buffer_name, "%s%d", "SPEAKER", ID_t);
    pref.putBool(buffer_name, true);
    board_state.state_array[ID] = true;
    if (board_state.Mode == true) {board_state.HasBeenEdited = true;}
    digitalWrite(ID, HIGH);
    return String(buffer_name);
}
String SetSpeakerOff(int ID) // desactivate a speaker and set in the board_state structure the actual state
{
    uint8_t ID_t;
    char buffer_name[10];
    if (ID == SPEAKER1) {ID_t = 1;}
    else if (ID == SPEAKER2) {ID_t = 2;}
    else if (ID == SPEAKER3) {ID_t = 3;}
    else if (ID == SPEAKER4) {ID_t = 4;}
    sprintf(buffer_name, "%s%d", "SPEAKER", ID_t);
    pref.putBool(buffer_name, false);
    board_state.state_array[ID] = false;
    if (board_state.Mode == true) {board_state.HasBeenEdited = true;}
    digitalWrite(ID, LOW);
    return String(buffer_name);
}
String SetInput(int ID) // Enable an input, and set all other to 0
{
    board_state.state_array[INPUT1] = false;
    board_state.state_array[INPUT2] = false;
    board_state.state_array[INPUT3] = false;
    board_state.state_array[INPUT4] = false;

    digitalWrite(INPUT1, LOW);
    digitalWrite(INPUT2, LOW);
    digitalWrite(INPUT3, LOW);
    digitalWrite(INPUT4, LOW);

    pref.putBool("INPUT1", false);
    pref.putBool("INPUT2", false);
    pref.putBool("INPUT3", false);
    pref.putBool("INPUT4", false);
    // the function begins with a reset of all inputs / state array / eeprom

    uint8_t ID_t;
    char buffer_name[10];
    if (ID == INPUT1) {ID_t = 1;}
    else if (ID == INPUT2) {ID_t = 2;}
    else if (ID == INPUT3) {ID_t = 3;}
    else if (ID == INPUT4) {ID_t = 4;}
    
    sprintf(buffer_name, "%s%d", "INPUT", ID_t);
    pref.putBool(buffer_name, true);
    board_state.state_array[ID] = true;
    
    if (board_state.Mode == true) {board_state.HasBeenEdited = true;}
    
    digitalWrite(ID, HIGH);
    return String(buffer_name);
    // activate the outputs and save in the rights places the data
}
void memory_init() {
    pref.begin("board_state", false);
}
void memory_read() // need to be done at start
{
    board_state.state_array[SPEAKER1] = pref.getBool("SPEAKER1", false);
    board_state.state_array[SPEAKER2] = pref.getBool("SPEAKER2", false);
    board_state.state_array[SPEAKER3] = pref.getBool("SPEAKER3", false);
    board_state.state_array[SPEAKER4] = pref.getBool("SPEAKER4", false);

    board_state.state_array[INPUT1] = pref.getBool("INPUT1", false);
    board_state.state_array[INPUT2] = pref.getBool("INPUT2", false);
    board_state.state_array[INPUT3] = pref.getBool("INPUT3", false);
    board_state.state_array[INPUT4] = pref.getBool("INPUT4", false);

    digitalWrite(SPEAKER1, board_state.state_array[SPEAKER1]);
    digitalWrite(SPEAKER2, board_state.state_array[SPEAKER2]);
    digitalWrite(SPEAKER3, board_state.state_array[SPEAKER3]);
    digitalWrite(SPEAKER4, board_state.state_array[SPEAKER4]);

    digitalWrite(INPUT1, board_state.state_array[INPUT1]);
    digitalWrite(INPUT2, board_state.state_array[INPUT2]);
    digitalWrite(INPUT3, board_state.state_array[INPUT3]);
    digitalWrite(INPUT4, board_state.state_array[INPUT4]);

    // writing ton IO port, to match the state array and output state.

    board_state.OUT1_name = pref.getString("OUT1_NAME");
    board_state.OUT2_name = pref.getString("OUT2_NAME");
    board_state.OUT3_name = pref.getString("OUT3_NAME");
    board_state.OUT4_name = pref.getString("OUT4_NAME");

    board_state.IN1_name = pref.getString("IN1_NAME");
    board_state.IN2_name = pref.getString("IN2_NAME");
    board_state.IN3_name = pref.getString("IN3_NAME");
    board_state.IN4_name = pref.getString("IN4_NAME");
  
    for (int HOUR = 0; HOUR < 24; HOUR++)
    {
      char buf[8];
      sprintf(buf, "HOUR%d", HOUR);
      board_state.off_hour[HOUR] = pref.getBool(buf, false);
    }
}

String GenerateJsonPackage()
{
  StaticJsonDocument<1024> doc;
            
  JsonObject speakers = doc.createNestedObject("speakers");
  speakers["1"] = board_state.state_array[SPEAKER1];
  speakers["2"] = board_state.state_array[SPEAKER2];
  speakers["3"] = board_state.state_array[SPEAKER3];
  speakers["4"] = board_state.state_array[SPEAKER4];

  JsonObject sources = doc.createNestedObject("sources");
  sources["1"] = board_state.state_array[INPUT1];
  sources["2"] = board_state.state_array[INPUT2];
  sources["3"] = board_state.state_array[INPUT3];
  sources["4"] = board_state.state_array[INPUT4];

  JsonObject speakerNames = doc.createNestedObject("speakerNames");
  speakerNames["1"] = board_state.OUT1_name;
  speakerNames["2"] = board_state.OUT2_name;
  speakerNames["3"] = board_state.OUT3_name;
  speakerNames["4"] = board_state.OUT4_name;

  JsonObject inputsNames = doc.createNestedObject("inputsNames");
  inputsNames["1"] = board_state.IN1_name;
  inputsNames["2"] = board_state.IN2_name;
  inputsNames["3"] = board_state.IN3_name;
  inputsNames["4"] = board_state.IN4_name;

  JsonObject schedules = doc.createNestedObject("schedules");
  char buf[8];
  for (int k = 0; k < 24; k++)
  {
    sprintf(buf, "%d", k);
    schedules[buf] = board_state.off_hour[k];
  }
 
  String event_buffer;
  serializeJson(doc, event_buffer);

  return event_buffer;
}

void IRAM_ATTR isOnorOff() // timer interrupt, place the system in eco mode or active mode according to schedules
{
  getLocalTime(&timeinfo);
  ets_printf("%d\n", timeinfo.tm_hour);
  
  if (board_state.off_hour[timeinfo.tm_hour] == true) // If the actual hour is on night mode
  {
    if (board_state.off_hour[timeinfo.tm_hour - 1] == false) // If the last hour was on active mode, we set the modification bit to false. 
    {
       if (board_state.mtime == false) // is executed only the first time, when enterring in the off mode
       {
          board_state.Mode = true;
          board_state.HasBeenEdited = false;
          board_state.mtime = true; // prevent from resetting the modification bit detector every 5mn in the first HOUR
          board_state.mtime2 = false;

          board_state.NeedGoOff = true;
          ets_printf("Mode OFF activé \n");
       }
    }
  }
  else
  {
    if (board_state.HasBeenEdited == false)
    {
      if (board_state.mtime2 == false)
      {
        board_state.mtime = false;
        board_state.mtime2 = true;
        board_state.Mode = false;
        board_state.HasBeenEdited = false;

        board_state.NeedGoOn = true;   
        ets_printf("Mode OFF desactivé + paramètres resets \n"); 
      }
    }
    else 
      {
        if (board_state.mtime2 == false)
        {
          board_state.mtime = false;
          board_state.mtime2 = true;
          board_state.Mode = false;
          board_state.HasBeenEdited = false;
         
          ets_printf("Mode off desactivé + paramètres laissés tels quels \n");
        }
     }
  }
}

void SetName(int ID, String NAME)
{
  if (ID == SPEAKER1) {
    board_state.OUT1_name = NAME;
    pref.putString("OUT1_NAME", board_state.OUT1_name);
  }
  else if (ID == SPEAKER2) {
    board_state.OUT2_name = NAME;
    pref.putString("OUT2_NAME", board_state.OUT2_name);
  }
  else if (ID == SPEAKER3) {
    board_state.OUT3_name = NAME;
    pref.putString("OUT3_NAME", board_state.OUT3_name);
  }
  else if (ID == SPEAKER4) {
    board_state.OUT4_name = NAME;
    pref.putString("OUT4_NAME", board_state.OUT4_name);
  }
  else if (ID == INPUT1) {
    board_state.IN1_name = NAME;
    pref.putString("IN1_NAME", board_state.IN1_name);
  }
  else if (ID == INPUT2) {
    board_state.IN2_name = NAME;
    pref.putString("IN2_NAME", board_state.IN2_name);
  }
  else if (ID == INPUT3) {
    board_state.IN3_name = NAME;
    pref.putString("IN3_NAME", board_state.IN3_name);
  }
  else if (ID == INPUT4) {
    board_state.IN4_name = NAME;
    pref.putString("IN4_NAME", board_state.IN4_name);
  }
  if (board_state.Mode == 1) {board_state.HasBeenEdited = true;}
}

void InitNames()
{
  board_state.OUT1_name = "Sortie 1";
  board_state.OUT2_name = "Sortie 2";
  board_state.OUT3_name = "Sortie 3";
  board_state.OUT4_name = "Sortie 4";
  
  board_state.IN1_name = "Entrée 1";
  board_state.IN2_name = "Entrée 2";
  board_state.IN3_name = "Entrée 3";
  board_state.IN4_name = "Entrée 4";

  pref.putString("OUT1_NAME", board_state.OUT1_name);
  pref.putString("OUT2_NAME", board_state.OUT2_name);
  pref.putString("OUT3_NAME", board_state.OUT3_name);
  pref.putString("OUT4_NAME", board_state.OUT4_name);

  pref.putString("IN1_NAME", board_state.IN1_name);
  pref.putString("IN2_NAME", board_state.IN2_name);
  pref.putString("IN3_NAME", board_state.IN3_name);
  pref.putString("IN4_NAME", board_state.IN4_name);
}

void EnterOffMode()
{        
  pref.putBool("SPEAKER1_OFF", board_state.state_array[SPEAKER1]);
  pref.putBool("SPEAKER2_OFF", board_state.state_array[SPEAKER2]);
  pref.putBool("SPEAKER3_OFF", board_state.state_array[SPEAKER3]);
  pref.putBool("SPEAKER4_OFF", board_state.state_array[SPEAKER4]);
  pref.putBool("INPUT1_OFF", board_state.state_array[INPUT1]);
  pref.putBool("INPUT2_OFF", board_state.state_array[INPUT2]);
  pref.putBool("INPUT3_OFF", board_state.state_array[INPUT3]);
  pref.putBool("INPUT4_OFF", board_state.state_array[INPUT4]); // we save the actual state under a specific key

  board_state.state_array[SPEAKER1] = false;
  digitalWrite(SPEAKER1, LOW);
  board_state.state_array[SPEAKER2] = false;
  digitalWrite(SPEAKER2, LOW);
  board_state.state_array[SPEAKER3] = false; 
  digitalWrite(SPEAKER3, LOW);
  board_state.state_array[SPEAKER4] = false;
  digitalWrite(SPEAKER4, LOW);
  board_state.state_array[INPUT1] = false;
  digitalWrite(INPUT1, LOW);
  board_state.state_array[INPUT2] = false;
  digitalWrite(INPUT2, LOW);
  board_state.state_array[INPUT3] = false;
  digitalWrite(INPUT3, LOW);
  board_state.state_array[INPUT4] = false;
  digitalWrite(INPUT4, LOW); // setting to 0 all the I/O and actualizing the board_state array     
}
void LeaveOffMode()
{
  board_state.state_array[SPEAKER1] = pref.getBool("SPEAKER1_OFF", false);
  board_state.state_array[SPEAKER2] = pref.getBool("SPEAKER2_OFF", false);
  board_state.state_array[SPEAKER3] = pref.getBool("SPEAKER3_OFF", false);
  board_state.state_array[SPEAKER4] = pref.getBool("SPEAKER4_OFF", false);
  board_state.state_array[INPUT1] = pref.getBool("INPUT1_OFF", false);
  board_state.state_array[INPUT2] = pref.getBool("INPUT2_OFF", false);
  board_state.state_array[INPUT3] = pref.getBool("INPUT3_OFF", false);
  board_state.state_array[INPUT4] = pref.getBool("INPUT4_OFF", false);

  digitalWrite(SPEAKER1, board_state.state_array[SPEAKER1]);
  digitalWrite(SPEAKER2, board_state.state_array[SPEAKER2]);
  digitalWrite(SPEAKER3, board_state.state_array[SPEAKER3]);
  digitalWrite(SPEAKER4, board_state.state_array[SPEAKER4]);
  digitalWrite(INPUT1, board_state.state_array[INPUT1]);
  digitalWrite(INPUT2, board_state.state_array[INPUT2]);
  digitalWrite(INPUT3, board_state.state_array[INPUT3]);
  digitalWrite(INPUT4, board_state.state_array[INPUT4]);
}
void MemRes()
{
  for (uint8_t HOUR = 0; HOUR < 24; HOUR++)
    {
      char buf[8];
      sprintf(buf, "HOUR%d", HOUR);
      pref.putBool(buf, false);
      board_state.off_hour[HOUR] = false;

      pref.putBool("SPEAKER1_OFF", (bool)false);
      pref.putBool("SPEAKER2_OFF", (bool)false);
      pref.putBool("SPEAKER3_OFF", (bool)false);
      pref.putBool("SPEAKER4_OFF", (bool)false);
      pref.putBool("INPUT1_OFF", (bool)false);
      pref.putBool("INPUT2_OFF", (bool)false);
      pref.putBool("INPUT3_OFF", (bool)false);
      pref.putBool("INPUT4_OFF", (bool)false);

      pref.putBool("SPEAKER1", false);
      pref.putBool("SPEAKER2", false);
      pref.putBool("SPEAKER3", false);
      pref.putBool("SPEAKER4", false);

      pref.putBool("INPUT1", false);
      pref.putBool("INPUT2", false);
      pref.putBool("INPUT3", false);
      pref.putBool("INPUT4", false);

      InitNames();
    }
}

// ------------ MAIN CODE ---------------------------------------------------------------------------------------------------------------------------------------------
void setup() {
    // put your setup code here, to run once:
    /*
    pref.putBool("SPEAKER1_OFF", (bool)false);
    pref.putBool("SPEAKER2_OFF", (bool)false);
    pref.putBool("SPEAKER3_OFF", (bool)false);
    pref.putBool("SPEAKER4_OFF", (bool)false);
    pref.putBool("INPUT1_OFF", (bool)false);
    pref.putBool("INPUT2_OFF", (bool)false);
    pref.putBool("INPUT3_OFF", (bool)false);
    pref.putBool("INPUT4_OFF", (bool)false);

    for (uint8_t HOUR = 0; HOUR < 24; HOUR++)
    {
      char buf[8];
      sprintf(buf, "HOUR%d", HOUR);
      pref.putBool(buf, false);
    }
*/
    pins_init();
    memory_init();
    memory_read();

    //InitNames(); // Use this function if you want to set as default all names (Sortie 1, Entrée 1...)
    
    SPIFFS.begin();
    MDNS.begin(MDNS_DEVICE_NAME);
    MDNS.addService(SERVICE_NAME, SERVICE_PROTOCOL, SERVICE_PORT);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    board_state.HasBeenEdited = 0;

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &isOnorOff, true);
    timerAlarmWrite(timer, 15000000, true); // actually 15s, in the future will be something like 5 Mins
    timerAlarmEnable(timer);

    // ISR every 5 minutes, to get the hour and check if night mode is enabled or not

    DBG_OUTPUT_PORT.begin(115200);
    delay(10);
    // We start by connecting to a WiFi network
    DBG_OUTPUT_PORT.println();
    DBG_OUTPUT_PORT.println();
    DBG_OUTPUT_PORT.print("Connecting to ");
    DBG_OUTPUT_PORT.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DBG_OUTPUT_PORT.print(".");
    }

    DBG_OUTPUT_PORT.println("");
    DBG_OUTPUT_PORT.println("WiFi connected.");
    DBG_OUTPUT_PORT.println("IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.localIP());

    MDNS.begin(host);
    DBG_OUTPUT_PORT.print("Open http://");
    DBG_OUTPUT_PORT.print(host);
    DBG_OUTPUT_PORT.println("/ to access to the main page");
    DBG_OUTPUT_PORT.println("HTTP server started");
    DBG_OUTPUT_PORT.print("\n");

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    server.begin();

    server.on("/hello", HTTP_GET, [](AsyncWebServerRequest * request) {
        request -> send(200, "text/plain", "hello world");
    });
    
    server.on("/out", HTTP_GET, [](AsyncWebServerRequest * request) {
        String SPEAKER_ID;
        String STATE;
        if (request -> hasParam(input_parameter1) && request -> hasParam(input_parameter2)) {
            SPEAKER_ID = request -> getParam(input_parameter1) -> value();
            STATE = request -> getParam(input_parameter2) -> value();
            if (STATE == "on") {
                if (SPEAKER_ID == "1") {SetSpeakerOn(SPEAKER1);}
                else if (SPEAKER_ID == "2") {SetSpeakerOn(SPEAKER2);}
                else if (SPEAKER_ID == "3") {SetSpeakerOn(SPEAKER3);}
                else if (SPEAKER_ID == "4") {SetSpeakerOn(SPEAKER4);}
            } else if (STATE == "off") {
                if (SPEAKER_ID == "1") {SetSpeakerOff(SPEAKER1);}
                else if (SPEAKER_ID == "2") {SetSpeakerOff(SPEAKER2);}
                else if (SPEAKER_ID == "3") {SetSpeakerOff(SPEAKER3);}
                else if (SPEAKER_ID == "4") {SetSpeakerOff(SPEAKER4);}
            }
            // send event to client (JSON-formatted)
            event_buffer = GenerateJsonPackage();
            event_buffer.toCharArray(event_send, event_buffer.length()+1);
            
            events.send(event_send, "speaker", millis());
        }
        HTTP_RES_ID = 200;
        HTTP_RES = "OK";
        AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
        request -> send(res);
    });

    events.onConnect([](AsyncEventSourceClient * client) {
        if (client -> lastId()) {
            DBG_OUTPUT_PORT.print("Client Reconnected");
        }
        event_buffer = GenerateJsonPackage();
            
        event_buffer.toCharArray(event_send, event_buffer.length()+1);
                
        client -> send(event_send, "speaker" , millis());
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/index.html", "text/html");
    });
    server.on("/assets/index-cc179eba.js", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/assets/index-cc179eba.js", "text/javascript");
    });
    server.on("/assets/index-4decc29d.css", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/assets/index-4decc29d.css", "text/css");
    });
    server.on("/vite.svg", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/vite.svg", "image/svg+xml");
    });
    server.addHandler( & events);

    server.on("/in", HTTP_GET, [](AsyncWebServerRequest * request) {
        String INPUT_ID;
        if (request -> hasParam(input_parameter1)) {
            INPUT_ID = request -> getParam(input_parameter1) -> value();
            if (INPUT_ID == "1") {SetInput(INPUT1);}
            else if (INPUT_ID == "2") {SetInput(INPUT2);}
            else if (INPUT_ID == "3") {SetInput(INPUT3);}
            else if (INPUT_ID == "4") {SetInput(INPUT4);}
            
            // send event to client (JSON-formatted)
            event_buffer = GenerateJsonPackage();
            event_buffer.toCharArray(event_send, event_buffer.length()+1);
            
            events.send(event_send, "speaker", millis());
        }
        HTTP_RES_ID = 200;
        HTTP_RES = "OK";
        AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
        request -> send(res);
    });

    server.on("/set-input-name", HTTP_GET, [](AsyncWebServerRequest * request) {
        String INPUT_ID;
        String NAME;
        if (request -> hasParam(input_parameter1) && request -> hasParam(input_parameter3)) {
            INPUT_ID = request -> getParam(input_parameter1) -> value();
            NAME = request -> getParam(input_parameter3) -> value();

            if (NAME == "") {HTTP_RES_ID = 400; HTTP_RES = "Nom nul ou invalide"; return;}
            if (INPUT_ID == "1")      {SetName(INPUT1, NAME);}
            else if (INPUT_ID == "2") {SetName(INPUT2, NAME);}
            else if (INPUT_ID == "3") {SetName(INPUT3, NAME);}
            else if (INPUT_ID == "4") {SetName(INPUT4, NAME);}
            HTTP_RES_ID = 200;
            HTTP_RES = "OK";
            
            // send event to client (JSON-formatted)
            event_buffer = GenerateJsonPackage();
            event_buffer.toCharArray(event_send, event_buffer.length()+1);
            
            events.send(event_send, "speaker", millis());
        }
        AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
        request -> send(res);
    });
     server.on("/set-speaker-name", HTTP_GET, [](AsyncWebServerRequest * request) {
        String SPEAKER_ID;
        String NAME;
        if (request -> hasParam(input_parameter1) && request -> hasParam(input_parameter3)) {
            SPEAKER_ID = request -> getParam(input_parameter1) -> value();
            NAME = request -> getParam(input_parameter3) -> value();

            if (NAME == "") {HTTP_RES_ID = 400; HTTP_RES = "Nom nul ou invalide"; return;}            
            if (SPEAKER_ID == "1")      {SetName(SPEAKER1, NAME);}
            else if (SPEAKER_ID == "2") {SetName(SPEAKER2, NAME);}
            else if (SPEAKER_ID == "3") {SetName(SPEAKER3, NAME);}
            else if (SPEAKER_ID == "4") {SetName(SPEAKER4, NAME);}
            HTTP_RES_ID = 200;
            HTTP_RES = "OK";
      
            // send event to client (JSON-formatted)
            event_buffer = GenerateJsonPackage();
            event_buffer.toCharArray(event_send, event_buffer.length()+1);
            
            events.send(event_send, "speaker", millis());
        }
        AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
        request -> send(res);
    });

       server.on("/set-schedule", HTTP_GET, [](AsyncWebServerRequest * request) {
        String HOUR;
        String STATE;
        if (request -> hasParam(input_parameter4) && request -> hasParam(input_parameter2)) {
            HOUR = request -> getParam(input_parameter4) -> value();
            STATE = request -> getParam(input_parameter2) -> value();
          
            uint8_t HOUR_t;
            //String buf;
            char tmp[10];
            HOUR_t = HOUR.toInt();

            bool state_buf;
            if (STATE == "enabled")
            {
              board_state.off_hour[HOUR_t] = true;
              sprintf(tmp, "HOUR%d", HOUR_t);
              pref.putBool(tmp, (bool)true);
            }
            else if (STATE == "disabled")
            {
              board_state.off_hour[HOUR_t] = false;
              sprintf(tmp, "HOUR%d", HOUR_t);
              pref.putBool(tmp, (bool)false);
            }
 
            // send event to client (JSON-formatted)
            
            event_buffer = GenerateJsonPackage();
            event_buffer.toCharArray(event_send, event_buffer.length()+1);
            
            events.send(event_send, "speaker", millis());
        }
        HTTP_RES_ID = 200;
        HTTP_RES = "OK";
        AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
        request -> send(res);
    });
    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
      HTTP_RES_ID = 200;
      HTTP_RES = "redémarrage imminent";
      AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
      request -> send(res);
      delay(500);
      ESP.restart();
    });
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
      HTTP_RES_ID = 200;
      HTTP_RES = "Les paramètres par defaut ont été appliqués";
      MemRes();
      memory_read();
      AsyncWebServerResponse * res = request -> beginResponse(HTTP_RES_ID, "text/plain", HTTP_RES);
      request -> send(res);
    });
}

void loop() 
{
   if (board_state.NeedGoOff == true)
   {
      EnterOffMode();
      board_state.NeedGoOff = false;
      
      event_buffer = GenerateJsonPackage();
      event_buffer.toCharArray(event_send, event_buffer.length()+1);    
      events.send(event_send, "speaker", millis());
   }
   if (board_state.NeedGoOn == true)
   {
      LeaveOffMode();
      board_state.NeedGoOn = false;
      
      event_buffer = GenerateJsonPackage();
      event_buffer.toCharArray(event_send, event_buffer.length()+1);
      events.send(event_send, "speaker", millis());
   }  
}
