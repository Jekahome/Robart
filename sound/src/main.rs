
#[macro_use]
extern crate serde_json;

#[macro_use]
extern crate handlebars;

extern crate failure;

use actix_web::{
    error, middleware, web, dev, guard,App, Error, HttpRequest, HttpResponse, HttpServer,
    http::header,http::Method, http::StatusCode,body::Body,Result
};
use actix_web::dev::ServiceResponse;
use actix_web::middleware::errhandlers::{ErrorHandlerResponse, ErrorHandlers};
use actix_files as fs;
use actix_http::Response;

use bytes::{Bytes, BytesMut};
use json::JsonValue;
use serde::{Deserialize, Serialize};
use actix_multipart::Multipart;
use std::io::Write;
use futures::{StreamExt, TryStreamExt};

use actix_http::ResponseBuilder;
use failure::Fail;
use chrono::prelude::*;

use std::cell::Cell;
use std::sync::Mutex;
use std::borrow::BorrowMut;

use std::fs::File;
use std::fs::OpenOptions;
use std::path::Path;
use std::io::Read;
mod parse_sound;
use parse_sound::create_wav;
use std::collections::HashMap;

use handlebars::{Handlebars, HelperDef, RenderContext, Helper, Context, JsonRender, HelperResult, Output, RenderError,ScopedJson};

static mut COUNTER: usize = 0;

handlebars_helper!(new_tr_helper: |{max_elem: u64=3}| {unsafe{
    if max_elem as usize==COUNTER {
        COUNTER=0;
        return Ok(Some(ScopedJson::Constant(&json!(false))));
    }
    COUNTER += 1;
    return Ok(Some(ScopedJson::Constant(&json!(true))));
}});


#[derive(Fail, Debug)]
enum MyError {
    #[fail(display = "internal error")]
    InternalError,
    #[fail(display = "bad request")]
    BadClientData,
    #[fail(display = "timeout")]
    Timeout,
}

impl error::ResponseError for MyError {
    fn error_response(&self) -> HttpResponse {
        ResponseBuilder::new(self.status_code())
            .set_header(header::CONTENT_TYPE, "text/html; charset=utf-8")
            .body(self.to_string())
    }

    fn status_code(&self) -> StatusCode {
        match *self {
            MyError::InternalError => StatusCode::INTERNAL_SERVER_ERROR,
            MyError::BadClientData => StatusCode::BAD_REQUEST,
            MyError::Timeout => StatusCode::GATEWAY_TIMEOUT,
        }
    }
}

/*
    method : POST
    url : http://127.0.0.1:8080/
    header : Content-Type = application/json
    body (raw) : {"name": "Test user", "number": 100}
*/
/*
#[derive(Debug, Serialize, Deserialize)]
struct MyObj {
    name: String,
    number: i32,
}


/// Внешнее управление
/// Получения изображения
async fn get_image(item: web::Json<MyObj>, req: HttpRequest) ->  Result<&'static str, MyError> {
    println!("request: {:?}", req);
    println!("model: {:?}", &item);

    std::thread::spawn(move||{
        let target = "http://192.168.0.105/get-picture";
        //println!("POST {}",target);
        //let tmp_dir = tempfile::Builder::new().prefix("tmp").tempdir().map_err(|e|MyError::BadClientData)?;
        let utc: DateTime<Utc> = Utc::now();
        let mut file = std::fs::File::create(format!("tmp/pic_{}.jpeg", utc.format("%Y-%m-%e-%H-%M-%S").to_string())).unwrap();
        let mut res = reqwest::blocking::get(target).map_err(|e|MyError::BadClientData).unwrap();

        //println!("Status: {}", res.status());//Status: 200 OK
        //println!("Headers:\n{:?}", res.headers());
        // {"content-type": "image/jpeg", "content-length": "86317", "content-disposition": "inline; filename=capture.jpg", "access-control-allow-origin": "*"}

        // copy the response body directly to stdout
        res.copy_to(&mut file).map_err(|e|MyError::BadClientData).unwrap();
        //Err(MyError::BadClientData)
    });

    Ok("Ok")
}*/



async fn get_years_picture( req: HttpRequest) -> Result<HttpResponse> {
    Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json( GeneralPhotoVideo::get_all_years()))
}
use serde::Deserializer;
#[derive(Default,Serialize,Deserialize)]
struct RequestYear {
    year: String,
}

const MAX_SIZE: usize = 262_144;
async fn get_months_picture(  mut payload: web::Payload,req: HttpRequest) -> Result<HttpResponse> {
    let mut body = web::BytesMut::new();
    while let Some(chunk) = payload.next().await {
        let chunk = chunk?;
        if (body.len() + chunk.len()) > MAX_SIZE {
            return Err(error::ErrorBadRequest("overflow"));
        }
        body.extend_from_slice(&chunk);
    }

   match serde_json::from_slice::<RequestYear>(&body){
       Ok(e)=>{
          return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json( GeneralPhotoVideo::get_months(e.year.clone())));
       },
       Err(e)=>{
           return  Err(actix_web::error::ErrorBadGateway(e));
       }
   }
}

#[derive(Default,Serialize,Deserialize)]
struct RequestYearMonth {
    year: String,
    month: String
}
async fn get_days_picture(  mut payload: web::Payload,req: HttpRequest) -> Result<HttpResponse> {
    let mut body = web::BytesMut::new();
    while let Some(chunk) = payload.next().await {
        let chunk = chunk?;
        if (body.len() + chunk.len()) > MAX_SIZE {
            return Err(error::ErrorBadRequest("overflow"));
        }
        body.extend_from_slice(&chunk);
    }

    match serde_json::from_slice::<RequestYearMonth>(&body){
        Ok(e)=>{
            return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json( GeneralPhotoVideo::get_days(e.year.clone(),e.month.clone()  )));
        },
        Err(e)=>{
            return  Err(actix_web::error::ErrorBadGateway(e));
        }
    }
}

