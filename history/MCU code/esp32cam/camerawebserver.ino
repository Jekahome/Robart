#include "esp_camera.h"
#include <WiFi.h>
#include "ArduinoJson.h"

const char* ssid = "Jeka";
const char* password = "********";

enum Command
{
succeess_picture = 1,
succeess_sound = 2,
send_picture = 3,

motor_user_control = 4,
motor_auto_control = 5,
say = 6,
error_message = 7,
invalid_command = 8,
get_sentence = 9,
succeess_sentence = 10,
set_sentence = 11,
msg_log = 12,
esp32cam_restart = 13
};

// key "motor_command,distance,speed"
enum MotorControl
{
forward_full_motor_uc = 1,// вперед
back_full_motor_uc = 2, // назад
left_full_motor_uc = 3, // разворот влево
right_full_motor_uc = 4, // разворот вправо
rotate_180_full_motor_uc = 5,// разворот на 180 градусов
motor_stop_uc = 6 // стоять
};

// Из файла app_httpd.cpp

struct type_msg{
const char *type;
const char *msg;
String arr_int;
String arr_float;
};

void setup_sd_mmc();
void startCameraServer();
void send_log_self(type_msg obj);
Command send_picture_server_self();


// Задаем статический IP-адрес:
IPAddress local_IP(192, 168, 0, 105);
// Задаем IP-адрес сетевого шлюза:
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // опционально
IPAddress secondaryDNS(8, 8, 4, 4); // опционально

byte counter = 0;// wifi connect

void myDelayMicroseconds_main(uint32_t us) {
uint32_t tmr = micros();
while (micros() - tmr < us);
}

void setup() {


Serial.begin(115200);
Serial.setDebugOutput(true);

camera_config_t config;
config.ledc_channel = LEDC_CHANNEL_0;
config.ledc_timer = LEDC_TIMER_0;
config.pin_d0 = 5;
config.pin_d1 = 18;
config.pin_d2 = 19;
config.pin_d3 = 21;
config.pin_d4 = 36;
config.pin_d5 = 39;
config.pin_d6 = 34;
config.pin_d7 = 35;
config.pin_xclk = 0;
config.pin_pclk = 22;
config.pin_vsync = 25;
config.pin_href = 23;
config.pin_sscb_sda = 26;
config.pin_sscb_scl = 27;
config.pin_pwdn = 32;
config.pin_reset = -1;
config.xclk_freq_hz = 20000000;
config.pixel_format = PIXFORMAT_JPEG;//PIXFORMAT_JPEG;
config.jpeg_quality =  10;
config.fb_count =  2;
config.frame_size = FRAMESIZE_SXGA;

// gpio_install_isr_service();
// camera init

esp_err_t err = esp_camera_init(&config);
if (err != ESP_OK) {
Serial.printf("Camera init failed with error 0x%x", err);
return;
}

// Файл sensor.h
// typedef enum {
//    PIXFORMAT_RGB565,    // 2BPP/RGB565
//    PIXFORMAT_YUV422,    // 2BPP/YUV422
//    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
//    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
//    PIXFORMAT_RGB888,    // 3BPP/RGB888
//    PIXFORMAT_RAW,       // RAW
//    PIXFORMAT_RGB444,    // 3BP2P/RGB444
//    PIXFORMAT_RGB555,    // 3BP2P/RGB555
//} pixformat_t;

// if PSRAM IC present, init with UXGA resolution and higher JPEG quality
//                      for larger pre-allocated frame buffer.
/*if(psramFound()){
config.frame_size = FRAMESIZE_UXGA;
config.jpeg_quality = 10;
config.fb_count = 2;
} else {
config.frame_size = FRAMESIZE_SVGA;
config.jpeg_quality = 12;
config.fb_count = 1;
}*/

//Serial.println("Доп. настройки камеры");
sensor_t * s = esp_camera_sensor_get();
/*
//s->set_framesize(s, FRAMESIZE_XGA);// разрешение
s->set_quality(s, 10);// четкость
s->set_brightness(s, 1);// яркость
s->set_contrast(s,  1);
s->set_saturation(s, 0);

//s->set_exposure_ctrl(s, 0);// ручная экспозиция
// s->set_aec_value(s, 864);// экспозиция
s->set_lenc(s, 0);
*/
/*
* Файл sensor.h

typedef enum {
FRAMESIZE_96x96,    // 96x96
FRAMESIZE_QQVGA,    // 160x120
FRAMESIZE_QQVGA2,   // 128x160
FRAMESIZE_QCIF,     // 176x144
FRAMESIZE_HQVGA,    // 240x176
FRAMESIZE_240x240,  // 240x240
FRAMESIZE_QVGA,     // 320x240
FRAMESIZE_CIF,      // 400x296
FRAMESIZE_VGA,      // 640x480
FRAMESIZE_SVGA,     // 800x600
FRAMESIZE_XGA,      // 1024x768
FRAMESIZE_SXGA,     // 1280x1024
FRAMESIZE_UXGA,     // 1600x1200
FRAMESIZE_QXGA,     // 2048*1536
FRAMESIZE_INVALID
} framesize_t;
*/

delay(1000);
// Настраиваем статический IP-адрес:
if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
Serial.println("STA Failed to configure !!!");
}
delay(1000);
// Подключаемся к WiFi-сети с помощью заданных выше SSID и пароля:
Serial.print("Connecting to ");  //  "Подключаемся к "
Serial.print(ssid);
delay(1000);
WiFi.begin(ssid, password);

