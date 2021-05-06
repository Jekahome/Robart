
/*
* 1.Настроить прием/передачу HTTP команд
* 2.Настроить работу SD card
* 3. Реализовать сохранение в файл данных микрофона
* 4.Отправить файл на сервер
*/
#include <ArduinoJson.h>
#include <SD.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <base64.h>
#include <FS.h>
//#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>


//#include <Wire.h>
//#include <Adafruit_MCP4725.h>
//Adafruit_MCP4725 dac;

const char *ssid = "Jeka";
                   const char *password = "******";

                   const char* host = "esp8266";
                   IPAddress addr = IPAddress(192,168,0,102);
IPAddress ip(192,168,0,102);  //статический IP
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
static byte port = 80;
ESP8266WebServer server(addr,port);
//ESP8266WebServer server(port);
void server_init();
void handleFileList();
void handleFileUpload();

enum Command
{
sound_rec = 1,
error_message = 2,
msg_log = 3
};
void command_handler();
void MAX9814_frequency();
void test_buffer();
boolean load_data_sound(/*fs::FS &fs*/);
void sound_recording();
//int rdataf(uint8_t *buffer, int len);
boolean sound_busy = false;

#define MAX9814 A0

//server adress
#define SERVER_IP "192.168.0.106:4000"
//const char *host = "192.168.0.106:4000/load_bytes";
                   boolean isSD=false;


                   byte * ptr_buff = nullptr;
                   byte * ptr_buff_head = nullptr;
                                          int size_buff = 15000;
                                          //char cstr[1200]="";
//byte glob_arr[16000]={0};

void myDelayMicroseconds(uint32_t us) {
uint32_t tmr = micros();
while (micros() - tmr < us);
}

void setup()
{
Serial.begin(115200);
//size_buff = 17000;//ESP.getFreeHeap()-10000;
//ptr_buff = (byte *)glob_arr;
ptr_buff = (byte *)malloc(size_buff);
ptr_buff_head = ptr_buff;
//free(ptr_buff);

delay(3000);
Serial.printf("\nSize heap %d\n",size_buff);
Serial.println("\nStart initialization Wemos D1 esp8266");
Serial.setDebugOutput(true);
//WIFI INIT
Serial.println("WIFI INIT");
if (String(WiFi.SSID()) != String(ssid)) {
// WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
WiFi.config(ip, gateway, subnet);
}
Serial.println("Connecting");
while (WiFi.status() != WL_CONNECTED)
{
Serial.print(".");
delay(500);
}
Serial.printf("\nConnected to %s network with IP Address: ",ssid);
Serial.print(WiFi.localIP());
Serial.printf(":%d\n",port);
/* if(MDNS.begin(host)) {
Serial.println("MDNS responder started");
Serial.println("----------------------------");
Serial.printf("|Open http://%s.local/|\n",host);
Serial.println("----------------------------");
}*/
WiFi.printDiag(Serial);

//SERVER INIT
Serial.println("STARTING THE SERVER");
server_init();

// SD INIT
Serial.print("Initialization SD ");
int count=0;
while(SD.begin(SS)==false){
delay(500);
Serial.print(".");
count++;
if(count>60){
Serial.println(" failed!");
return;
}
}
isSD=true;
Serial.println(" done.");

// Microphone INIT
Serial.println("Microphone MAX9814 INIT");
pinMode(MAX9814, INPUT);

Serial.print("\nChipId: ");Serial.println(ESP.getChipId());
Serial.print("FlashChipId: ");Serial.println(ESP.getFlashChipId());
                           Serial.print("FlashChipSize: ");Serial.println(ESP.getFlashChipSize());
                           Serial.print("FlashChipRealSize: ");Serial.println(ESP.getFlashChipRealSize());
                           Serial.print("FreeHeap: ");Serial.println(ESP.getFreeHeap());


}

