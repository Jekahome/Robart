/*
PIN:  0    LOW: FAIL  HIGH: OK    PULL UP: OK       OK
PIN:  1    LOW: FAIL  HIGH: OK    PULL UP: FAIL     OK
PIN:  2    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN:  3    LOW: OK    HIGH: OK    PULL UP: FAIL     OK
PIN:  4    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN:  5    LOW: OK    HIGH: OK    PULL UP: FAIL     OK
PIN:  6    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN:  7    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN:  8    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN:  9    LOW: OK    HIGH: FAIL  PULL UP: FAIL     SHORT
PIN: 10    LOW: OK    HIGH: OK    PULL UP: FAIL     OK
PIN: 11    LOW: OK    HIGH: OK    PULL UP: FAIL     OK
PIN: 12    LOW: FAIL  HIGH: OK    PULL UP: OK       OK
PIN: 13    LOW: OK    HIGH: OK    PULL UP: FAIL     OK
PIN: 14    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN: 15    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN: 16    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN: 17    LOW: OK    HIGH: OK    PULL UP: OK       OK
PIN: 18    LOW: FAIL  HIGH: OK    PULL UP: OK       OK
PIN: 19    LOW: FAIL  HIGH: OK    PULL UP: OK       OK
*/
#include <NewPing.h>
#include <Servo.h> // подключаем библиотеку для сервопривода
#include <ArduinoJson.h>
//#include <MsTimer2.h>
#include <SoftwareSerial.h>
#include "DFRobotDFPlayerMini.h"

//#include <Fat16.h>
//#include <Fat16util.h>
//#define PIN_CHIP_SELECT 10
//SdCard card;

DFRobotDFPlayerMini myDFPlayer;
SoftwareSerial mySerial(A4, A5); // RX, TX
enum e_say_mp3{
process_say = 1,
stop_say = 2,
load_say = 3,
loaded_say = 4,
};
byte say_mp3 = e_say_mp3::stop_say;

#define PIN_SERVO 11
Servo servo;            // объявляем переменную servo типа "servo"
#define START_ANGLE_LEFT 120
#define START_ANGLE_RIGHT 0
#define START_ANGLE_CENTER 75

// HC–SR04
#define TRIGGER_PIN 10
#define ECHO_PIN 6
#define MAX_DISTANCE 200//см
#define MIN_DISTANCE 50//см
NewPing sonar(TRIGGER_PIN, TRIGGER_PIN, MAX_DISTANCE); // Настройка пинов и максимального расстояния.
unsigned int distance;
unsigned int distance_min;
int history_distance_min[3]= {0, 0, 0};
int current_index_history_distance_min=0;
                                       int stuck_delta = 3;

                                       // PWM 3, 5, 6, 9, 10, 11
                                       // моторы
                                       #define IN_1_2_PWM 3
                                       #define IN1 4
                                       #define IN2 7

                                       #define IN_3_4_PWM 5
                                       #define IN3 8
                                       #define IN4 13
                                       #define SPEED 150

                                       #define DISCTANCE_C 30

                                       /*
                                       enum Direction
                                       {
                                       forward = 1,
back, //2
left, //3
right  //4
};
Direction history_run[4];
*/
boolean congestion;// перегруженность

void forward_full_motor(unsigned int d,byte speed = SPEED);
void back_full_motor(byte speed = SPEED);
void left_full_motor(byte speed = SPEED);
void right_full_motor(byte speed = SPEED);
                                 void rotate_180_full_motor(byte speed = SPEED);
                                 void motor_user_control_fn(byte motor_command,unsigned int distance_c = DISCTANCE_C,byte speed_c = SPEED);

                                 void say_sound_arr(const int *const ptr=nullptr ,const int size=0);

                                                                         boolean motor_auto_control_var = true;

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

void myDelayMicroseconds(uint32_t us) {
uint32_t tmr = micros();
while (micros() - tmr < us);
}