#[derive(Default,Serialize,Deserialize)]
struct RequestYearMonthDay {
    year: String,
    month: String,
    day: String
}
async fn get_day_picture(  mut payload: web::Payload,req: HttpRequest) -> Result<HttpResponse> {
    let mut body = web::BytesMut::new();
    while let Some(chunk) = payload.next().await {
        let chunk = chunk?;
        if (body.len() + chunk.len()) > MAX_SIZE {
            return Err(error::ErrorBadRequest("overflow"));
        }
        body.extend_from_slice(&chunk);
    }

    match serde_json::from_slice::<RequestYearMonthDay>(&body){
        Ok(e)=>{
            return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json( GeneralPhotoVideo::get_day(e.year.clone(),e.month.clone(),e.day.clone()  )));
        },
        Err(e)=>{
            return  Err(actix_web::error::ErrorBadGateway(e));
        }
    }
}

#[derive(Default,Serialize,Deserialize)]
struct RequestYearMonthDayFile {
    year: String,
    month: String,
    day: String,
    file_name:String
}
async fn delete_picture(  mut payload: web::Payload,req: HttpRequest) -> Result<HttpResponse> {
    let mut body = web::BytesMut::new();
    while let Some(chunk) = payload.next().await {
        let chunk = chunk?;
        if (body.len() + chunk.len()) > MAX_SIZE {
            return Err(error::ErrorBadRequest("overflow"));
        }
        body.extend_from_slice(&chunk);
    }

    match serde_json::from_slice::<RequestYearMonthDayFile>(&body){
        Ok(e)=>{
            if let Some(_) = GeneralPhotoVideo::delete_picture(e.year.clone(),e.month.clone(),e.day.clone(),e.file_name.clone()){
                return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*")
                    .json( GeneralPhotoVideo::get_day(e.year.clone(),e.month.clone(),e.day.clone()  )));
            }else{
                return  Err(actix_web::error::ErrorBadGateway("Bad format file name"));
            }

        },
        Err(e)=>{
            return  Err(actix_web::error::ErrorBadGateway(e));
        }
    }
}

#[derive(Default,Serialize,Deserialize)]
struct RequestSoundFile {
    sound: String
}
async fn delete_sound(mut payload: web::Payload,req: HttpRequest)-> Result<HttpResponse> {
    let mut body = web::BytesMut::new();
    while let Some(chunk) = payload.next().await {
        let chunk = chunk?;
        if (body.len() + chunk.len()) > MAX_SIZE {
            return Err(error::ErrorBadRequest("overflow"));
        }
        body.extend_from_slice(&chunk);
    }

    match serde_json::from_slice::<RequestSoundFile>(&body){
        Ok(e)=>{
           if let Some(_) = GeneralSound::delete_sound(e.sound){
               let gs = GeneralSound::new();
               let gs_data:serde_json::Value = serde_json::to_value(gs).unwrap();
               let data = json!(gs_data);
               return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json(data));
           }else{
               return  Err(actix_web::error::ErrorBadGateway("Bad format file name"));
           }
        },
        Err(e)=>{
            return  Err(actix_web::error::ErrorBadGateway(e));
        }
    }
}

async fn get_sounds(req: HttpRequest)-> Result<HttpResponse> {
    let gs = GeneralSound::new();
    let gs_data:serde_json::Value = serde_json::to_value(gs).unwrap();
    let data = json!(gs_data);
    return Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json(data));
}

async fn get_last_picture( req: HttpRequest) -> Result<HttpResponse> {
    let utc: DateTime<Utc> = Utc::now();
    let year =  utc.format("%Y").to_string();
    let month = utc.format("%m").to_string();
    let day = utc.format("%d").to_string();
    let mut pictures = GeneralPhotoVideo::get_day(year.clone(),month.clone(),day.clone());
    pictures.sort();

    let data = json!({
          "year": year,
          "month": month,
          "day":day,
          "picture":pictures.pop()
  });
    Ok(HttpResponse::Ok().header("Access-Control-Allow-Origin","*").json( data))
}



#[derive(Debug, Serialize, Deserialize)]
struct Log{
    msg:String,
    r#type:String,
    data_int:Option<String>,
    data_float:Option<String>,
}

// curl --header "Content-Type: application/json"  --request POST --data '{"key":"xyz","value":"xyz"}' http://192.168.0.106:4000/test_post
async fn log( msg: web::Json<Log>,req: HttpRequest) ->  Result<&'static str, MyError> {
    println!("request: {:?}", req);
    println!("model: {:?}", &msg);

    let utc: DateTime<Utc> = Utc::now();

    let mut file = OpenOptions::new()
        .append(true)
        .read(false)
        .write(true)
        .create(true)
        .open("./log/log.txt")
        .unwrap();

    let mut buff = format!("{}", format_args!("{} msg:{} type:{} ", utc.format("%Y-%m-%d-%H-%M-%S").to_string(),msg.msg,msg.r#type));
    let mut buffer:&[u8] = buff.as_bytes();
    file.write(buffer);

    if let Some(data_int) = &msg.data_int {
        buff = format!("{}", format_args!("data_int:{} ",data_int));
        buffer = buff.as_bytes();
        file.write(buffer);
    }
    if let Some(data_float) = &msg.data_float {
        buff = format!("{}", format_args!("data_float:{} ", data_float));
        buffer = buff.as_bytes();
        file.write(buffer);
    }
    file.write("\n".as_bytes());

    Ok("Ok")
}

#[derive(Debug, Serialize, Deserialize)]
struct SoundChank{
    command:u8,
    chank:u32,
    sound:Vec<u8>,
    all_chanks:u32
}
// curl --header "Content-Type: application/json"  --request POST --data '{"command":4,"chank":2,"all_chanks":111,"sound":[1,2,3,4]}' http://192.168.0.106:4000/load_bytes
// принять сразу json
async fn load_bytes(body:String /*body: Bytes*//* body:web::Json<SoundChank>*/,counter: web::Data<Mutex<Vec<u8>>>,) ->  Result<&'static str, MyError> {

    //println!("{:?}",body);
    let res:SoundChank =  serde_json::from_str( body.as_str()).unwrap();
    //println!("{:?}",res);
    let mut duff = counter.lock().unwrap();

    if duff.len() > 0 && res.chank==0{
        *duff=vec![];
    }

    duff.extend(res.sound);

    //println!("{:?}",duff);


    if res.chank>=res.all_chanks  {
        // последний кусок
        let utc: DateTime<Utc> = Utc::now();
        let mut out_file =  std::fs::File::create(format!("tmp/sound_{}.wav", utc.format("%Y-%m-%e-%H-%M-%S").to_string())).unwrap();

        let audio_format = 1;// 1 (uncompressed) 2 (compressed)
        let channel_count = 1;// 1 (mono) or 2 (stereo)
        let sampling_rate = 20695;// Частота дискретизации (например, 16 кГц, 44,1 кГц, 48 кГц, 96 кГц и т. Д.)
        let bits_per_sample = 8;// Количество битов в каждой (подканальной) выборке. Обычно 8, 16 или 24
        let header:wav::Header = wav::Header::new(audio_format, channel_count, sampling_rate, bits_per_sample);

        {
            //Чистить шум
            let mut prev:u8=duff[0];
            for item in duff.iter_mut().skip(1){
                if *item<prev && prev-*item > 80{
                    *item=127-*item+127;
                }
                prev=*item;
            }
        }

        wav::write(header, wav::BitDepth::Eight(duff.clone()), &mut out_file);
        *duff=vec![];
    }
    /*
    let mut data:Vec<u8> = Vec::new();
    for item in body.iter().skip(44){
        //print!("{n:>w$}",n=item,w=4);
        data.push(*item);
    }
    let utc: DateTime<Utc> = Utc::now();
    let mut out_file =  std::fs::File::create(format!("tmp/sound_{}.wav", utc.format("%Y-%m-%e-%H-%M-%S").to_string())).unwrap();

    let audio_format = 1;// 1 (uncompressed) 2 (compressed)
    let channel_count = 1;// 1 (mono) or 2 (stereo)
    let sampling_rate = 16000;// Частота дискретизации (например, 16 кГц, 44,1 кГц, 48 кГц, 96 кГц и т. Д.)
    let bits_per_sample = 8;// Количество битов в каждой (подканальной) выборке. Обычно 8, 16 или 24
    let header:wav::Header = wav::Header::new(audio_format, channel_count, sampling_rate, bits_per_sample);

    wav::write(header, wav::BitDepth::Eight(data), &mut out_file);
*/
    print!("\n");
    Ok("Ok")
}

