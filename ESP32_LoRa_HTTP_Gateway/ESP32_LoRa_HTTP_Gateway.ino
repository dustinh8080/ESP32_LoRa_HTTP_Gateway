/********************************************************************************************************************
 * Code from https://www.aeq-web.com/selbstbau-https-lora-gateway-fur-iot-mit-esp32-lora-board/
 * edited by Dustin H. with codes from https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-post
 * for posting LoRa-data to webserver using http.
********************************************************************************************************************/


#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <LoRa.h>
#include <U8x8lib.h>

#define ss 18
#define rst 14
#define dio0 26
const char* ssid = "SSID"; //replace with your WiFi SSID
const char* password =  "Password"; //replace with your WiFi password
String gatewayid = "11AF12ADE32F"; //for authentication, doesn't matter at the moment 
String serverPath = "http://MyServer.com/cn/gateway.php"; //replace with your server-domain (or raspberry pi-ip with apache2/nginx installed)

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(15, 4, 16); //OLED LCD

String RxString, server_state;
int RxRSSI; boolean rx_fp;
long lastrx, crontimer, bctimer, wtimer;
  
void setup() {
  
  Serial.begin(9600);
  //-------------------Start OLED--------------------------
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.clearDisplay();
  u8x8.drawString(0, 1, "LoRa IoT Gateway");
  //-------------------Start OLED--------------------------
  delay(4000);   //Delay needed before calling the WiFi.begin
  //-------------------Start WiFi--------------------------
  u8x8.drawString(0, 3, "Connecting...");
  WiFi.begin(ssid, password); 
  
  while (WiFi.status() != WL_CONNECTED) { //Check for the connection
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  u8x8.clearDisplay();
  u8x8.drawString(0, 1, "WiFi connected");
  server_state = "wait...";
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.println("Starting LoRa...");
  //-------------------Start WiFi--------------------------

  //-------------------Start LoRA--------------------------
  SPI.begin(5, 19, 27, 18);
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(868E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("-------LoRa initialized-------");
  //-------------------Start LoRA--------------------------
  
}
  
void loop() {
  
 
  //-------------------LoRa Receiver-----------------------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("Receive LoRa Packet...");
    get_lora_data();
    call_https(RxString, RxRSSI, "gateway");
  }
  //-------------------LoRa Receiver-----------------------

  //-------------------Timer OLED Update-------------------
  if (millis() / 1000 > crontimer + 5) {
    crontimer = millis() / 1000;
    display_status();
  }
  //-------------------Timer OLED Update-------------------

  //-------------------Timer Send Beacon-------------------
  if (millis() / 1000 > bctimer + 300) {
    bctimer = millis() / 1000;
    call_https("beacon", 0, "beacon");
  }
  //-------------------Timer Send Beacon-------------------

  //-------------------Timer Check WiFi--------------------
  if (millis() / 1000 > wtimer + 600) {
    wtimer = millis() / 1000;
    check_wifi();
  }
  //-------------------Timer Check WiFi-------------------

  //-------------------Timer Reboot ESP-------------------
  if (millis() / 1000 > 90000) { //Reboot after a day
    ESP.restart();
  }
}

void get_lora_data() {
  boolean clr = 1;
  rx_fp = true;
  while (LoRa.available()) {
    if (clr == 1) {
      RxString = "";
      clr = 0;
    }
    RxString += (char)LoRa.read();
  }
  RxRSSI = LoRa.packetRssi();
  Serial.print("Received Packet: ");
  Serial.println(RxString);
  lastrx = millis() / 1000;
  clr = 1;
}


void call_https(String iotdata, int iotrssi, String reqtype) {
  Serial.println("Connect to server via port 80");
  int wifirssi = WiFi.RSSI();
  HTTPClient http;   
  if (!http.begin(serverPath)) {
    Serial.println("Connection failed!");
    server_state = "error";
  } else {
    Serial.println("Connected to server...");
    server_state = "online";
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postString = "data=" + iotdata + "&rssi=" + iotrssi + "&gwrssi=" + wifirssi + "&gwid=" + gatewayid + "&type=" + reqtype + "&end=end";
    Serial.println(postString);
    int httpResponseCode = http.POST(postString);
    Serial.println();
    Serial.println("-------Response Server-------");
    Serial.println();
    if(httpResponseCode>0){
  
    String response = http.getString();                       //Get the response to the request
  
    Serial.println(httpResponseCode);   //Print return code
    Serial.println(response);           //Print request answer
  
   }else{
  
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
  
   }
    http.end();  //Free resources
    delay(50);
  }
}

void display_status() {
  u8x8.clearDisplay();
  long lastdata = ((millis() / 1000) - lastrx);
  u8x8.setCursor(0, 1);
  u8x8.print("Lastrx: ");
  if(rx_fp == true){
  u8x8.print(lastdata);
  u8x8.print("s");
  }else{
  u8x8.print("none");  
  }
  u8x8.setCursor(0, 3);
  u8x8.print("Server: ");
  u8x8.print(server_state);
  u8x8.setCursor(0, 7);
  u8x8.print("RSSI: ");
  u8x8.print(RxRSSI);
  u8x8.print(" dBm");
}

void check_wifi() {
  int wifi_retry;
  while (WiFi.status() != WL_CONNECTED && wifi_retry < 5 ) {
    wifi_retry++;
    Serial.println("WiFi not connected. Try to reconnect");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    delay(5000);
  }
  if (wifi_retry >= 5) {
    Serial.println("Reboot");
    ESP.restart();
  }
}
