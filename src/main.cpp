#include "Arduino.h"
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <SD.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

const char* WIFI_SSID = "Robocode_2.4G";
const char* WIFI_PASS = "robocode220";

AsyncWebServer server(80);

AudioGenerator *decoder = NULL;
AudioFileSourceSD *fileSource = NULL;
AudioOutputI2S *out = NULL;

String fileToPlay = "";

void setupServer();
void playAudioFile(String filename);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void playRandomTrack();

// --- ВБУДОВАНИЙ HTML / CSS / JS (УКРАЇНСЬКОЮ) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Панель WALL-E</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1e272e; color: #d2dae2; margin: 0; padding: 20px; text-align: center; }
        h1 { margin-bottom: 30px; color: #0fbcf9; }
        .layout { display: flex; flex-wrap: wrap; gap: 20px; justify-content: center; align-items: flex-start; }
        .panel { background: #2d3436; padding: 20px; border-radius: 15px; flex: 1; min-width: 320px; max-width: 450px; box-shadow: 0 8px 15px rgba(0,0,0,0.3); }
        h3 { border-bottom: 2px solid #0fbcf9; padding-bottom: 10px; margin-top: 0; }
        button { background: #0fbcf9; border: none; color: #1e272e; padding: 10px 15px; border-radius: 8px; cursor: pointer; font-size: 14px; font-weight: bold; transition: 0.2s; margin: 5px; }
        button:hover { background: #4bcffa; }
        button:active { transform: scale(0.95); }
        .btn-red { background: #ff3f34; color: white; }
        .btn-red:hover { background: #ff5e57; }
        .btn-green { background: #0be881; color: #1e272e; }
        .file-item { display: flex; justify-content: space-between; align-items: center; background: #353b48; padding: 10px; border-radius: 8px; margin: 8px 0; border-left: 4px solid #0fbcf9; }
        .file-name { word-break: break-all; text-align: left; flex-grow: 1; margin-right: 10px; font-size: 14px; }
        .file-actions button { padding: 6px 10px; margin: 2px; font-size: 12px; }
        .dpad { display: grid; grid-template-columns: repeat(3, 70px); gap: 8px; justify-content: center; margin: 20px 0; }
        .dpad button { height: 70px; font-size: 24px; margin: 0; user-select: none; -webkit-user-select: none; touch-action: none; }
        .empty { visibility: hidden; }
        .control-group { background: #353b48; padding: 15px; border-radius: 10px; margin-bottom: 15px; text-align: center; }
        input[type="range"] { width: 100%; margin: 10px 0; }
        input[type="color"] { width: 60px; height: 40px; border: none; background: transparent; cursor: pointer; vertical-align: middle; }
    </style>
</head>
<body>
    <h1>🤖 Керування WALL-E</h1>
    <div class="layout">
        <div class="panel">
            <h3>🎵 Файли та Аудіо</h3>
            <div class="control-group">
                <form id="uploadForm">
                    <input type="file" id="fileInput" accept=".wav,.mp3" required style="margin-bottom: 10px; width:100%;">
                    <button type="button" class="btn-green" onclick="uploadFile()" style="width: 100%;">☁️ Завантажити на SD</button>
                </form>
                <p id="status" style="font-size: 12px; margin-top:5px;"></p>
            </div>
            <button onclick="loadFiles()">🔄 Оновити список</button>
            <button class="btn-red" onclick="sendCommand('/stop_audio')">⏹ Зупинити звук</button>
            <div id="playlist" style="margin-top: 15px;"></div>
        </div>

        <div class="panel">
            <h3>🕹️ Керування</h3>
            <div class="control-group">
                <strong>⚡ Швидкість моторів:</strong><br>
                <input type="range" id="speedSlider" min="80" max="255" value="255" onchange="sendRobotCmd('V' + this.value)">
                <span id="speedVal">255</span>
            </div>
            <div class="dpad">
                <div class="empty"></div>
                <button onmousedown="sendRobotCmd('F')" onmouseup="sendRobotCmd('S')" onmouseleave="sendRobotCmd('S')" ontouchstart="sendRobotCmd('F')" ontouchend="sendRobotCmd('S')" oncontextmenu="return false;">▲</button>
                <div class="empty"></div>
                <button onmousedown="sendRobotCmd('l')" onmouseup="sendRobotCmd('S')" onmouseleave="sendRobotCmd('S')" ontouchstart="sendRobotCmd('l')" ontouchend="sendRobotCmd('S')" oncontextmenu="return false;">◄</button>
                <button class="btn-red" onclick="sendRobotCmd('S')" oncontextmenu="return false;">🛑</button>
                <button onmousedown="sendRobotCmd('r')" onmouseup="sendRobotCmd('S')" onmouseleave="sendRobotCmd('S')" ontouchstart="sendRobotCmd('r')" ontouchend="sendRobotCmd('S')" oncontextmenu="return false;">►</button>
                <div class="empty"></div>
                <button onmousedown="sendRobotCmd('B')" onmouseup="sendRobotCmd('S')" onmouseleave="sendRobotCmd('S')" ontouchstart="sendRobotCmd('B')" ontouchend="sendRobotCmd('S')" oncontextmenu="return false;">▼</button>
            </div>
            <div class="control-group">
                <strong>💡 Основне світло:</strong><br>
                <button onclick="sendRobotCmd('L')">Увімкнути</button>
                <button onclick="sendRobotCmd('Q')">Вимкнути</button>
            </div>
            <div class="control-group">
                <strong>🦾 Поворот голови (Серво):</strong><br>
                <input type="range" id="servoSlider" min="0" max="180" value="90" onchange="sendRobotCmd('A' + this.value)">
                <span id="servoVal">90°</span>
            </div>
            <div class="control-group">
                <strong>🌈 Колір очей (LED):</strong><br>
                <input type="color" id="colorPicker" value="#ff0000" onchange="sendColor()">
                <button onclick="sendRobotCmd('c')">🎲 Випадковий</button>
                <button onclick="sendRobotCmd('y')">🔄 Переливання</button>
            </div>
        </div>
    </div>
    <script>
        function uploadFile() {
            const input = document.getElementById('fileInput');
            const status = document.getElementById('status');
            if (!input.files.length) return;
            const formData = new FormData();
            formData.append("file", input.files[0]);
            status.innerText = "Завантаження..."; status.style.color = "#f1c40f";
            fetch('/upload', { method: 'POST', body: formData })
            .then(res => res.text()).then(data => {
                status.innerText = data; status.style.color = "#0be881";
                loadFiles(); input.value = "";
            }).catch(() => { status.innerText = "Помилка"; status.style.color = "#ff3f34"; });
        }
        function loadFiles() {
            fetch('/list').then(res => res.json()).then(files => {
                const playlist = document.getElementById('playlist');
                playlist.innerHTML = "";
                if (!files.length) { playlist.innerHTML = "<p>Файлів немає</p>"; return; }
                files.forEach(file => {
                    playlist.innerHTML += `
                    <div class="file-item">
                        <span class="file-name">${file}</span>
                        <div class="file-actions">
                            <button class="btn-green" onclick="fetch('/play?file=${file}')">▶</button>
                            <button onclick="renameFile('${file}')">✏️</button>
                            <button class="btn-red" onclick="deleteFile('${file}')">🗑️</button>
                        </div>
                    </div>`;
                });
            });
        }
        function deleteFile(filename) {
            if(confirm('Точно видалити ' + filename + '?')) fetch(`/delete?file=${filename}`).then(() => loadFiles());
        }
        function renameFile(oldName) {
            let newName = prompt("Введіть нове ім'я (з .wav або .mp3):", oldName.replace('/', ''));
            if(newName && newName !== oldName) {
                if(!newName.startsWith('/')) newName = '/' + newName;
                fetch(`/rename?old=${oldName}&new=${newName}`).then(() => loadFiles());
            }
        }
        function sendCommand(cmd) { fetch(cmd); }
        function sendRobotCmd(cmd) { fetch(`/robot_cmd?c=${cmd}`); }
        function sendColor() {
            let hex = document.getElementById('colorPicker').value;
            fetch(`/robot_cmd?c=C${hex}`);
        }
        document.getElementById('speedSlider').addEventListener('input', function() {
            document.getElementById('speedVal').innerText = this.value;
        });
        document.getElementById('servoSlider').addEventListener('input', function() {
            document.getElementById('servoVal').innerText = this.value + '°';
        });
        window.onload = loadFiles;
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // Ініціалізація генератора випадкових чисел
  randomSeed(esp_random());

  if (!SD.begin()) {
    Serial.println("Помилка SD карти!");
    return;
  }

  out = new AudioOutputI2S(0, 1); 

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  String myIP = WiFi.localIP().toString();
  Serial.print("\nIP: ");
  Serial.println(myIP);

  Serial2.print("#IP:" + myIP + "\n");
  setupServer();
}

void loop() {
  if (fileToPlay != "") { playAudioFile(fileToPlay); fileToPlay = ""; }
  
  if (decoder && decoder->isRunning()) {
    if (!decoder->loop()) { 
      decoder->stop(); 
      Serial2.print("#ST:\n");
    }
  }

  // --- ЧИТАННЯ КОМАНД ВІД MEGA ---
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();
    if (msg == "X") {
      playRandomTrack();
    }
  }
}

void playRandomTrack() {
  File root = SD.open("/");
  int count = 0;
  File file = root.openNextFile();
  // Рахуємо скільки всього аудіофайлів
  while(file) {
    if(!file.isDirectory()) {
      String name = String(file.name());
      if(name.endsWith(".wav") || name.endsWith(".mp3") || name.endsWith(".WAV") || name.endsWith(".MP3")) count++;
    }
    file = root.openNextFile();
  }
  if(count == 0) return;

  // Вибираємо випадковий індекс
  int randIndex = random(0, count);
  root.rewindDirectory();
  file = root.openNextFile();
  int currentIndex = 0;
  
  // Шукаємо файл під цим індексом
  while(file) {
    if(!file.isDirectory()) {
      String name = String(file.name());
      if(name.endsWith(".wav") || name.endsWith(".mp3") || name.endsWith(".WAV") || name.endsWith(".MP3")) {
        if(currentIndex == randIndex) {
          fileToPlay = "/" + name;
          Serial.println("Сонар тригер! Рандомний трек: " + fileToPlay);
          break;
        }
        currentIndex++;
      }
    }
    file = root.openNextFile();
  }
}

void playAudioFile(String filename) {
  if (decoder && decoder->isRunning()) decoder->stop();
  if (fileSource) { delete fileSource; fileSource = NULL; }
  if (decoder) { delete decoder; decoder = NULL; }

  String dispName = filename;
  if (dispName.startsWith("/")) dispName = dispName.substring(1);
  if (dispName.length() > 16) dispName = dispName.substring(0, 16);
  
  Serial2.print("#TR:" + dispName + "\n");

  fileSource = new AudioFileSourceSD(filename.c_str());
  if (filename.endsWith(".mp3") || filename.endsWith(".MP3")) decoder = new AudioGeneratorMP3();
  else if (filename.endsWith(".wav") || filename.endsWith(".WAV")) decoder = new AudioGeneratorWAV();
  else return;

  decoder->begin(fileSource, out);
}

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) request->_tempFile = SD.open("/" + filename, FILE_WRITE);
  if (len && request->_tempFile) request->_tempFile.write(data, len);
  if (final && request->_tempFile) request->_tempFile.close();
}

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html);
  });
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest * request) {
    String json = "[";
    File root = SD.open("/");
    File file = root.openNextFile();
    bool first = true;
    while(file){
      if(!file.isDirectory()){
        String name = String(file.name());
        if(name.endsWith(".wav") || name.endsWith(".mp3") || name.endsWith(".WAV") || name.endsWith(".MP3")) {
          if(!first) json += ",";
          json += "\"" + (name.startsWith("/") ? name : "/" + name) + "\"";
          first = false;
        }
      }
      file = root.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });
  server.on("/play", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("file")) fileToPlay = request->getParam("file")->value();
    request->send(200, "text/plain", "OK");
  });
  server.on("/stop_audio", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (decoder && decoder->isRunning()) { decoder->stop(); Serial2.print("#ST:\n"); }
    request->send(200, "text/plain", "OK");
  });
  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("file")) SD.remove(request->getParam("file")->value());
    request->send(200, "text/plain", "Deleted");
  });
  server.on("/rename", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("old") && request->hasParam("new")) {
      SD.rename(request->getParam("old")->value(), request->getParam("new")->value());
    }
    request->send(200, "text/plain", "Renamed");
  });
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", "Збережено!");
  }, handleUpload);
  server.on("/robot_cmd", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (request->hasParam("c")) {
      String cmd = request->getParam("c")->value();
      Serial2.print(cmd);
      Serial2.print('\n'); 
      Serial.println("Відправлено на Mega: " + cmd);
    }
    request->send(200, "text/plain", "CMD OK");
  });

  server.begin();
}