/*
// curl http://192.168.0.106:4000/send_sound
async fn send_sound(req: HttpRequest) ->  Result<&'static str, MyError> {
    println!("request: {:?}", req);
    use std::io::Read;
    // curl -F 'file=@/home/jeka/projects/project_rust/image/examples/imageproc/server/sound/new.wav' -H  'Content-Type: multipart/form-data' -H 'Content-Disposition: attachment; filename="new.wav"'  http://192.168.0.105/new_sound
    std::thread::spawn(move||{
        let target = "http://192.168.0.105/get-sentence";
        //let target = "http://192.168.0.106:4000/post_sound";

        let mut file = std::fs::File::open( "sound/new.wav" ).unwrap();
        //let d = vec![82,73,70,70,10,178,0,0,87,65,86,69,102,109,116,32,18,0,0,0,1,0,1,0,17,43,0,0,34,86,0,0,2,0,16,0,0,0,100,97,116,97,228,177,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,123,255,68,0,99,255,84,254,103,255,166,255,242,255,6,1,67,0,207,255,93,255,85,255,46,0,52,0,204,255,2,255,9,0,99,0,137,255,165,255,98,0,43,0,95,255,89,0,3,1,147,0,82,0,98,0,215,255,82,0,14,1,89,0,247,255,209,255,66,0,30,0,109,0,16,0,127,255,86,255,17,255,40,255,67,255,162,255,78,0,80,0,151,0,21,0,65,255,170,255,43,255,159,254,209,255,161,0,190,255,32,0,54,254,170,253,114,2,159,0,221,253,103,253,55,252,54,255,59,4,175,7,227,7,164,6,231,6,133,1,144,254,26,255,235,1,45,5,139,255,187,249,62,245,226,244,201,246,247,246,29,252,85,253,228,251,32,253,141,253,39,1,48,3,167,2,60,4,65,5,24,6,184,2,214,3,35,5,197,6,29,4,39,0,70,3,152,255,127,252,15,252,151,252,154,255,64,255,161,0,123,253,200,255,145,3,23,3,168,6,206,4,190,3,209,2,52,2,169,2,250,1,82,4,175,1,204,255,211,254,156,252,210,250,84,249,192,248,134,247,39,249,225,253,30,253,109,252,58,255,152,1,225,4,113,4,242,3,244,2,122,255,116,255,247,253,121,0,40,0,186,252,15,252,220,249,206,248,135,249,110,255,244,1,242,1,223,4,57,1,249,254,52,0,64,3,166,5,220,6,222,8,240,4,163,0,124,253,87,253,123,1,94,1,151,255,79,252,175,248,84,248,211,248,79,251,144,253,177,254,61,255,7,254,165,253,248,254,105,0,13,2,70,2,136,1,41,1,192,0,190,0,115,255,30,255,128,0,28,0,202,254,136,254,43,255,84,254,75,253,167,254,130,255,95,255,50,0,210,255,74,255,111,0,70,1,136,1,129,1,23,2,89,2,62,2,164,2,169,2,214,2,29,3,231,2,232,2,250,2,5,3,31,3,225,2,147,2,228,2,165,2,56,2,200,3,221,4,169,3,13,4,253,3,37,4,16,7,99,5,226,3,180,4,152,5,104,1,127,250,46,249,166,248,25,248,81,248,178,247,67,248,111,248,203,248,107,249,204,250,187,253,4,255,213,255,185,0,31,1,225,1,22,2,130,2,187,2,134,2,84,2,105,1,229,0,144,0,30,0,238,255,129,255,82,255,17,255,213,254,229,254,222,254,35,255,85,255,115,255,171,255,191,255,246,255,23,0,45,0,138,255,81,254,184,253,49,253,7,253,11,252,71,252,184,252,101,251,245,249,18,249,229,251,66,254,32,254,220,253,44,253,146,254,193,253,124,253,148,0,40,0,32,0,92,255,161,254,248,0,140,255,82,255,100,0,53,1,27,1,114,255,168,255,188,1,189,2,223,1,109,2,93,4,136,4,48,4,92,4,24,5,19,6,18,6,121,6,55,7,111,7,63,8,35,7,113,7,194,8,50,9,117,9,7,10,76,11,207,9,10,10,51,10,33,10,186,7,48,7,150,3,6,251,18,246,101,243,111,242,83,242,102,241,78,241,10,241,206,241,78,243,60,245,168,248,14,251,161,252,17,254,60,255,212,0,244,1,228,2,131,3,122,3,89,3,195,2,77,2,14,2,152,1,41,1,134,0,250,255,146,255,48,255,17,255,247,254,245,254,253,254,245,254,18,255,53,255,105,255,161,255,42,255,101,254,234,253,217,252,127,253,136,253,128,251,5,251,127,250,220,251,251,251,70,251,144,253,226,251,183,251,82,251,85,253,82,0,26,254,104,254,154,0,35,255,148,252,123,255,128,1,107,255,122,1,36,2,118,0,39,0,38,1,35,2,15,2,38,3,216,3,239,3,182,3,92,3,237,4,215,5,133,5,126,5,38,6,99,6,141,6,169,8,21,10,144,7,209,4,250,6,186,9,175,10,181,7,164,10,105,16,253,7,17,5,62,12,3,7,82,254,42,250,226,248,209,247,103,244,1,243,219,242,254,242,144,242,80,242,55,245,113,247,57,248,148,249,75,251,86,253,137,254,146,255,227,0,176,1,33,2,36,2,97,2,166,2,96,2,10,2,176,1,97,1,3,1,141,0,72,0,6,0,195,255,134,255,90,255,85,255,76,255,72,255,87,255,190,254,12,254,13,254,245,253,169,252,191,252,28,254,236,251,197,249,20,251,77,253,58,253,241,249,250,250,4,254,237,252,212,252,248,254,254,252,224,252,107,255,105,253,124,253,172,255,252,0,128,1,179,1,229,0,95,0,106,3,84,2,217,2,191,4,46,2,63,2,147,3,89,4,232,4,64,4,110,4,219,4,116,5,208,5,40,7,193,7,202,6,192,5,248,8,247,10,99,7,65,8,29,8,175,10,115,13,139,6,154,5,252,16,136,13,78,250,238,248,40,255,193,249,58,245,190,244,238,243,141,241,68,240,40,243,93,245,63,244,143,244,81,248,41,250,52,250,203,252,31,255,166,255,113,0,214,1,0,3,26,3,17,3,138,3,155,3,254,2,154,2,152,2,2,2,61,1,238,0,148,0,16,0,160,255,104,255,62,255,252,254,212,254,153,253,111,253,38,254,221,251,217,252,203,251,81,251,218,249,163,251,155,251,201,249,199,248,45,252,74,252,117,251,156,252,62,251,249,250,78,253,103,1,245,252,147,251,123,255,64,3,251,2,161,255,23,0,121,1,17,4,66,6,211,3,238,2,157,5,209,5,40,5,71,6,28,6,37,5,60,6,21,7,231,5,46,7,144,7,60,5,142,8,61,9,131,8,17,10,142,5,165,4,17,10,190,17,44,4,166,5,114,23,38,9,6,252,131,3,88,1,53,249,187,248,173,247,174,242,137,240,194,240,154,242,149,240,249,237,134,243,222,244,165,242,37,247,55,250,226,249,92,252,155,255,149,0,166,1,76,3,175,4,126,5,14,5,221,5,138,6,19,5,231,4,27,5,177,3,191,2,117,2,126,1,113,0,218,255,26,255,79,253,137,252,61,252,187,251,14,250,31,250,68,250,198,247,103,250,210,248,31,248,181,251,97,247,226,248,180,254,202,249,225,247,165,0,99,253,135,250,174,3,118,254,17,255,254,5,132,255,17,3,253,5,250,3,151,6,249,2,100,4,225,7,224,4,25,4,104,7,148,5,230,3,210,7,166,5,182,4,66,8,108,5,192,7,83,7,18,4,158,7,143,7,74,10,219,8,209,8,203,11,234,7,104,9,103,14,106,4,210,255,219,3,109,255,162,250,217,248,81,246,88,244,181,242,117,241,39,241,189,239,154,239,192,242,88,242,10,242,37,246,157,247,155,248,173,251,149,253,103,255,141,1,76,3,246,4,2,6,180,6,15,8,94,8,174,7,50,8,219,7,146,6,45,6,49,5,211,3,27,3,168,0,102,254,215,253,56,252,131,249,9,248,39,246,240,245,163,244,37,243,203,242,229,241,193,243,134,243,234,241,205,243,247,245,203,248,148,247,89,249,118,253,125,252,145,0,199,2,243,0,204,5,122,7,244,6,21,10,135,10,222,10,60,12,203,12,10,13,66,13,68,13,85,13,214,13,145,12,254,11,221,12,174,12,173,11,188,11,108,11,157,11,198,10,65,12,13,12,144,9,163,15,147,12,123,11,154,17,237,252,188,250,47,9,195,251,16,242,119,242,151,240,106,239,28,236,69,232,202,231,10,233,91,232,183,235,224,234,94,232,188,241,105,244,37,242,138,247,37,251,60,254,196,1,117,3,243,5,231,8,204,10,118,12,200,13,181,12,0,14,168,15,202,12,240,11,204,11,181,10,83,8,150,4,146,2,53,255,28,251,131,248,92,245,42,242,196,238,192,236,246,236,28,235,60,231,230,232,238,236,26,233,206,231,80,237,170,242,70,241,54,239,233,249,145,254,232,251,118,1,40,4,25,8,111,12,103,14,62,16,104,16,156,21,9,25,226,22,55,22,199,24,36,28,134,25,59,24,93,24,58,23,225,26,182,24,66,20,109,19,109,23,224,23,117,18,1,22,174,17,98,20,190,26,9,1,196,253,133,7,43,250,13,243,177,235,37,230,177,230,69,226,76,220,18,217,111,217,147,217,110,221,69,220,221,216,231,226,82,232,104,232,93,237,127,241,146,248,184,254,94,1,84,5,46,10,190,14,80,18,189,20,154,20,123,22,232,25,81,24,126,22,122,23,0,22,62,18,166,13,49,8,4,2,105,254,164,249,255,241,230,234,19,233,24,231,60,224,223,220,73,219,117,218,129,223,4,223,56,217,143,222,131,231,180,235,136,237,111,240,4,247,154,255,157,5,12,8,51,11,161,17,212,24,48,30,139,29,2,30,51,37,241,40,253,39,11,39,137,39,82,42,12,41,69,39,162,40,86,38,156,36,125,37,137,35,107,34,43,32,231,30,17,36,209,32,29,8,77,251,124,6,128,254,143,231,145,220,128,217,150,216,196,209,205,199,25,196,127,196,71,199,33,203,88,203,223,201,181,209,15,223,87,228,54,230,254,236,72,248,255,2,239,7,65,11,142,17,201,24,99,30,223,32,169,32,199,32,79,35,113,36,215,32,185,30,171,28,123,24,196,17,13,9,197,255,167,247,165,241,222,234,69,227,154,220,218,215,101,214,93,213,141,211,64,212,233,215,173,218,47,222,68,229,188,235,52,241,70,246,235,252,237,7,66,13,29,15,27,22,7];

        let mut buffer:std::vec::Vec<u8> = Vec::new();
        file.read_to_end(&mut buffer).unwrap();

        let mut i =0;
        for  item in buffer.iter().skip(0)/*.take(2000)*/ {
           //if i==1024 {println!("\n");i=0;}
            i=i+1;
           // print!("{},",item);
       }

        // let body = reqwest::blocking::Body::new(d[..]);
        let client = reqwest::blocking::Client::new();

        let res = client.post(target)
            .body("buffer")
            .send()
            .map_err(|e|{println!("{:?}",e);MyError::BadClientData}).unwrap();

    });

    Ok("Ok")
}*/