void setup() {
Serial.begin(115200);

pinMode(IN_1_2_PWM, OUTPUT);
pinMode(IN_3_4_PWM, OUTPUT);
pinMode(IN1, OUTPUT);
pinMode(IN2, OUTPUT);
pinMode(IN3, OUTPUT);
pinMode(IN4, OUTPUT);

digitalWrite(IN1, LOW);
digitalWrite(IN2, LOW);
digitalWrite(IN3, LOW);
digitalWrite(IN4, LOW);

servo.attach(PIN_SERVO);
//Serial.println(servo.read()); // текущее положение

// Установка скорости двигателя A и B
analogWrite(IN_1_2_PWM, 0);
analogWrite(IN_3_4_PWM, 0);

distance_min=0;
congestion=false;

setup_DFPlayer();
motor_auto_control_var = false;

// Пытаемся проинициализировать модуль
/* if (!card.begin(PIN_CHIP_SELECT)) {
Serial.println("Card failed, or not present");
// Если что-то пошло не так, завершаем работу:
return;
}else{
if (!Fat16::init(&card)){
Serial.println(F("Fat16::init failed"));
return;
}else{
Serial.println(F("card initialized."));
}
} */

//MsTimer2::set(15000, say_sound_arr);
//MsTimer2::start();

myDelayMicroseconds(3000);
//forward_full_motor(100,100);
//delay(9000);
//test_degress();

}


// Общение двух плат по RX/TX
// byte led_bytes[]={49,48,49,48,49};
                    // RX первой платы соеденить с TX другой и TX первой с RX другой .
                    // При записи в порт Serial.write байты пойдут по TX каналу к RX другой платы

                    void send_log(String msg){
                    DynamicJsonDocument doc(256);
                    doc["command"] = (int)Command::msg_log;
                    doc["msg"]=msg;
                    doc["type"]="atmega328";

                    float arr_float[5] = {0.8, 1.188, 2.0, 3.2, 4.4};
                    String buff_float="";
for(int i=0;i<5;i++){
if(buff_float!=""){buff_float=buff_float+",";}
buff_float=buff_float+String(arr_float[i]);
}
doc["data_float"]=buff_float;

int arr_int[5] = {8, 1, 2, 3, 4};
String buff_int="";
for(int i=0;i<5;i++){
if(buff_int!=""){buff_int=buff_int+",";}
buff_int=buff_int+String(arr_int[i]);
}
doc["data_int"]=buff_int;
serializeJson(doc, Serial);
}