while (WiFi.status() != WL_CONNECTED) {
delay(500);
Serial.print(".");
counter++;
if(counter>=60){ //30 seconds timeout - reset board
ESP.restart();
}
}
Serial.println("\nWiFi connected");

setup_sd_mmc();
startCameraServer();

Serial.print("Camera Ready! Use 'http://");
Serial.print(WiFi.localIP());
Serial.println("' to connect");
// ESP.restart();
}


// Прием команд от atmega328
void rx_tx_json_atmega328(){
if (Serial.available() > 0) {
Command output = Command::invalid_command;
DynamicJsonDocument doc(256);
// Пропуск Метка порядка байтов
// ASCII code 123 = '{'
while (Serial.peek() != 123 && Serial.available() > 0){
Serial.read();
}
DeserializationError err = deserializeJson(doc, Serial);
if ( err == DeserializationError::Ok ){
if ( doc["command"].isNull() == false ){
int command = doc["command"].as<int>();
switch ( command ) {
case Command::send_picture:
{
output = send_picture_server_self();
//Serial.printf("rx_tx_json_atmega328 Command::send_picture output %d\n",output);
//output = Command::succeess_picture;
break;
}
case Command::msg_log:
{
// лог от atmega328
struct type_msg msg;
msg.type=doc["type"].as<char*>();
msg.msg=doc["msg"].as<char*>();

if( doc["data_int"].isNull() == false){
msg.arr_int=doc["data_int"].as<String>();
}
if( doc["data_float"].isNull() == false){
msg.arr_float=doc["data_float"].as<String>();
}
send_log_self(msg);
}
default:
//Serial.printf("rx_tx_json_atmega328 default %d\n",command);
output = Command::error_message;
break;
}
}

}else{
// Ошибка передачи
char message[100] = "Error:\t";
strcat(message, err.c_str());
strcat(message, "\t");
switch ( err.code() ) {
case DeserializationError::IncompleteInput:{
strcat(message, "The end of the input is missing");
break;
}
case DeserializationError::InvalidInput:{
strcat(message, "The input is not recognized");
break;
}
case DeserializationError::NoMemory:{
strcat(message, "The JsonDocument is too small; you need to increase its capacity.\t");
strcat(message, "Capacity of the memory pool:");
strcat(message, (char*)doc.capacity());
break;
}
case DeserializationError::NotSupported:{
strcat(message, "The document included features not supported by the parser");
break;
}
case DeserializationError::TooDeep:{
strcat(message, "The nesting limit was reached");
break;
}
default:
strcat(message, "DeserializationError default");
break;
}

struct type_msg msg;
msg.type="esp32";
msg.msg=message;
send_log_self(msg);
}
while (Serial.available() > 0){
Serial.read();
}
Serial.flush();
DynamicJsonDocument doc_output(256);
doc_output["command"] = (int)output;
serializeJson(doc_output, Serial);
}
}


void loop() {

myDelayMicroseconds_main(500);
rx_tx_json_atmega328();
// Serial.printf("Free heap size:\t%d (bytes)\n", ESP.getFreeHeap());
}



/*

1. Воспроизведение аудио файла

command:
Воспроизведение аудио файла
curl -d '{"command":6,"mp3":[1,2,3],"delay_sound":2000}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command

2.Передача аудио сообщения на ATMega328

command:

curl -d '{"command":9}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
(сервер пошлет новый запрос на получения готового аудио)

3.Получить фото

handlers:
curl http://192.168.0.105/get-picture --output /home/jeka/Изображения/3.jpeg

command:
Отправка фото на rust сервер "http://192.168.0.106:4000/picture"
curl -d '{"command":3}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command


4.Получить звук от esp8266

command:
Запись и отправка звука на rust сервер "http://192.168.0.106:4000/sound"
curl -d '{"command":1}' -H "Content-Type: application/json" 192.168.0.104:80/command

5.Передача параметров управления мотором

command:
user control
curl -d '{"command":4,"motor_command":1,"distance":40,"speed":150}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
auto control
curl -d '{"command":5}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command

motor_command = 1,// вперед
motor_command = 2, // назад
motor_command = 3, // разворот влево
motor_command = 4, // разворот вправо
motor_command = 5,// разворот на 180 градусов
motor_command = 6  // стоп



*/