async fn post_sound(mut payload: Multipart,req: HttpRequest) ->  Result<&'static str, MyError> {

    while let Ok(Some(mut field)) = payload.try_next().await {
        println!("post_sound while");
        while let Some(chunk) = field.next().await {
            if chunk.is_ok(){
                let data = chunk.unwrap();
                println!("{:?}",data);
            }
        }
    }
    println!("post_sound exit");
    Ok("Ok")

}

async fn sound(body:String ) ->  Result<&'static str, MyError> {
    println!("SOUND");
    println!("{}",body);
    println!("END");
    Ok("Ok")
}


//use utils::upload::{save_file as upload_save_file, split_payload, UploadFile};
//mod utils;
async fn save_picture(mut payload: Multipart,req: HttpRequest) -> Result<HttpResponse, Error> {
    // iterate over multipart stream

    // let pl = split_payload(payload.borrow_mut()).await;
    // println!("bytes={:#?}", pl.0);
    println!("---------------");
    let map = req.headers();
    for (key, value) in map.iter() {
        println!("{:?}: {:?}", key, value);
    }
    println!("---------------");

    while let Ok(Some(mut field)) = payload.try_next().await {
        println!("save_file_test while");
        //let content_type = field.content_disposition().unwrap();
        //let filename = content_type.get_filename().unwrap();

        let filepath = GeneralPhotoVideo::get_new_filename();

        // File::create is blocking operation, use threadpool
        let mut f = web::block(|| std::fs::File::create(filepath))
            .await
            .unwrap();
        let mut chank_count:i32=0;
        // Field in turn is stream of *Bytes* object
        while let Some(chunk) = field.next().await {
            //println!("chunk={:?}",chunk.is_err());
            if chunk.is_ok(){
                let data  = chunk.unwrap();
                chank_count=chank_count+data.len() as i32;
                println!(" chank_count={}",chank_count);
                // filesystem operations are blocking, we have to use threadpool
                f = web::block(move || f.write_all(&data).map(|_| f)).await?;
            }else{
                println!("chunk no ok {:?}",chunk);
                return Ok(HttpResponse::Ok().into());

            }
        }
    }
    println!("save_file_test exit");
    Ok(HttpResponse::Ok().into())
}