// Получение команд от esp32
void rx_tx_json_esp32(){
if (Serial.available() > 0) {
/* while (Serial.peek() != 123 && Serial.available() > 0){
Serial.read();
}
byte c;
while (Serial.available() > 0){
c=Serial.read();
Serial.print((char)c);Serial.print("=");Serial.println(c);
}
Serial.println("");
return;*/



// Пропуск Метка порядка байтов
while (Serial.peek() != 123 && Serial.available() > 0){
Serial.read();
//Serial.println((char)Serial.read());
}
DynamicJsonDocument doc(600);
//doc.clear();
DeserializationError err = deserializeJson(doc, Serial);
//Serial.println(F("\nСообщение от esp32-cam"));

if (err == DeserializationError::Ok)
{// https://arduinojson.org/v6/api/misc/deserializationerror/
if ( doc["command"].isNull() == true ){
//Serial.println(F("command not found"));

while (Serial.available() > 0){
Serial.read();
}
}else{
//Serial.print(F("command ="));
int command = doc["command"].as<byte>();
//Serial.print(command);Serial.print("\n");
switch ( command) {
case  Command::succeess_picture:
{
//Serial.println(F("Deserialization succeeded succeess_picture"));
break;
}
case  Command::motor_user_control:
{
//Serial.println("Deserialization succeeded user_control");
// {"command":5,"motor_command":1,"distance":40,"speed":150}
motor_auto_control_var = false;
byte motor_command = doc["motor_command"].as<byte>();
unsigned int distance_c = DISCTANCE_C;
byte speed_c = SPEED;
if ( doc["distance"].isNull() == false ){
distance_c = doc["distance"].as<int>();
}
if ( doc["speed"].isNull() == false ){
speed_c = doc["speed"].as<byte>();
}
motor_user_control_fn(motor_command,distance_c,speed_c);
break;
}
case  Command::motor_auto_control:
{
motor_auto_control_var = true;
break;
}
case  Command::error_message:
{
//Serial.println("Deserialization succeeded error_message");
break;
}
case Command::say:
{ // '{"command":7,"mp3":[1,2,3]}'
//Serial.print(F("rx_tx_json_esp32 Command::say \n"));
if( doc["mp3"].isNull() == false && say_mp3==e_say_mp3::stop_say){
say_mp3 = e_say_mp3::load_say;

JsonArray array = doc["mp3"].as<JsonArray>();
//Serial.print(F("JsonArray size=")); Serial.print(array.size() );  Serial.print(F("\n"));
int arr_mp3[array.size()];

int count_arr = 0;
//Serial.print(F("arr_mp3[count_arr]="));
for(JsonVariant v : array) {
arr_mp3[count_arr]=v.as<int>();
//Serial.print(v.as<int>());Serial.print(F(", "));
count_arr++;
}
//Serial.print(F("\n"));
say_mp3 = e_say_mp3::loaded_say;
say_sound_arr(arr_mp3 ,array.size());
}
break;
}
/*case Command::set_sentence:
{ // '{"command":12,"fs":4,"c":1,"lc":true,"d":"12"}'
if ( doc["fs"].isNull() == true || doc["c"].isNull() == true ||  doc["d"].isNull() == true ||  doc["lc"].isNull() == true){
Serial.println(F("Notfound key"));
}else{
// Fat16 example
Fat16 file;
int file_size = doc["fs"].as<int>();
byte chank = doc["c"].as<int>();
boolean last_chank = doc["lc"].as<boolean>();
if (chank==1){
if (file.open("s.wav", O_WRITE)) {
if (!file.remove()) Serial.println(F("error file.remove"));
if (!file.close()) Serial.println(F("error file.close"));
}
}

if (!file.open("s.wav", O_CREAT | O_APPEND | O_WRITE)) {
Serial.println(F("error file.open"));
}else{
file.write(doc["d"].as<char*>(), 400);
if (!file.close())  Serial.println(F("error file.close"));
if(last_chank==true){
Serial.println(F("File load"));
}
}
// SD example
int file_size = doc["file_size"].as<int>();
byte chank = doc["chank"].as<int>();
boolean last_chank = doc["last_chank"].as<boolean>();

if (chank==1){SD.remove("/sentence.wav");}

File file = SD.open("/sentence.wav", "a");
if (file) {
//file.write();
Serial.println(doc["data"].as<char*>());
} else {
Serial.println("error opening file");
}
if(last_chank==true){
Serial.println("File load");
}
}
doc.clear();
break;
}*/
default:
{
//Serial.println(F("Deserialization failed"));
break;
}
}
while (Serial.available() > 0){
Serial.read();
}
}

} else{
// Нужен лог вместо Serial
String msg ="Error: ";
msg = msg + String(err.c_str());
switch ( err.code() ) {
case DeserializationError::IncompleteInput:{
msg = msg + String("The end of the input is missing");
break;
}
case DeserializationError::InvalidInput:{
msg = msg + String("The input is not recognized");
break;
}
case DeserializationError::NoMemory:{
msg = msg + String("The JsonDocument is too small; you need to increase its capacity.");
msg = msg + String("Capacity of the memory pool:");
msg = msg + String(doc.capacity());
break;
}
case DeserializationError::NotSupported:{
msg = msg + String("The document included features not supported by the parser");
break;
}
case DeserializationError::TooDeep:{
msg = msg + String("The nesting limit was reached");
break;
}
default:
msg = msg + String("DeserializationError default");
break;
}

send_log(msg);
while (Serial.available() > 0){
Serial.read();
}
}
// Serial.flush();
}

// Отправка сообщения esp32-cam в тестовом режиме автоматически
//DynamicJsonDocument doc_output(256);
//doc_output["command"] = (int)Command::send_picture;
//serializeJson(doc_output, Serial);
}

