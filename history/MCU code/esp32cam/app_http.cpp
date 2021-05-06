
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"
#include "ArduinoJson.h"
//   #include "fb_gfx.h"
#include "fd_forward.h"
//   #include "fr_forward.h"
//#define ENROLL_CONFIRM_TIMES 5
#include "HTTPClient.h"
#include "esp_http_client.h"
#include "UDHttp.h"
#include "camera_index.h"
#include "FS.h"
#include "SD_MMC.h"
/*#include <WiFi.h>
const char* send_host = "192.168.0.106";
const int send_port = 4000;
const char*  pskIdent = "Client_identity"; // PSK identity (sometimes called key hint)
const char*  psKey = "1a2b3c4d"; // PSK Key (must be hex string without 0x)
*/

void push_data_serial(DynamicJsonDocument * data_json);

File root;

typedef struct {
  String name;
  String path;
  int port;
} remote_server_uri_;


typedef struct {
  const char *type;
  const char *msg;
  String arr_int;
  String arr_float;
} type_msg;

/*
  // Used type_after_msg after_msg
  // Add log persistent
 if(!after_msg.is_send){
  after_msg.msg = after_msg.msg + doc["msg"].as<String>();
 }else{
  after_msg.msg=doc["msg"].as<String>();
 }
 Serial.print(after_msg.msg);
  // Send log after
   type_msg msg = {
        .type="esp32",
        .msg=after_msg.msg
    };
  send_log_self(msg);
*/
struct type_after_msg {
  String msg;
  boolean is_send;
}after_msg;


remote_server_uri_ server_uri_picture = {
  .name = "192.168.0.106",
  .path = "/picture",
  .port = 4000
};

remote_server_uri_ server_uri_log = {
  .name = "192.168.0.106",
  .path = "/log",
  .port = 4000
};

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

typedef struct {
        size_t size; //number of values used for filtering
        size_t index; //current value index
        size_t count; //value count
        int sum;
        int * values; //array to be filled with values
} ra_filter_t;

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

void myDelayMicroseconds(uint32_t us) {
  uint32_t tmr = micros();
  while (micros() - tmr < us);
}

static ra_filter_t ra_filter;
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
static mtmn_config_t mtmn_config = {0};
static int8_t is_enrolling = 0;