void server_init(){
server.on("/command", HTTP_POST, command_handler);

server.on("/list", HTTP_GET, handleFileList);

server.on("/edit", HTTP_GET, []() {
server.send(404, "text/plain", "FileNotFound");
});

server.on("/edit", HTTP_POST, []() {
server.send(200, "text/plain", "");
}, handleFileUpload);

server.onNotFound([]() {
Serial.println(server.uri());
if (server.method() == HTTP_OPTIONS)
{
server.sendHeader("Access-Control-Allow-Origin", "*");
server.sendHeader("Access-Control-Max-Age", "10000");
server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
server.sendHeader("Access-Control-Allow-Headers", "*");
server.send(204);
}
else
{
server.send(404, "text/plain", "");
}
});

//get heap status, analog input value and all GPIO statuses in one json call
server.on("/all", HTTP_GET, []() {
String json = "{";
json += "\"heap\":" + String(ESP.getFreeHeap());
json += ", \"analog\":" + String(analogRead(A0));
json += ", \"gpio\":" + String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
json += "}";
server.send(200, "text/json", json);
json = String();
});
server.begin(port);
Serial.println("HTTP server started");
}

void command_handler(){
/*for(int i=0;i<5;i++){
           Serial.printf("i=%d ",i);
           Serial.println(server.argName(i));
           }
           */
           if(!server.hasArg("plain")){
           server.send(400, "text/plain", "Notfound 'plain'");
           return;
           }
String plain = server.arg("plain");
Serial.println(plain);

StaticJsonDocument<900> doc;
DeserializationError res_des = deserializeJson(doc, plain);

switch ( res_des.code() ) {
case DeserializationError::Ok:{
break;
}
case DeserializationError::IncompleteInput:{
server.send(400, "text/plain", "The end of the input is missing");
return;
break;
}
case DeserializationError::InvalidInput:{
server.send(400, "text/plain", "The input is not recognized");
return;
break;
}
case DeserializationError::NoMemory:{
server.send(400, "text/plain", "The JsonDocument is too small; you need to increase its capacity");
return;
break;
}
case DeserializationError::NotSupported:{
server.send(400, "text/plain", "The document included features not supported by the parser");
return;
break;
}
case DeserializationError::TooDeep:{
server.send(400, "text/plain", "The nesting limit was reached");
return;
break;
}
default:
server.send(400, "text/plain", "Invalid arg");
return;
break;
}
if ( doc["command"].isNull() == true ){
server.sendHeader("Access-Control-Allow-Origin", "*");
server.send(400, "text/plain", "Invalid arguments.Not found key 'command'");
return;
}
int command = doc["command"].as<byte>();

switch ( command ) {
case  Command::sound_rec:
{
// curl -d '{"command":1}' -H "Content-Type: application/json" 192.168.0.104:80/command

//MAX9814_frequency();
//test_buffer();
sound_recording();
break;
}
case  2:
{
// curl -d '{"command":2}' -H "Content-Type: application/json" 192.168.0.104:80/command


break;
}
case  3:
{
// curl -d '{"command":3}' -H "Content-Type: application/json" 192.168.0.104:80/command


break;
}
default:
{
String out = "Invalid arguments.\nExample request:\n";
out=out+"For sound recording => curl -d '{\"command\":1}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n";
server.sendHeader("Access-Control-Allow-Origin", "*");
server.send(400, "text/plain", out);
break;
}
}
}