void motor_user_control_fn(byte motor_command,unsigned int distance_c,byte speed_c){
// ручное управление

switch ( motor_command ) {
case MotorControl::forward_full_motor_uc:
{
forward_full_motor(distance_c,speed_c);
break;
}
case MotorControl::back_full_motor_uc:
{
back_full_motor(speed_c);
break;
}
case  MotorControl::left_full_motor_uc:
{
left_full_motor(speed_c);
break;
}
case  MotorControl::right_full_motor_uc:
{
right_full_motor(speed_c);
break;
}
case  MotorControl::rotate_180_full_motor_uc:
{
rotate_180_full_motor(speed_c);
break;
}
case MotorControl::motor_stop_uc:
{
stop();
break;
}
default:{
stop();
break;
}
}
}

void loop() {

//send_log("hgf");delay(5000);

rx_tx_json_esp32();// получение команд от esp32
myDelayMicroseconds(500);

if (motor_auto_control_var==true){
// автоматическое управление
stop();
distance = HC_SR04( START_ANGLE_CENTER );

if(!stuck(distance)){

if ( distance > MIN_DISTANCE ){
if( congestion == true) {
congestion=false;
// разворот на 180 градусов
rotate_180_full_motor();
}else{
//say_sound(3,3000);
forward_full_motor(distance);
}

//Serial.print("вперед ");
//Serial.println(distance);
}else{
//случаный выбор проверки поворота
if(random(2)){

distance = HC_SR04( START_ANGLE_LEFT );
myDelayMicroseconds(1000);
if ( distance > MIN_DISTANCE ){
//Serial.println("влево");
//say_sound(1,3000);
left_full_motor();
if( congestion == true) congestion=false;
}else{

distance = HC_SR04( START_ANGLE_RIGHT );
myDelayMicroseconds(1000);
if ( distance > MIN_DISTANCE ){
//Serial.println("вправо");
say_sound(2,3000);
right_full_motor();
if( congestion == true) congestion=false;
}else{
if( congestion == false) congestion=true;
//Serial.println("назад");
//say_sound(4,3000);
back_full_motor();

}
}
}else{

distance = HC_SR04( START_ANGLE_RIGHT );
myDelayMicroseconds(1000);
if ( distance > MIN_DISTANCE ){
//Serial.println("вправо");
//say_sound(2,3000);
right_full_motor();
if( congestion == true) congestion=false;
}else{

distance = HC_SR04( START_ANGLE_LEFT );
myDelayMicroseconds(1000);
if ( distance > MIN_DISTANCE ){
//Serial.println("влево");
//say_sound(1,3000);
left_full_motor();
if( congestion == true) congestion=false;
}else{
if( congestion == false) congestion=true;
//Serial.println("назад");
//say_sound(4,3000);
back_full_motor();
}
}
}
}
}else{
// выход из затора
back_full_motor(250);
rotate_180_full_motor(200);
}
stop();
myDelayMicroseconds(1000);
}
}

boolean stuck(int distance){
if(distance==0)return false;
if( current_index_history_distance_min  == 2 ){
int avg = (history_distance_min[0] + history_distance_min[1] + history_distance_min[2])/3 ;
if(    (avg >= history_distance_min[0] - stuck_delta &&  avg <= history_distance_min[0] + stuck_delta ) &&
(avg >= history_distance_min[1] - stuck_delta &&  avg <= history_distance_min[1] + stuck_delta ) &&
(avg >= history_distance_min[2] - stuck_delta &&  avg <= history_distance_min[2] + stuck_delta )
){
history_distance_min[0]=0;
history_distance_min[1]=0;
history_distance_min[2]=0;
current_index_history_distance_min=0;
return true;
}else{
history_distance_min[0]=0;
history_distance_min[1]=0;
history_distance_min[2]=0;
current_index_history_distance_min=0;
return false;
}
}else{
history_distance_min[current_index_history_distance_min]=distance;
current_index_history_distance_min++;
return false;
}
}