static ra_filter_t * ra_filter_init(ra_filter_t * filter, size_t sample_size){
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if(!filter->values){
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

static int ra_filter_run(ra_filter_t * filter, int value){
    if(!filter->values){
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }
    return filter->sum / filter->count;
}


static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}


void * new_buff(size_t size_){//http://www.cplusplus.com/doc/tutorial/pointers/
   auto  ptr=heap_caps_malloc(size_, MALLOC_CAP_8BIT);
   return ptr!=NULL?ptr:malloc(size_);
}

//**************************************************************************************
// Отправка звука

int wdataf(uint8_t *buffer, int len){
  // заменить на roor
  File file = SD_MMC.open("/new_sound.wav",FILE_WRITE);
  return file.write(buffer, len);
  //return wfile.write(buffer, len);
}
//read data callback
int rdataf(uint8_t *buffer, int len){
  //read file to upload
  if (root.available()) {
    return root.read(buffer, len);
  }
  return 0;
}
//  Внутренний запрос на сервер esp32cam для отправки звука на сервер rust
/*Command send_sound_server_self() {
   Command output = Command::error_message;
    Serial.println("send_sound_server_self");
    if(sound_busy==false){
      Serial.println("sound_busy false");
      sound_busy = true;
      load_data_sound(SD_MMC);
      UDHttp udh;
      root = SD_MMC.open("/file_sound.txt",FILE_READ);
      if (!root) {
       sound_busy = false;
       return output;
      }
       Serial.println("upload downloaded file to local server");
      //upload downloaded file to local server
      udh.upload(server_uri_sound, "f", root.size(), rdataf, NULL, NULL);
      root.close();
      sound_busy = false;
      output = Command::succeess_sound;
    }
    return output;
}*/


/*
// Внешний запрос на сервер esp32cam для отправки звука
// curl http://192.168.0.105/get-sound
static esp_err_t send_sound_server_handler(httpd_req_t *req) {
 // if (sound_busy==false){
 //      sound_busy = true;
 //      load_data_sound(SD_MMC);
 //
 //      UDHttp udh;
 //      //open file on sdcard to read
 //      root = SD_MMC.open("/file_sound.txt",FILE_READ);
 //      if (!root) {
 //       sound_busy = false;
 //       return ESP_ERR_INVALID_STATE;
 //      }
 //      //upload downloaded file to local server
 //      udh.upload(server_uri_sound, "f", root.size(), rdataf, NULL, NULL);
 //      root.close();
 //      sound_busy = false;
 //      return ESP_OK;
 //    }else{
 //      return ESP_ERR_INVALID_STATE;
 //    }

   //Serial.printf(" method='%d'  \n", req->method);
   //Serial.printf(" content_len='%d' \n", req->content_len);
   //Serial.printf("uri='%s'  \n",  req->uri);

   esp_err_t res = ESP_OK;
   if(sound_busy==false){
      sound_busy = true;
      load_data_sound(SD_MMC);
      root = SD_MMC.open("/file_sound.txt",FILE_READ);
      if (!root) {
      sound_busy = false;
      return ESP_FAIL;
      }

      httpd_resp_set_type(req, "text/plain");
      httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=f");
      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

      res = httpd_resp_send(req, (const char *)&root, root.size());
      root.close();
      sound_busy = false;
      return res;
    }else{
       return ESP_ERR_INVALID_STATE;
    }
}
*/

//**********************************************************************************************
// Отправка фото

// curl http://192.168.0.105/get-picture --output /home/jeka/Изображения/3.jpeg
static esp_err_t capture_handler(httpd_req_t *req){
   //Serial.printf(" method='%d'  \n", req->method);
   //Serial.printf(" content_len='%d' \n", req->content_len);
   //Serial.printf("uri='%s'  \n",  req->uri);
    camera_fb_t * fb = NULL;
    /*
    Файл esp_camera.h

    typedef struct {
        uint8_t * buf;              !< Pointer to the pixel data
        size_t len;                 !< Length of the buffer in bytes
        size_t width;               !< Width of the buffer in pixels
        size_t height;              !< Height of the buffer in pixels
        pixformat_t format;         !< Format of the pixel data
    } camera_fb_t;


    Файд http_parser.h

    #define HTTP_METHOD_MAP(XX)       \
    XX(0,  DELETE,      DELETE)       \
    XX(1,  GET,         GET)          \
    XX(2,  HEAD,        HEAD)         \
    XX(3,  POST,        POST)         \
    XX(4,  PUT,         PUT)          \
    pathological                      \
    XX(5,  CONNECT,     CONNECT)      \
    XX(6,  OPTIONS,     OPTIONS)      \
    XX(7,  TRACE,       TRACE)        \
   */

   /*
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        const char resp[] = "Camera capture failed";
        httpd_resp_send(req, resp, strlen(resp));
        //httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    bool detected = false;
    // fb->width > 400
    size_t fb_len = 0;
    fb_len = fb->len;
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    //Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
    return res;
    */
        esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    bool detected = false;
    int face_id = 0;
    if(fb->width > 400){
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }

    dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    if (!image_matrix) {
        esp_camera_fb_return(fb);
        Serial.println("dl_matrix3du_alloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    out_buf = image_matrix->item;
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;

    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if(!s){
        dl_matrix3du_free(image_matrix);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    box_array_t *net_boxes = face_detect(image_matrix, &mtmn_config);

    if (net_boxes){
        detected = true;


        free(net_boxes->score);
        free(net_boxes->box);
        free(net_boxes->landmark);
        free(net_boxes);
    }

    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    dl_matrix3du_free(image_matrix);
    if(!s){
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    Serial.printf("FACE: %uB %ums %s%d\n", (uint32_t)(jchunk.len), (uint32_t)((fr_end - fr_start)/1000), detected?"DETECTED ":"", face_id);
    return res;
}

Command send_picture_server_self() {// https://randomnerdtutorials.com/esp32-cam-post-image-photo-server/
  WiFiClient client;
  Command output = Command::error_message;
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    //Serial.println("Camera capture failed");
    myDelayMicroseconds(1000);
    ESP.restart();
  }

  if (client.connect(server_uri_picture.name.c_str(), server_uri_picture.port)) {
    //Serial.println("Connection successful!");
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"p.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";
    size_t imageLen = fb->len;
    size_t fbLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
    client.println("POST " + server_uri_picture.path + " HTTP/1.1");
    client.println("Host: " + server_uri_picture.name);
    client.println("Content-Length: " + String(totalLen));
    client.println("Access-Control-Allow-Origin: *");
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);
    uint8_t *fbBuf = fb->buf;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (/*fbLen%1024>0*/ fbLen&10000000000 >0 ) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }
    client.print(tail);
    esp_camera_fb_return(fb);

   unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            client.stop();
            return output;
        }
    }
    /*while(client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }*/
     output = Command::succeess_picture;
  }
  else {
   return output;
  }
  return output;
}
//********************************************************************************************************************