// curl -F 'file=@/home/jeka/projects/atmega328_esp32/sound/sound/2021-05-6-10-54-05.txt' -H  'Content-Type: multipart/form-data' -H 'Content-Length: 4096' -H 'Content-Disposition: attachment; filename="2021-05-6-10-54-05.txt"'  http://192.168.0.106:4000/sound
async fn save_file(mut payload: Multipart,req: HttpRequest) -> Result<HttpResponse, Error> {

    println!("---------------");
    let map = req.headers();
    for (key, value) in map.iter() {
        println!("{:?}: {:?}", key, value);
    }
    println!("---------------");
    // iterate over multipart stream

    // let pl = split_payload(payload.borrow_mut()).await;
    // println!("bytes={:#?}", pl.0);

    let (filepath,file_out) = GeneralSound::get_new_filename();
    let file_in = filepath.clone();
    if let Ok(Some(mut field)) = payload.try_next().await {
        println!("save_file_test while");
        //let content_type = field.content_disposition().unwrap();
        //let filename = content_type.get_filename().unwrap();


        // let filepath = format!("./sound/{}", sanitize_filename::sanitize(&filename));
        // File::create is blocking operation, use threadpool
        let mut f = web::block( || std::fs::File::create(filepath))
            .await
            .unwrap();
        let mut chank_count:i32=0;
        // Field in turn is stream of *Bytes* object
        while let Some(chunk) = field.next().await {

            if chunk.is_ok(){
                let data = chunk.unwrap();
                chank_count=chank_count+data.len() as i32;
                println!(" chank_count={}",chank_count);
                // filesystem operations are blocking, we have to use threadpool
                f = web::block(move || f.write_all(&data).map(|_| f)).await?;
            }else{
                println!("chunk no ok {:?}",chunk);
                return Ok(HttpResponse::Ok().into());

            }

        }
        create_wav(file_in,file_out);
    }

    println!("save_file_test exit");
    Ok(HttpResponse::Ok().into())
}


/*fn index() -> HttpResponse {
    let html = r#"<html>
        <head><title>Upload Test</title></head>
        <body>
            <form action="/sound" target="/ttt" method="post" enctype="multipart/form-data">
                <input type="file" multiple name="file"/>
                <input type="submit" value="Submit"></button>
            </form>
        </body>
    </html>"#;

    HttpResponse::Ok().body(html)
}*/

#[derive(Serialize, Deserialize,Default,Debug)]
struct GeneralSound{
    sounds:Vec<String>
}

impl GeneralSound{
    fn get_new_filename()->(String,String){
        let utc: DateTime<Utc> = Utc::now();
        (format!("./sound/{}.txt", utc.format("%Y-%m-%d-%H-%M-%S").to_string()).to_string(),
         format!("./wav/{}.wav", utc.format("%Y-%m-%d-%H-%M-%S").to_string()).to_string())
    }