void sound_recording() {
Serial.println("sound_recording");
if(sound_busy==false){
Serial.println("sound_busy false");
sound_busy = true;
if(load_data_sound()){
Serial.println("load_data_sound true");
WiFiClient client;
if (client.connect("192.168.0.106", 4000)) {
Serial.println("Wifi connected");
// String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"soundFile\"; filename=\"file_sound.txt\"\r\nContent-Type: text/plain\r\n\r\n";
String head = "--RandomNerdTutorials\r\n\r\n\r\n";
String tail = "\r\n--RandomNerdTutorials--\r\n";
File file_out = SD.open("/file_sound.txt",FILE_READ);

Serial.printf("ESP.getFreeHeap=%d\n",ESP.getFreeHeap());
// обнуление буффера
// попробовать
/*
memset(ptr, значение, количество) – заполняет область памяти, на которую указывает ptr,
байтами значение, в количестве количество штук.
Часто используется для задания начальных значений выделенной области памяти.
Внимание! Заполняем только байтами, 0-255.
*/
ptr_buff = ptr_buff_head;
memset(ptr_buff, 0, size_buff);
/* for(int i=0;i<size_buff;i++){
*ptr_buff=0;
if(i<size_buff-1)ptr_buff++;
}
ptr_buff = ptr_buff_head;
*/

/*size_t len = size_buff;//(int)(sizeof(buf) - 1);
if(size_buff  (int)(sizeof(buf) - 1)  > file_out.size()){
len = file_out.size();
}*/

size_t totalLen = head.length() + tail.length() + file_out.size();
Serial.printf("size_buff=%d file_out.size=%d kb totalLen=%D\n",size_buff,file_out.size()/1000,totalLen);
client.println(String("POST ") + "/sound" + " HTTP/1.1");
client.println(String("Host: ") + "192.168.0.106");
client.println(String("Content-Length: ") + String(totalLen));
client.println("Access-Control-Allow-Origin:*");
client.println(String("Content-Disposition: form-data; name=\"soundFile\"; filename=\"file_sound.txt\"")  );
client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");

client.println();
client.print(head);
Serial.println("HEAD true");

int get_read = 0;
//int get_write = 0;
while (file_out.available()) {
get_read = file_out.read((uint8_t *)ptr_buff, size_buff);
ptr_buff = ptr_buff_head;
client.write((const char *)ptr_buff, get_read);
//Serial.printf("get_read=%d get_write=%d\n",get_read,get_write);

// обнуление буффера
ptr_buff = ptr_buff_head;
memset(ptr_buff, 0, size_buff);
/*for(int i=0;i<size_buff;i++){
*ptr_buff=0;
if(i<size_buff-1)ptr_buff++;
}
ptr_buff = ptr_buff_head;
*/
}

file_out.close();

Serial.println("Content true");
client.print(tail);

sound_busy = false;

unsigned long timeout = millis();
while (client.available() == 0) {
if (millis() - timeout > 8000) {
client.stop();
Serial.println("client.stop");
server.send(200, "text/plain","timeout end");
return;
}
}
server.sendHeader("Access-Control-Allow-Origin", "*");
server.send(200, "text/plain","Ok");
}else{
Serial.println("No connect");
}
}
}else{
server.sendHeader("Access-Control-Allow-Origin", "*");
server.send(200, "text/plain","Microphone is busy");
return;
}
}