// Отправка лог сообщений
void send_log_self(type_msg obj){
    HTTPClient http;

  if(http.begin(server_uri_log.name.c_str(),server_uri_log.port,server_uri_log.path.c_str())){
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Access-Control-Allow-Origin", "*");
    StaticJsonDocument<512> doc;
    doc["msg"] = obj.msg;
    doc["type"] = obj.type;


    if(obj.arr_int && obj.arr_int.length()){
      doc["data_int"]=obj.arr_int;
    }
    if(obj.arr_float && obj.arr_float.length()){
      doc["data_float"]=obj.arr_float;
    }

    String requestBody;
    serializeJsonPretty(doc, requestBody);

    int httpResponseCode = http.POST(requestBody);
  //Serial.printf("6");
  /*if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.print(httpResponseCode);

    switch ( httpResponseCode ) {
        case HTTPC_ERROR_CONNECTION_REFUSED:{
            Serial.println("HTTPC_ERROR_CONNECTION_REFUSED");
            break;
        }
        case HTTPC_ERROR_SEND_HEADER_FAILED:{
            Serial.println("HTTPC_ERROR_SEND_HEADER_FAILED");
            break;
        }
        case HTTPC_ERROR_SEND_PAYLOAD_FAILED:{
            Serial.println("HTTPC_ERROR_SEND_PAYLOAD_FAILED");
            break;
        }
        case HTTPC_ERROR_NOT_CONNECTED:{
            Serial.println("HTTPC_ERROR_NOT_CONNECTED");
            break;
        }
        case HTTPC_ERROR_CONNECTION_LOST:{
            Serial.println("HTTPC_ERROR_CONNECTION_LOST");
            break;
        }
        case HTTPC_ERROR_NO_STREAM:{
            Serial.println("HTTPC_ERROR_NO_STREAM");
            break;
        }
        case HTTPC_ERROR_NO_HTTP_SERVER:{
            Serial.println("HTTPC_ERROR_NO_HTTP_SERVER");
            break;
        }
        case HTTPC_ERROR_TOO_LESS_RAM:{
            Serial.println("HTTPC_ERROR_TOO_LESS_RAM");
            break;
        }
        case HTTPC_ERROR_ENCODING:{
            Serial.println("HTTPC_ERROR_ENCODING");
            break;
        }
        case HTTPC_ERROR_STREAM_WRITE:{
            Serial.println("HTTPC_ERROR_STREAM_WRITE");
            break;
        }
        case HTTPC_ERROR_READ_TIMEOUT:{
            Serial.println("HTTPC_ERROR_READ_TIMEOUT");
            break;
        }
    }

    String payload = http.getString();
    Serial.println(payload);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }*/

    http.end();
  }
}

/*void send_log_get(const char *msg){
   int value_log = 0;
   const char* privateKey = "...";
   WiFiClient client;
    ++value_log;
    const char* host = "192.168.0.106";
    const int httpPort = 4000;
    if (!client.connect(host, httpPort)) {
        Serial.println("connection failed");
        return;
    }
    String url = "/log/";
    url += "?private_key=";
    url += privateKey;
    url += "&value=";
    url += value_log;

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return;
        }
    }
    while(client.available()) {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }

    Serial.println();
    Serial.println("closing connection");
}*/




//********************************************************************************************************************