    fn delete_sound(sound:String)->Option<()>{
        use regex::Regex;
        if Regex::new(r"^\d{4}-\d{1,2}-\d{1,2}-\d{1,2}-\d{1,2}-\d{1,2}\.wav$").unwrap().is_match(&sound) {
            std::fs::remove_file(format!("./wav/{}",sound));
            return Some(());
        }else{
            return None;
        }

        return None;
    }

    fn new()->Self{
        let mut result = vec![];
        if let Ok(entries) = std::fs::read_dir(std::path::Path::new("./wav")) {
            for entry in entries {
                if let Ok(entry) = entry {
                   result.push(entry.file_name().into_string().unwrap())
                }
            }
        }
        result.sort_by(|a, b| b.cmp(a));
        GeneralSound{sounds: result}
    }

}

#[derive(Serialize, Deserialize,Default,Debug)]
struct GeneralPhotoVideo{
    years:HashMap<String, Years>
}

#[derive(Serialize, Deserialize,Default,Debug)]
struct Days{
    day:String,
    files:Vec<String>
}
#[derive(Serialize, Deserialize,Default,Debug)]
struct Months{
    month:String,
    days:HashMap<String, Days>
}
#[derive(Serialize, Deserialize,Default,Debug)]
struct Years{
    year:String,
    months:HashMap<String, Months>
}

impl GeneralPhotoVideo{
    fn get_new_filename()->String{
        let utc: DateTime<Utc> = Utc::now();
        let year =  utc.format("%Y").to_string();
        let month = utc.format("%m").to_string();
        let day = utc.format("%d").to_string();

        std::fs::create_dir_all(format!("./picture/{}/{}/{}",year.clone(),month.clone(),day.clone() )).unwrap();

        let filepath = format!("./picture/{}/{}/{}/{}.jpg",year.clone(),month.clone(),day.clone(),utc.format("%H-%M-%S").to_string());
        let file_out = String::from(&filepath);
        file_out
    }

    fn delete_picture(year:String,month:String,day:String,file_name:String)->Option<()>{
        use regex::Regex;
        if let Ok(_) = year.parse::<i32>(){
            if let Ok(_) = month.parse::<i32>(){
                if let Ok(_) = day.parse::<i32>(){
                    if Regex::new(r"^\d{1,2}-\d{1,2}-\d{1,2}\.jpg$").unwrap().is_match(&file_name) {
                        std::fs::remove_file(format!("./picture/{}/{}/{}/{}",year,month,day,file_name));
                        return Some(());
                    }else{
                        return None;
                    }
                }
            }
        }
        return None;
    }

    fn get_all_years()->Vec<String>{
        let mut result = vec![];
        if let Ok(entries) = std::fs::read_dir(std::path::Path::new("./picture")) {
            for entry in entries {
                if let Ok(entry) = entry {
                    if entry.path().is_dir(){
                        result.push(entry.file_name().into_string().unwrap())
                    }
                }
            }
        }
        result
    }