void test_buffer(){


/*
int buffer_size = 80000;

byte * ptr = (byte *)malloc(buffer_size);
while (ptr==NULL){
buffer_size=buffer_size-1000;
ptr = (byte *)malloc(buffer_size);
}
Serial.printf("Buffer size:%d\n",buffer_size);
                          */


                          /*
                          int buffer_size = ESP.getFreeHeap()/2;
                          byte * ptr = (byte *)malloc(buffer_size);
byte * ptr_start = ptr;
int bufferposition=0;
//unsigned long  TimeIntervalSmal = micros();
Serial.println("Start load ptr");
while( bufferposition < buffer_size ){
//if (micros() - TimeIntervalSmal >= 41){//60=12218 50=13966 40=16225 41=15954/41=15997(без map)
*ptr = (byte)map(analogRead(MAX9814), 0, 1023, 0, 255);// 0,2500,0,255
if(bufferposition < buffer_size){
ptr++;
}
bufferposition++;
// TimeIntervalSmal = micros();
//}
}
Serial.println("load ptr true");
Serial.printf("Data %d \n",(byte)*ptr);
*/

/*
byte * ptr = (byte *)malloc(5);
byte * ptr_start = ptr;
for(int i=0;i<5;i++){
*ptr=i*2;
ptr++;
}

for(int i=0;i<5;i++){
Serial.printf("Data %d \n",(byte)*ptr_start);
ptr_start++;
}
*/



/*
SD.remove("/test.txt");
File file = SD.open("/test.txt", FILE_WRITE);
String str_chank = "1";
file.print(str_chank);
file.flush();
file.print("2");
file.flush();
file.print("3");
file.flush();
file.close();

File file_out = SD.open("/test.txt",FILE_READ);

//Serial.println(String((const char *)&file_out));
//while (file_out.available()) { Serial.write(file_out.read()); }
char buf[20];
           size_t len = 0;
           int siz = file_out.size();
           len = std::min((int)(sizeof(buf) - 1), siz);
file_out.read((uint8_t *)buf, len);
Serial.println(String((const char *)buf));
file_out.close();
*/





int size_b = 5;
ptr_buff = ptr_buff_head;
for(byte i=0;i<size_b;i++){
*ptr_buff=i;ptr_buff++;
}
SD.remove("/file_sound.txt");
File file = SD.open("/file_sound.txt", FILE_WRITE);
file.write((uint8_t *)ptr_buff,size_b);
file.close();

File fileread = SD.open("/file_sound.txt", FILE_READ);
              while (fileread.available()) {
              Serial.println(fileread.read());
              }
              fileread.close();
              /*
              // сброс в начало
              // for(byte i=0;i<size_b;i++){ ptr_buff--;}

                            ptr_buff = ptr_buff_head;

                            for(byte i=0;i<size_b;i++){
                            Serial.printf("i=%d\n",*ptr_buff);
                            ptr_buff++;
                            }

                            ptr_buff = ptr_buff_head;

                            for(byte i=0;i<size_b;i++){
                                      Serial.printf("i=%d\n",*ptr_buff);
                                      ptr_buff++;
                                      }
                                      // сброс в начало
                                      ptr_buff = ptr_buff_head;
                                      // обнуление
                                      for(byte i=0;i<size_b;i++){
                                                 *ptr_buff=0;
                                                 ptr_buff++;
                                                 }
                                                 */
                                                 }

                                                 /*
                                                 uint16_t analogReadFast(uint8_t pin) {
                                                 pin = ((pin < 8) ? pin : pin - 14);    // analogRead(2) = analogRead(A2)
                                                 ADMUX = (DEFAULT<< 6) | pin;    // Set analog MUX & reference
                                                 bitSet(ADCSRA, ADSC);            // Start
                                                 while (ADCSRA & (1 << ADSC));        // Wait
                                                 return ADC;                // Return result
                                                 }*/

                                                 boolean load_data_sound(/*fs::FS &fs*/){
                                                 Serial.println("load_data_sound start record\n");
                                                 Serial.printf("ESP.getFreeHeap=%d\n",ESP.getFreeHeap());
                                                                                SD.remove("/file_sound.txt");
                                                                                myDelayMicroseconds(1000);

                                                                                byte count = 6;
                                                                                int bufferposition=0;
                                                                                File file = SD.open("/file_sound.txt", FILE_WRITE);

                                                                                //String str_chank = String("");
                                                                                unsigned long myTime= millis();

                                                                                //char cstr[1600]="";
                                                                                //char str_byte[4]="";

                                                 //unsigned long TimeIntervalSmal;

                                                 while(count>0){
                                                 count--;
                                                 //Serial.printf("Count %d\n",count);

                                                 ptr_buff=ptr_buff_head;
                                                 memset(ptr_buff, 0, size_buff);
                                                 /*for(int i=0;i<size_buff;i++){
                                                 *ptr_buff=0;
                                                 if(i<size_buff-1) ptr_buff++;
                                                 }
                                                 ptr_buff=ptr_buff_head;*/
                                                 bufferposition=0;

                                                 //Serial.printf("ptr_buff_head=%d,ptr_buff=%d,*ptr_buff=%d\n",ptr_buff_head,ptr_buff,*ptr_buff);
                                                 Serial.printf("count %d ESP.getFreeHeap=%d\n",count,ESP.getFreeHeap());
                                                 //Serial.printf("bufferposition=%d size_buff=%d\n",bufferposition,size_buff);
                                                 //TimeIntervalSmal = micros();
                                                 while( bufferposition < size_buff ){
                                                 //if (micros() - TimeIntervalSmal >= 100){//  без задержки 9882

                                                 // *ptr_buff=map(analogRead(MAX9814), 0, 1023, 0, 255);// 0,2500,0,255
                                                 *ptr_buff=(byte)(analogRead(MAX9814)/5);
                                                 if(bufferposition < size_buff){

                                                 ptr_buff++;
                                                 }

                                                 bufferposition++;

                                                 //delayMicroseconds(100);
                                                 //TimeIntervalSmal = micros();
                                                 // }
                                                 }
                                                 //Serial.printf("ptr loaded ptr_buff=%d ptr_buff_head=%d \n",ptr_buff,ptr_buff_head);
                                                 ptr_buff=ptr_buff_head;
                                                 //Serial.println("Переброс");
                                                 //***************************************
                                                 file.write((uint8_t*)ptr_buff,size_buff);
                                                 /*
                                                 //12695
                                                 for (int indx=0;indx<size_buff;indx++){
                                                 //Serial.println(indx);
                                                 //Serial.printf("log 1 strlen=%d\n",strlen(cstr));

                                                 //memset(str_byte,'\0', 3);
                                                 //itoa(*ptr_buff,str_byte,DEC);
                                                 //strcat(cstr,str_byte);
                                                 itoa((int)*ptr_buff,strchr(cstr,'\0'),DEC);

                                                 //Serial.println("log 2");
                                                 if(indx < size_buff){
                                                 strcat(cstr,",");
                                                 ptr_buff++;
                                                 }
                                                 //Serial.println("log 3");
                                                 if (indx%300==0){

                                                 if(!file.write((uint8_t*)cstr,strlen(cstr))){
                                                 Serial.println("Append failed 1");
                                                 }
                                                 memset(cstr,'\0', 1);
                                                 }
                                                 }

                                                 if(!file.write((uint8_t*)cstr,strlen(cstr))){
                                                 Serial.println("Append failed 2");
                                                 }
                                                 */
                                                 //***************************************
                                                 /*
                                                 str_chank="";
                                                 for (int i=0;i<size_buff;i++){
                                                 str_chank+=(*ptr_buff);
                                                 if(i < size_buff){
                                                 str_chank+=",";
                                                 ptr_buff++;
                                                 }
                                                 if (i%100==0){
                                                 if(!file.print(str_chank)){
Serial.println("Append failed 1");
}
str_chank="";
}
}
if(!file.print(str_chank)){
Serial.println("Append failed 2");
}
*/
//**********************************
//Serial.println("File loaded");
}

file.flush();
Serial.print("myTime=");
Serial.println(myTime);
Serial.printf("File size=%d\n",file.size());//160000
file.close();

return true;
}