// загружает файл с http://192.168.0.106:4000/sound/sentence.wav
Command get_sentence_self(){
    Command output = Command::error_message;


/*
     // для маленьких файлов
     UDHttp udh;

     wfile  = SD_MMC.open("/new_sound.wav", FILE_WRITE);

     if (!wfile) {
       Serial.println("can not open file!");
       return ESP_FAIL;
     }

     udh.download("http://192.168.0.106:4000/sound/sentence.wav", wdataf, NULL);
     wfile.flush();
     wfile.close();
*/

    WiFiClient client;

    if (!client.connect("192.168.0.106" /*server_uri_picture.name*/, 4000)) {
        Serial.println("Connection failed.");
        Serial.println("Waiting 5 seconds before retrying...");
        myDelayMicroseconds(5000);
        return output;
    }
    client.print("GET /sound/sentence.wav HTTP/1.1\n\n");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            Serial.println(">>> Client Timeout !");
            client.stop();
            return output;
        }
    }
    char *header_end = NULL;
    unsigned int total_content_length = 0;
    {
      unsigned int bytes;
      unsigned int received = 0;
      char buff_header[400];
      int buffer_size = 10000;
      // поиск content-length
      do {
           //Serial.print(".");
              if(client.available()){
                  bytes = client.read((uint8_t *)&buff_header[received], 400);
                  if(bytes != -1){
                      received += bytes;
                      header_end = strstr(buff_header, "\r\n\r\n");
                  }
              }
      } while (header_end == NULL);
      // Serial.print("\nЗаголовки:\n"); for (int i=0;i<400;i++){Serial.print(buff[i]);}

      char content_length[10];
      const char *start_content_length;
      start_content_length = strstr(buff_header, "content-length: ");
      const char *end_content_length = strstr(start_content_length, "\r\n");
      int full_length = strlen("content-length: ");
      memset(content_length, 0, 10);
      memcpy(content_length, start_content_length+full_length, end_content_length-start_content_length-full_length);
      total_content_length = atoi(content_length);
      start_content_length=nullptr;
      end_content_length=nullptr;
      // Serial.printf("content_length=%d\n",total_content_length);// 45586
    }

    if(total_content_length==0){
      return output;
    }
    // загрузка файла
    char * ptr = (char *) new_buff(total_content_length);
    if (ptr==NULL){
      Serial.printf("You need chank! content-length:%d\n",total_content_length);
      return output;
    }
    client.readBytes(ptr,total_content_length);
    SD_MMC.remove("/sentence.wav");
    File file = SD_MMC.open("/sentence.wav", FILE_WRITE);
    file.write((uint8_t*)ptr,total_content_length);
    file.flush();
    ptr=nullptr;
    //Serial.printf("File size=%d,total_content_length=%d",file.size(),total_content_length);

    if(file.size()==total_content_length){
      output=Command::succeess_sentence;
    }
    file.close();
    Serial.print("End\n");
    return output;
}

//********************************************************************************************************************
// Прием команды

/*typedef struct {
    String name;
    size_t number;
} json_data;*/

// curl --verbose -d '{"name":"Jeka","number":255}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command

#define min(a,b) ((a)<(b)?(a):(b))