    fn get_months(year:String)->Vec<String>{
       let mut result = vec![];
        if let Ok(_) = year.parse::<i32>(){
            if let Ok(entries) = std::fs::read_dir(std::path::Path::new("./picture")) {
                for entry in entries {
                    if let Ok(entry) = entry {
                        if entry.path().is_dir(){
                            if entry.file_name().into_string().unwrap() == year {
                                if let Ok(entries_months) = std::fs::read_dir(entry.path()) {
                                    for entry_months in entries_months {
                                        if let Ok(entry_months) = entry_months {
                                            if entry_months.path().is_dir(){
                                                result.push(entry_months.file_name().into_string().unwrap());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

       result
    }

    fn get_days(year:String,month:String)->Vec<String>{
        let mut result = vec![];
        if let Ok(_) = year.parse::<i32>() {
            if let Ok(_) = month.parse::<i32>() {
                if let Ok(entries) = std::fs::read_dir(std::path::Path::new("./picture")) {
                    for entry in entries {
                        if let Ok(entry) = entry {
                            if entry.path().is_dir(){
                                if entry.file_name().into_string().unwrap() == year {
                                    if let Ok(entries_months) = std::fs::read_dir(entry.path()) {
                                        for entry_months in entries_months {
                                            if let Ok(entry_months) = entry_months {
                                                if entry_months.path().is_dir(){
                                                    if entry_months.file_name().into_string().unwrap() == month {
                                                        if let Ok(entries_days) = std::fs::read_dir(entry_months.path()) {
                                                            for entry_day in entries_days {
                                                                if let Ok(entry_day) = entry_day {
                                                                    if entry_day.path().is_dir(){
                                                                        result.push(entry_day.file_name().into_string().unwrap());
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }



        result
    }

    fn get_day(year:String,month:String,day:String)->Vec<String>{
        let mut result = vec![];
        if let Ok(_) = year.parse::<i32>(){
            if let Ok(_) = month.parse::<i32>(){
                if let Ok(_) = day.parse::<i32>(){
                    if let Ok(entries) = std::fs::read_dir(std::path::Path::new("./picture")) {
                        for entry in entries {
                            if let Ok(entry) = entry {
                                if entry.path().is_dir(){
                                    if entry.file_name().into_string().unwrap() == year {
                                        if let Ok(entries_months) = std::fs::read_dir(entry.path()) {
                                            for entry_months in entries_months {
                                                if let Ok(entry_months) = entry_months {
                                                    if entry_months.path().is_dir(){
                                                        if entry_months.file_name().into_string().unwrap() == month {
                                                            if let Ok(entries_days) = std::fs::read_dir(entry_months.path()) {
                                                                for entry_day in entries_days {
                                                                    if let Ok(entry_day) = entry_day {
                                                                        if entry_day.path().is_dir(){
                                                                            if entry_day.file_name().into_string().unwrap() == day {
                                                                                if let Ok(entries_pic) = std::fs::read_dir(entry_day.path()) {
                                                                                    for entry_pic in entries_pic {
                                                                                        if let Ok(entry_pic) = entry_pic {
                                                                                            if entry_pic.path().is_file(){
                                                                                                result.push(entry_pic.file_name().into_string().unwrap());
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }}}


        result
    }

    fn new()->Self{
        let mut files_path:Vec<String> = vec![];
        GeneralPhotoVideo::dir_files(std::path::Path::new("./picture"),&mut files_path);
        let mut dirs: Vec<String> = vec![];
        let mut year:String;
        let mut month:String;
        let mut day:String;
        let mut file_name:String;
        let mut result: HashMap<String, Years> = HashMap::new();

        for f in files_path{
            // skip "." and "picture"
            dirs = f.split("/").skip(2).map(|e|String::from(e)).collect();
            if dirs.len()!=4{continue;}
            year = dirs[0].clone();
            month = dirs[1].clone();
            day = dirs[2].clone();
            file_name = dirs[3].clone();
            if !result.contains_key(&year){
                let mut d = HashMap::new();
                d.insert(day.clone(),Days{ day:day.clone(), files:vec![file_name.clone()]});
                let mut m = HashMap::new();
                m.insert(month.clone(),Months{month:month.clone(),days:d});
                result.insert(year.clone(),Years{year:year.clone(), months:m});

            }else{
                for (key_y,y) in result.iter_mut() {
                    if *key_y==year{
                        if !y.months.contains_key(&month){
                            let mut d = HashMap::new();
                            d.insert(day.clone(),Days{ day:day.clone(), files:vec![file_name.clone()]});
                            y.months.insert(month.clone(),Months{month:month.clone(),days:d});
                        }else{
                            for (key_m,m) in y.months.iter_mut() {
                                if *key_m==month{
                                    if !m.days.contains_key(&day){
                                        m.days.insert(day.clone(),Days{ day:day.clone(), files:vec![file_name.clone()]});
                                    }else{
                                        for (key_d,d) in m.days.iter_mut() {
                                            if *key_d==day{
                                                d.files.push(file_name.clone());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        GeneralPhotoVideo{years:result}
    }

    fn dir_files(dir: &std::path::Path,result:&mut Vec<String>)-> std::io::Result<()>{
        if let Ok(entries) = std::fs::read_dir(dir) {
            for entry in entries {
                if let Ok(entry) = entry {
                    if entry.path().is_dir(){
                        //println!("DIR {:?}",entry.path());
                        GeneralPhotoVideo::dir_files(&entry.path(),result);
                    }else{
                        //println!("FILE {:?}",entry.path());
                        result.push(entry.path().as_path().display().to_string());
                    }
                }
            }
            Ok(())
        }else{
            Err(std::io::Error::new(std::io::ErrorKind::Other, "oh no!"))
        }
    }
}

async fn index(hb: web::Data<Handlebars<'_>>) -> HttpResponse {
    // подготовка данных для страницы
    // 1.список файлов wav
    // 2.список фото и папок с сортировкой по дате (обьект)

    // Фичи:
    // 1.словарь слов и поиск по ним
    // 2. передача параметров управления
   unsafe{COUNTER=0;}

    let gs = GeneralSound::new();
    let gs_data:serde_json::Value = serde_json::to_value(gs).unwrap();

    let gpv = GeneralPhotoVideo::new();
    //let data:String = serde_json::to_string(&gp).unwrap();
    let gpv_data:serde_json::Value = serde_json::to_value(gpv).unwrap();
    let data = json!({
        "years":gpv_data,
        "sounds": gs_data
    });
    /*let data2 = json!({
        "ttt": data
    });*/
    let body = hb.render("index", &data).unwrap();

    HttpResponse::Ok().body(body)
}

/*
fn rec(dir: &std::path::Path,mut year:String,mut month:String,mut day:String,result:&mut Vec<Years>)-> std::io::Result<()>{
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries {
            if let Ok(entry) = entry {

                if entry.path().is_dir(){
                    println!("DIR {:?}",entry.path());
                    let temp:String = entry.file_name().into_string().unwrap();

                    if(year.len() == 0 || year!=temp){
                        year = temp.clone();
                        result.push(Years{year:year.clone(),months:vec![]});
                    }else if month.len() == 0 {
                        month = entry.file_name().into_string().unwrap();
                        for y in result.iter_mut() {
                            if *y.year == year {
                                y.months.push(Months{month: month.clone(),days:vec![]});
                                break;
                            }
                        }
                    }else if day.len() == 0 {
                        day = entry.file_name().into_string().unwrap();
                        for  y in result.iter_mut() {
                            if *y.year == year {
                                for m in y.months.iter_mut() {
                                    if *m.month == month {
                                        m.days.push(Days{day:day.clone(),files:vec![]});
                                        break;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    rec(&entry.path(),year.clone(),month.clone(),day.clone(),result);
                }else{
                    println!("FILE {:?}",entry.path());
                    for  y in result.iter_mut() {
                        if *y.year == year {
                            for m in y.months.iter_mut() {
                                if *m.month == month {
                                    for  d in m.days.iter_mut() {
                                        if *d.day == day {
                                            d.files.push(entry.file_name().into_string().unwrap());
                                        }
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        Ok(())
    }else{
        Err(std::io::Error::new(std::io::ErrorKind::Other, "oh no!"))
    }
}
*/



// lsof -n -i4TCP:4000 | grep LISTEN | tr -s ' ' | cut -f 2 -d ' ' | xargs kill -9

// ngrok http 4000 --authtoken 1okjXfwKGugN4HzH0fsrBFRpynN_7E3F3A4SnMUe**** -host-header=rewrite
#[actix_rt::main]
async fn main() -> std::io::Result<()> {
     //create_wav("sound/2021-04-20-17-26-08.txt");
    //test("sound/2021-04-20-17-26-08.txt");


/*
    let sparkle_heart:&std::vec::Vec<i16> = &std::fs::read("sound/2021-04-24-21-04-18.txt").unwrap().iter().map(|e| *e as i16).collect();
for v in sparkle_heart.iter().take(3) {
println!("{}",v);
}


    return Ok(());*/
    //let mut result:Vec<Years> = vec![];
    //rec(std::path::Path::new("./picture"),"".to_string(),"".to_string(),"".to_string(),&mut result);


/*
    let gp = GeneralPhotoVideo::new();
    //let string_point:String = serde_json::to_string(&gp).unwrap();
    let data:serde_json::Value = serde_json::to_value(gp).unwrap();

    let data2 = json!({
        "ttt": data
    });
     println!("{}",data2);
    return Ok(());*/

    std::env::set_var("RUST_LOG", "actix_web=info");
    env_logger::init();
    std::fs::create_dir_all("./sound").unwrap();
    std::fs::create_dir_all("./log").unwrap();
    std::fs::create_dir_all("./picture").unwrap();
    let data = web::Data::new(Mutex::new(Vec::<u8>::new()));

    let mut handlebars = Handlebars::new();
    handlebars.register_helper("new_tr_helper", Box::new(new_tr_helper));
    handlebars
        .register_templates_directory(".html", "./static/templates")
        .unwrap();
    let handlebars_ref = web::Data::new(handlebars);

    HttpServer::new(move || {
        App::new()
            .wrap(
                middleware::DefaultHeaders::new()
                    .header("Access-Control-Allow-Origin", "*"))
            .wrap(error_handlers())
            .app_data( data.clone())
            .app_data( handlebars_ref.clone())
            // enable logger
            .wrap(middleware::Logger::new("---------------------------\n"))
            .wrap(middleware::Logger::default())
            .wrap(middleware::Logger::new("Content-Disposition %{Content-Disposition}i"))
            .wrap(middleware::Logger::new("Content-Type %{Content-Type}i"))
            .wrap(middleware::Logger::new("Content-Length %{Content-Length}i"))
            .data(web::JsonConfig::default().limit(4096)) // <- limit size of the payload (global configuration)

            /*.service(
                web::resource("/get-image")
                    //.data(web::JsonConfig::default().limit(1024)) // <- limit size of the payload (resource level)
                    .route(web::post().to(get_image)),
            )*/

            /*.service(
                web::resource("/sound")
                    .route(
                        //web::post().to(save_file_test)
                        web::route()
                            .guard(guard::Get())
                            .guard(guard::fn_guard(|head| head.method == Method::POST))
                            .to(save_file_test)
                    )
            )*/
            .service(
                web::resource("/")
                    .route(web::get().to(index))
            )
            .service(web::resource("/sound")
                .route(web::post().to(save_file))
            )
            .service(web::resource("/picture")
                .route(web::post().to(save_picture))
            )
            .service(web::resource("/log")
                .route(web::post().to(log))
            )
            .service(web::resource("/get_years_picture")
                .route(web::post().to(get_years_picture))
            )
            .service(web::resource("/get_months_picture")
                .route(web::post().to(get_months_picture))
            )
            .service(web::resource("/get_days_picture")
                .route(web::post().to(get_days_picture))
            )
            .service(web::resource("/get_day_picture")
                .route(web::post().to(get_day_picture))
            )
            .service(web::resource("/get_last_picture")
                .route(web::post().to(get_last_picture))
            )
            .service(web::resource("/delete_picture")
                .route(web::post().to(delete_picture))
            )
            .service(web::resource("/delete_sound")
                .route(web::post().to(delete_sound))
            )
            .service(web::resource("/get_sounds")
                .route(web::post().to(get_sounds))
            )


            .service(web::resource("/post_sound")
                .route(web::post().to(post_sound))
            )
            .service( fs::Files::new("/sound", "./sound"))

            .service(
                fs::Files::new("/public", "./public")
                    .show_files_listing()
                    .use_last_modified(true),
            )
            .service(
                 fs::Files::new("/picture", "./picture")
                    .show_files_listing()
                    .use_last_modified(true),
            )
            .service(
                fs::Files::new("/wav", "./wav")
                    .show_files_listing()
                    .use_last_modified(true),
            )

    })
        .bind("0.0.0.0:4000")?
        .run()
        .await
}


// Custom error handlers, to return HTML responses when an error occurs.
fn error_handlers() -> ErrorHandlers<Body> {
    ErrorHandlers::new().handler(StatusCode::NOT_FOUND, not_found)
}

// Error handler for a 404 Page not found error.
fn not_found<B>(res: ServiceResponse<B>) -> Result<ErrorHandlerResponse<B>> {
    let response = get_error_response(&res, "Page not found");
    Ok(ErrorHandlerResponse::Response(
        res.into_response(response.into_body()),
    ))
}

// Generic error handler.
fn get_error_response<B>(res: &ServiceResponse<B>, error: &str) -> Response<Body> {
    let request = res.request();

    // Provide a fallback to a simple plain text response in case an error occurs during the
    // rendering of the error page.
    let fallback = |e: &str| {
        Response::build(res.status())
            .content_type("text/plain")
            .body(e.to_string())
    };

    let hb = request
        .app_data::<web::Data<Handlebars>>()
        .map(|t| t.get_ref());
    match hb {
        Some(hb) => {
            let data = json!({
                "error": error,
                "status_code": res.status().as_str()
            });
            let body = hb.render("error", &data);

            match body {
                Ok(body) => Response::build(res.status())
                    .content_type("text/html")
                    .body(body),
                Err(_) => fallback(error),
            }
        }
        None => fallback(error),
    }
}

fn test<P: AsRef<Path> + std::fmt::Debug>(path_in:P) {

    let s:String = std::fs::read_to_string(path_in).unwrap();
    let v: Vec<u8> = s.split(",").filter(|e|{if let Ok(res) = e.parse::<u8>(){return true;}return false;}).map(|e|{e.parse::<u8>().unwrap()}).collect();// преобразуем в вектор


    for i in v.iter().take(10) {
        println!("{}", i);//
    }


    /*let mut data= vec![];
    {
        let f = File::open(path_in).unwrap();
        for b in f.iter().take(10)  {
            data.push(b.unwrap());
        }
    }*/


}

#[cfg(test)]
mod tests {
    use super::*;
    use actix_web::dev::Service;
    use actix_web::{http, test, web, App};

    #[actix_rt::test]
    async fn test_index() -> Result<(), Error> {
        let mut app = test::init_service(
            App::new().service(web::resource("/").route(web::post().to(index))),
        )
            .await;

        let req = test::TestRequest::post()
            .uri("/")
            .set_json(&MyObj {
                name: "my-name".to_owned(),
                number: 43,
            })
            .to_request();
        let resp = app.call(req).await.unwrap();

        assert_eq!(resp.status(), http::StatusCode::OK);

        let response_body = match resp.response().body().as_ref() {
            Some(actix_web::body::Body::Bytes(bytes)) => bytes,
            _ => panic!("Response error"),
        };

        assert_eq!(response_body, r##"{"name":"my-name","number":43}"##);

        Ok(())
    }
}