void forward_full_motor (unsigned int d,byte speed){
stop();
motor_back();
run(speed);
myDelayMicroseconds(d*6);// в зависимости от расстояния находим необходимое расстояния движения
stop();
}
void back_full_motor(byte speed){
//Serial.println("назад")
stop();
motor_forward();
run(speed);
myDelayMicroseconds(300);
stop();
}
void left_full_motor(byte speed){
//Serial.println("влево");
stop();
motor_left_forward();
run(speed);
//myDelayMicroseconds(550);//45
myDelayMicroseconds(400);
stop();
}
void right_full_motor(byte speed){
//Serial.println("вправо");
stop();
motor_right_forward();
run(speed);
//myDelayMicroseconds(550);//45
myDelayMicroseconds(400);
stop();
}
void rotate_180_full_motor(byte speed){
stop();
motor_left_forward();
run(speed);
myDelayMicroseconds(980);//180
stop();
}

int HC_SR04(int d){
//myDelayMicroseconds(50);
//distance = sonar.ping_cm();
//Serial.print(uS);
// с одним пином
// myDelayMicroseconds(50);
// return (sonar.ping() / US_ROUNDTRIP_CM);

distance_min=1;

servo_custom(d);
myDelayMicroseconds(100);
distance = (sonar.ping() / US_ROUNDTRIP_CM);
if(distance==0)distance=MAX_DISTANCE;
distance_min = distance;
//Serial.print(distance);
//Serial.print(" ");

servo_custom(d+15);
myDelayMicroseconds(100);
distance  = (sonar.ping() / US_ROUNDTRIP_CM);
if(distance==0)distance=MAX_DISTANCE;
distance_min = distance < distance_min? distance: distance_min;
//Serial.print(distance_min);
//Serial.print(" ");

myDelayMicroseconds(100);
servo_custom(d+30);
distance = (sonar.ping() / US_ROUNDTRIP_CM);
if(distance==0)distance=MAX_DISTANCE;
distance_min = distance < distance_min? distance: distance_min;
//Serial.print(distance_min);
//Serial.print(" ");

myDelayMicroseconds(100);
servo_custom(d+45);
distance  = (sonar.ping() / US_ROUNDTRIP_CM);
if(distance==0)distance=MAX_DISTANCE;
distance_min = distance < distance_min? distance: distance_min;
//Serial.print(distance_min);

//Serial.print(" min:");
//Serial.print(distance_min);
//Serial.println("-------------------");
return distance_min;
}
// Поворот на 270 градусов
// analogWrite(IN_1_2_PWM, 150);
// analogWrite(IN_3_4_PWM, 150);
// delay(1500);

void stop(){
analogWrite(IN_3_4_PWM, 0);
analogWrite(IN_1_2_PWM, 0);
myDelayMicroseconds(500);
}
void run(byte speed){
analogWrite(IN_1_2_PWM, speed);
analogWrite(IN_3_4_PWM, speed);
}
void motor_back(){
// Вращение двигателем A и B назад
digitalWrite(IN1, HIGH);
digitalWrite(IN2, LOW);
digitalWrite(IN3, HIGH);
digitalWrite(IN4, LOW);
}
void motor_forward(){
// Вращение двигателем A и B вперед
digitalWrite(IN1, LOW);
digitalWrite(IN2, HIGH);
digitalWrite(IN3, LOW);
digitalWrite(IN4, HIGH);
}
void motor_left_forward(){
// Вращение двигателем A и B вперед
digitalWrite(IN1, HIGH);
digitalWrite(IN2, LOW);
digitalWrite(IN3, LOW);
digitalWrite(IN4, HIGH);
}
void motor_right_forward(){
// Вращение двигателем A и B вперед
digitalWrite(IN1, LOW);
digitalWrite(IN2, HIGH);
digitalWrite(IN3, HIGH);
digitalWrite(IN4, LOW);
}
void servo_custom(int degree){
servo.write(degree);
myDelayMicroseconds(1500);
}
void servo_left(){
// устанавливаем сервопривод в крайнее левое положение
//Serial.println("левое положение  ");
servo.write(0);
}
void servo_right(){
// устанавливаем сервопривод в крайнее правое положение
//Serial.println("правое положение ");
servo.write(180);
}
void servo_straight(){
// устанавливаем сервопривод в среднее положение
//Serial.println("серединное положение");
servo.write(90);
}
//***********************************************************************************************