static esp_err_t command_handler(httpd_req_t *req){
      char content[500];

      size_t recv_size = min(req->content_len, sizeof(content));

      int ret = httpd_req_recv(req, content, recv_size);
      if (ret <= 0) {
          if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
              httpd_resp_send_408(req);
          }
          return ESP_FAIL;
      }
      //Serial.print("content=");Serial.println(content);

      StaticJsonDocument<900> doc;
      DeserializationError res_des = deserializeJson(doc, content);// https://arduinojson.org/v6/api/json/deserializejson/
       switch ( res_des.code() ) {
        case DeserializationError::Ok:{
          break;
        }
        case DeserializationError::IncompleteInput:{
          const char resp[] = "The end of the input is missing";
          httpd_resp_send(req, resp, strlen(resp));
          return ESP_ERR_INVALID_SIZE;
          break;
        }
        case DeserializationError::InvalidInput:{
          const char resp[] = "The input is not recognized";
          httpd_resp_send(req, resp, strlen(resp));
          return ESP_ERR_INVALID_ARG;
          break;
        }
        case DeserializationError::NoMemory:{
          const char resp[] = "The JsonDocument is too small; you need to increase its capacity";
          httpd_resp_send(req, resp, strlen(resp));
          return ESP_ERR_NO_MEM;
          break;
        }
        case DeserializationError::NotSupported:{
          const char resp[] = "The document included features not supported by the parser";
          httpd_resp_send(req, resp, strlen(resp));
           return ESP_ERR_NOT_SUPPORTED;
           break;
        }
        case DeserializationError::TooDeep:{
          const char resp[] = "The nesting limit was reached";
          httpd_resp_send(req, resp, strlen(resp));
          return ESP_ERR_INVALID_ARG;
          break;
        }
        default:
          const char resp[] = "Invalid arguments";
          httpd_resp_send(req, resp, strlen(resp));
          return ESP_ERR_INVALID_ARG;
         break;
       }

      /*
        String name_ = doc["name"];
        String number_ = doc["number"];
        Serial.println(String("name=")+name_+", number="+number_);
        json_data jdata = {
          .name = doc["name"],
          .number = doc["number"]
        };
        Serial.println(String("name=")+jdata.name+", number="+String(jdata.number));
      */
      if ( doc["command"].isNull() == true ){
        const char resp[] = "Invalid arguments.Not found key 'command'";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_ERR_INVALID_ARG;
      }

         int command = doc["command"].as<byte>();
         switch ( command ) {
              case  Command::send_picture:
              // curl -d '{"command":3}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
                {
                   if ( Command::error_message == send_picture_server_self()){
                     const char resp[] = "Error send picture";
                     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                     httpd_resp_send(req, resp, strlen(resp));
                     return ESP_FAIL;
                   }else{
                     const char resp[] = "Ok";
                     httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                     httpd_resp_send(req, resp, strlen(resp));
                     return ESP_OK;
                   }
                break;
                }

              case  Command::motor_user_control:
              // curl -d '{"command":4,"motor_command":1,"distance":40,"speed":150}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
                {
                    if ( doc["motor_command"].isNull() == true ){
                      const char resp[] = "Invalid arguments.Not found key 'motor_command'";
                       httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                      httpd_resp_send(req, resp, strlen(resp));
                      return ESP_ERR_INVALID_ARG;
                    }
                    {
                      DynamicJsonDocument doc_output(100);
                      doc_output["command"] = (byte)Command::motor_user_control;
                      doc_output["motor_command"] = doc["motor_command"].as<int>();
                      if ( doc["distance"].isNull() == false ){
                           doc_output["distance"] = doc["distance"].as<int>();
                      }
                      if ( doc["speed"].isNull() == false ){
                           doc_output["speed"] = doc["speed"].as<int>();
                      }
                      serializeJson(doc_output, Serial);
                      const char resp[] = "Ok";
                      httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                      httpd_resp_send(req, resp, strlen(resp));
                      return ESP_OK;
                    }
                    break;
              }
              case  Command::motor_auto_control:
               // curl -d '{"command":5}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
                {
                  {
                    DynamicJsonDocument doc_output(50);
                    doc_output["command"] = (byte)Command::motor_auto_control;
                    serializeJson(doc_output, Serial);
                    const char resp[] = "Ok";
                    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                    httpd_resp_send(req, resp, strlen(resp));
                    return ESP_OK;
                  }
                  break;
                }
              case  Command::say:
              // curl -d '{"command":6,"mp3":[3156,1370,16],"delay_sound":2000}' -H "Content-Type: application/json" 192.168.0.105:80/command
               {

                  if ( doc["mp3"].isNull() == true || doc["delay_sound"].isNull() == true){
                    const char resp[] = "Invalid arguments.Not found key 'mp3' and 'delay_sound'";
                    httpd_resp_send(req, resp, strlen(resp));
                    return ESP_ERR_INVALID_ARG;
                  }
                  {
                    int delay_sound = doc["delay_sound"].as<int>();

                    DynamicJsonDocument doc_output(700);
                    doc_output["command"] = (byte)Command::say;
                    doc_output["mp3"] = doc["mp3"];
                    serializeJson(doc_output, Serial);

                    const char resp[] = "Ok";
                    httpd_resp_send(req, resp, strlen(resp));
                    return ESP_OK;
                  }
                  break;
                }
                case  Command::get_sentence:
                 // curl -d '{"command":9}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
                {
                  //digitalWrite(16, HIGH);
                   const char resp[] = "Awaiting development";
                   httpd_resp_send(req, resp, strlen(resp));
                   return ESP_OK;
                  if (Command::succeess_sentence == get_sentence_self()){
                    /*
                    // передать файл на atmega328
                    File file = SD_MMC.open("/sentence.wav", FILE_READ);
                    if(!file){
                        Serial.println("Failed to open file for reading");
                        return ESP_FAIL;
                    }

                    Serial.print("Read from file start: ");

                    int file_size = file.size();
                    int last_size = file_size;

                    byte chank = 0;
                    boolean last_chank = false;

                    size_t size_buff = 512;
                    uint8_t buf[size_buff];
                    uint8_t * ptr = nullptr;
                    while(file.available()){

                       ptr = new uint8_t[size_buff];
                       last_size=last_size-file.read(ptr,size_buff);
                       if(last_size<=0){last_chank=true;}
                       chank++;
                       DynamicJsonDocument doc_output(600);
                       doc_output["command"] = (byte)Command::set_sentence;
                       doc_output["fs"] = file_size;
                       doc_output["c"] = chank;
                       doc_output["lc"] = last_chank;
                       doc_output["d"] = ptr;
                       serializeJson(doc_output, Serial);
                       delete[] ptr;
                       delay(300);

                    }
                    file.close();
                    Serial.print("Read from file end: ");
                    */
                    return ESP_OK;
                  }else{
                    return ESP_FAIL;
                  }
                  break;
                }
                case Command::msg_log:
                {
                  // curl -d '{"command":12,"msg":"hello"}' -H "Content-Type: application/json" 192.168.0.105:80/command

                 // Add log persistent
                 /*if(!after_msg.is_send){
                  after_msg.msg = after_msg.msg + doc["msg"].as<String>();
                 }else{
                  after_msg.msg=doc["msg"].as<String>();
                 }
                 Serial.print(after_msg.msg);
                  // Send log after
                   type_msg msg = {
                        .type="esp32",
                        .msg=after_msg.msg
                    };
                  send_log_self(msg);*/


                 /*type_msg msg = {
                        .type="esp32",
                        .msg="Hello"
                    };
                  send_log_self(msg);*/


                   int arr_init[5] = {0, 1, 2, 3, 4};
                   String data_int = String("");
                   for(int i=0;i<5;i++){
                    data_int+=arr_init[i];
                    data_int+=",";
                   }

                   float arr_float[5] = {0.9, 1.1, 2.0, 3.2, 4.4};
                   String data_float = String("");
                   for(int i=0;i<5;i++){
                    data_float+=arr_float[i];
                    data_float+=",";
                   }

                    type_msg msg;
                    msg.type="esp32";
                    msg.msg="Hello";
                    msg.arr_int=data_int;
                    msg.arr_float=data_float;

                  send_log_self(msg);
                  break;
                }
                case Command::esp32cam_restart:{
                   // curl -d '{"command":13}' -H "Content-Type: application/json" 192.168.0.105:80/command
                  const char resp[] = "Ok";
                  httpd_resp_send(req, resp, strlen(resp));
                  ESP.restart();
                  return ESP_OK;
                  break;
                }
              default:
                  const char resp[] = "Invalid arguments.\nExample request:\n \
For send picture => curl -d '{\"command\":3}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
For motor user control => curl -d '{\"command\":4,\"motor_command\":1,\"distance\":40,\"speed\":150}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
For motor auto control => curl -d '{\"command\":5}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
For say => curl -d '{\"command\":6,\"mp3\":[1,2,3],\"delay_sound\":2000}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
For get sentence => curl -d '{\"command\":9}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
For esp32cam restart => curl -d '{\"command\":13}' -H \"Content-Type: application/json\" 192.168.0...:80/command\n \
";
              httpd_resp_send(req, resp, strlen(resp));
              return ESP_ERR_INVALID_ARG;
         }

      const char resp[] = "Ok";
      httpd_resp_send(req, resp, strlen(resp));
      return ESP_OK;
}
//********************************************************************************************************************

