



1. Воспроизведение аудио файла  
```
    command:
      Воспроизведение аудио файла
      curl -d '{"command":6,"mp3":[3156,1370,16],"delay_sound":2000}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
```
2.Передача аудио сообщения на ATMega328 (ОЖИДАЕТ)
```
    command:
     
     curl -d '{"command":9}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
       (сервер пошлет новый запрос на получения готового аудио)
```
3.Получить фото 
```
    handlers:
      curl http://192.168.0.105/get-picture --output /home/jeka/Изображения/3.jpeg

    command:
      Отправка фото на rust сервер "http://192.168.0.106:4000/picture"
      curl -d '{"command":3}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command

```
4.Получить звук от esp8266 (esp8266 server 192.168.0.104:80)
```   
    command:
      Запись и отправка звука на rust сервер "http://192.168.0.106:4000/sound"
      curl -d '{"command":1}' -H "Content-Type: application/json" 192.168.0.104:80/command
```
5.Передача параметров управления мотором
```
   command:
     user control
       curl -d '{"command":4,"motor_command":1,"distance":40,"speed":150}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command
     auto control
       curl -d '{"command":5}' -H "Content-Type: application/json" -X POST 192.168.0.105:80/command

 motor_command = 1, // вперед
 motor_command = 2, // назад
 motor_command = 3, // разворот влево
 motor_command = 4, // разворот вправо
 motor_command = 5, // разворот на 180 градусов
 motor_command = 6  // стоп
```