void setup_DFPlayer () {
mySerial.begin(9600);
if (!myDFPlayer.begin(mySerial,false)) {  //Use softwareSerial to communicate with mp3.
//Serial.println(F("Unable to begin:"));
//Serial.println(F("1.Please recheck the connection!"));
//Serial.println(F("2.Please insert the SD card!"));
String msg = "setup_DFPlayer Unable to begin:1.Please recheck the connection! 2.Please insert the SD card!";
send_log(msg);
while(true);
}
myDFPlayer.volume(30);
myDFPlayer.disableLoop();

}

void say_sound(int name_sound, float delay_sound){
//if(say_mp3==e_say_mp3::stop_say){
myDFPlayer.playMp3Folder(name_sound);
myDelayMicroseconds (delay_sound);
//}
}

void say_sound_arr(const int *const ptr=nullptr ,const int size=0) {

if(say_mp3==e_say_mp3::loaded_say){
say_mp3=e_say_mp3::process_say;

for(int i=0;i<size;i++){
//Serial.print(F("ptr="));  Serial.print(ptr[i]);Serial.print(F("\n"));
myDFPlayer.playMp3Folder(ptr[i]);
myDelayMicroseconds(2500);
}
myDelayMicroseconds(1400);
// Заполнить глобальный массив данными и задачу выполнит таймер прерывания
say_mp3=e_say_mp3::stop_say;
}
}

/*
void test_degress(){
// тест угла поворота
Serial.println("");
Serial.println("0   10  20  30  40  50  60  70  80  90  100 110 120 130 140 150 160 170 180");
Serial.println("---------------------------------------------------------------------------");
distance_min=0;
for(int d=0;d<=180;d+=10){
servo_custom(d);
distance_min  = (sonar.ping() / US_ROUNDTRIP_CM);
Serial.print(distance_min);
if(distance_min<10){Serial.print("   ");}
else if(distance_min<100){Serial.print("  ");}
else Serial.print(" ");
}Serial.println("");

distance_min=0;
for(int d=0;d<=180;d+=10){
servo_custom(d);
distance_min  = (sonar.ping() / US_ROUNDTRIP_CM);
Serial.print(distance_min);
if(distance_min<10){Serial.print("   ");}
else if(distance_min<100){Serial.print("  ");}
else Serial.print(" ");
}
Serial.println("");
Serial.println("---------------------------------------------------------------------------");
Serial.println("");
}
*/
/*void testing(){

String buff;
DynamicJsonDocument doc_output(256);
doc_output["command_to_atmega328"] = 2;//"hi";
serializeJson(doc_output, buff);

// for (int i = 0; i < buff.length(); i++)
// {
//  Serial.print(buff[i]);
//}
// Serial.println("");

DynamicJsonDocument doc(256);
DeserializationError err = deserializeJson(doc, buff);
//Serial.println(doc["command_to_atmega328"].as<char*>());
if (err == DeserializationError::Ok)
{
// Serial.println(doc["command_to_atmega328"].as<String>());
int command = doc["command_to_atmega328"].as<int>();
switch ( command) {
case  Command::hi:
Serial.print(F("Deserialization succeeded hi"));
break;
case  Command::bay:
Serial.print(F("Deserialization succeeded bay"));
break;
default:
Serial.print(F("Deserialization failed"));
break;
}
}else{
Serial.println(err.c_str());

}
Serial.println("");
}*/