// Потоковое вещание
// <img id="stream" src="http://192.168.0.105:81/stream">
 #define PART_BOUNDARY "123456789000000000000987654321"
static esp_err_t stream_handler(httpd_req_t *req){
    const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
    const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
    const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    int64_t fr_start = 0;
    int64_t fr_ready = 0;
    int64_t fr_encode = 0;

    static int64_t last_frame = 0;
    size_t hlen = 0;

    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    while(true){
            fb = esp_camera_fb_get();
            if (!fb) {
                 const char resp[] = "Camera capture failed";
                 httpd_resp_send(req, resp, strlen(resp));
                 res = ESP_FAIL;
            } else {// fb->width > 400
                fr_start = esp_timer_get_time();
                fr_ready = fr_start;
                fr_encode = fr_start;
                 _jpg_buf_len = fb->len;
                 _jpg_buf = fb->buf;
            }
            if(res == ESP_OK){
                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            }
            if(res == ESP_OK){
                hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if(res == ESP_OK){
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
            if(fb){
                esp_camera_fb_return(fb);
                fb = NULL;
                _jpg_buf = NULL;
            } else if(_jpg_buf){
                free(_jpg_buf);
                _jpg_buf = NULL;
            }
            if(res != ESP_OK){
                break;
            }
            last_frame = esp_timer_get_time();
     }
    last_frame = 0;
    return res;
}


 // Установка настроек камеры
static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) {
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u,", s->status.colorbar);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req){

    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t * s = esp_camera_sensor_get();// Файл sensor.h
        //typedef struct {
        //  uint8_t MIDH;
        //  uint8_t MIDL;
        //  uint8_t PID;
        //  uint8_t VER;
        //} sensor_id_t;

    if (s->id.PID == OV3660_PID) {
        return httpd_resp_send(req, (const char *)index_ov3660_html_gz, index_ov3660_html_gz_len);
    }
    return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

void setup_sd_mmc(){
   if(!SD_MMC.begin( "/sdcard", true )){
        Serial.println("\nCard Mount Failed !!!\n");
        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return;
    }
    Serial.println("SD_MMC Ready!");
}

void startCameraServer(){

  /*
     #define HTTPD_DEFAULT_CONFIG() {                       \
            .task_priority      = tskIDLE_PRIORITY+5,       \
            .stack_size         = 4096,                     \
            .server_port        = 80,                       \
            .ctrl_port          = 32768,                    \
            .max_open_sockets   = 7,                        \
            .max_uri_handlers   = 8,                        \
            .max_resp_headers   = 8,                        \
            .backlog_conn       = 5,                        \
            .lru_purge_enable   = false,                    \
            .recv_wait_timeout  = 5,                        \
            .send_wait_timeout  = 5,                        \
            .global_user_ctx = NULL,                        \
            .global_user_ctx_free_fn = NULL,                \
            .global_transport_ctx = NULL,                   \
            .global_transport_ctx_free_fn = NULL,           \
            .open_fn = NULL,                                \
            .close_fn = NULL,                               \
            .uri_match_fn = NULL                            \
    }
   */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();// Файл esp_http_server.h
    config.stack_size = 8192;// Максимальный размер стека, разрешенный для задачи сервера
    config.server_port = 80;// Номер порта TCP для приема и передачи трафика HTTP
    config.max_uri_handlers = 10;// Максимально допустимые URI обработчики
    config.lru_purge_enable = true;// Очистить соединение «Наименее недавно использованное»

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t picture_uri = {
        .uri       = "/get-picture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };
    httpd_uri_t command_uri = {
        .uri       = "/command",
        .method    = HTTP_POST,
        .handler   = command_handler,
        .user_ctx  = NULL
    };
   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

   ra_filter_init(&ra_filter, 20);
   /* mtmn_config.type = FAST;
    mtmn_config.min_face = 80;
    mtmn_config.pyramid = 0.707;
    mtmn_config.pyramid_times = 4;
    mtmn_config.p_threshold.score = 0.6;
    mtmn_config.p_threshold.nms = 0.7;
    mtmn_config.p_threshold.candidate_number = 20;
    mtmn_config.r_threshold.score = 0.7;
    mtmn_config.r_threshold.nms = 0.7;
    mtmn_config.r_threshold.candidate_number = 10;
    mtmn_config.o_threshold.score = 0.7;
    mtmn_config.o_threshold.nms = 0.7;
    mtmn_config.o_threshold.candidate_number = 1;

    face_id_init(&id_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);*/

    // Маршруты
    Serial.printf("\nStarting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &picture_uri);
        httpd_register_uri_handler(camera_httpd, &command_uri);
    }
   // Маршрут потокового вещания
    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
//**************************************
// Function for stopping the webserver
void stop_webserver()
{
     // Ensure handle is non NULL
     if (&camera_httpd != NULL) {
         // Stop the httpd server
         httpd_stop(&camera_httpd);
     }
}