void handleFileList() {
if (!server.hasArg("dir")) {
server.send(500, "text/plain", "BAD ARGS");
return;
}

String path = server.arg("dir");
String output = "[{\"name\":\"ggg\"}]";
server.send(200, "text/json", output);
}

void handleFileUpload() {
WiFiClient client;
HTTPClient http;
if( http.begin(client, "http://" SERVER_IP "/postplain/")){ //HTTP
http.addHeader("Content-Type", "application/json");
http.addHeader("Access-Control-Allow-Origin", "*");
int httpCode = http.POST("{\"hello\":\"world\"}");
if (httpCode > 0) {
// HTTP header has been send and Server response header has been handled
Serial.printf("[HTTP] POST... code: %d\n", httpCode);

// file found at server
if (httpCode == HTTP_CODE_OK) {
const String& payload = http.getString();
Serial.println("received payload:\n<<");
Serial.println(payload);
Serial.println(">>");
}
} else {
Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
}

http.end();
}else{
Serial.println("HTTPClient not begin!");
}
}


void MAX9814_frequency(){

// Определение частоты MHz
int Buffer[1];
int item = 0;
unsigned long  TimeInterval = millis();
int RecInterval = 1000;
while(millis() - TimeInterval < RecInterval ){
//delay(0.3);// 7092
// без задержки 9882
Buffer[0]= analogRead(MAX9814);
item++;
}
Serial.print("\nitem=");Serial.println( item );
}

void soundFX(float num,float period){
int pausa=num+num*sin(millis()/period);
for(int i=0;i<50;i++){
digitalWrite(MAX9814,HIGH);
myDelayMicroseconds(pausa);
digitalWrite(MAX9814,LOW);
myDelayMicroseconds(pausa);
}
}


void loop()
{

server.handleClient();
MDNS.update();
}