#include <WiFi.h>
#include <ESPmDNS.h>

const String ssid = "<userssid_here>";
const String password = "<user_pw_here>";
WiFiServer server = WiFiServer(80);

// led pins
const int GREEN_LED = 2;
const int YELLOW_LED = 18;
const int RED_LED = 19;

int failedAttempts = 0;
bool isLocked = false;
unsigned long lastBlinkTime = 0;
bool redState = LOW;

// set all led function
void setLED(bool green, bool yellow, bool red) {
  digitalWrite(GREEN_LED, green ? HIGH : LOW);
  digitalWrite(YELLOW_LED, yellow ? HIGH : LOW);
  digitalWrite(RED_LED, red ? HIGH : LOW);
}


void setup() {
  Serial.begin(115200);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // system ready state
  setLED(false, true, false);

  // wifi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // setup mdns
  if (MDNS.begin("jiat")) {
    Serial.println("started: http://jiat.local");
  }

  server.begin();
}

void loop() {

  // system lock state
  if (isLocked) {
    if (millis() - lastBlinkTime >= 500) {
      lastBlinkTime = millis();
      redState = !redState;
      digitalWrite(RED_LED, redState);
    }
  }

  WiFiClient client = server.available();
  if (!client) return;

  String request = client.readStringUntil('\r');
  client.flush();

  // web page
  if (request.indexOf("GET / ") != -1 || request.indexOf("GET /index.html") != -1) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();

    // ui
    client.println(R"(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>SMART ACCESS CONTROL SYSTEM</title>
  <style>
    body { 
      font-family: 'Segoe UI', Arial, sans-serif; 
      background: #f0f2f5; 
      display: flex; 
      flex-direction: column;
      justify-content: center; 
      align-items: center; 
      height: 100vh; 
      margin: 0; 
      transition: background 0.3s ease;
    }
    
    /* Top Banner for System Locked State */
    .top-banner {
      display: none;
      font-size: 1.8em;
      font-weight: bold;
      color: #dc3545;
      margin-bottom: 20px;
      align-items: center;
      gap: 10px;
    }

    .card { 
      background: white; 
      padding: 30px; 
      border-radius: 12px; 
      box-shadow: 0 4px 20px rgba(0,0,0,0.06); 
      width: 320px; 
      text-align: center; 
    }
    
    h2 { margin-top: 0; margin-bottom: 20px; color: #1a1a1a; font-size: 1.2em; font-weight: bold; }
    input { width: 92%; padding: 10px; margin: 6px 0; border: 1px solid #e1e4e8; border-radius: 6px; font-size: 14px; outline: none; background: #f8f9fa; box-sizing: border-box; }
    button { width: 92%; padding: 11px; background: #28a745; color: white; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; font-size: 15px; margin-top: 10px; box-sizing: border-box; }
    button:disabled, input:disabled { background: #e9ecef; color: #6c757d; cursor: not-allowed; }
    
    /* Alert Box styles */
    .alert-box { margin: 15px auto 5px auto; width: 92%; padding: 12px 0; border-radius: 8px; font-weight: bold; font-size: 1em; box-sizing: border-box; }
    .alert-ready { background-color: #feffd9; color: #ccc400; border: 1px solid #fff500; }
    .alert-success { background-color: #e8f8ea; color: #00a82d; border: 1px solid #28a745; }
    .alert-danger { background-color: #fde8e8; color: #d93025; border: 1px solid #dc3545; }
    
    #attempts { margin-top: 8px; color: #6c757d; font-size: 0.85em; }

    /* Locked state background styling */
    body.locked-bg { background-color: #fce8e8; }
  </style>
</head>
<body>

  <!-- Top Banner (Hidden by default) -->
  <div id='topBanner' class='top-banner'>
    System Locked &#128683;
  </div>

  <div class='card'>
    <h2>SMART ACCESS CONTROL<br>SYSTEM</h2>
    <input type='text' id='username' placeholder='Username'><br>
    <input type='password' id='password' placeholder='Password'><br>
    <button id='loginBtn' onclick='login()'>Login</button>
    
    <!-- Inner Alert Box -->
    <div id='status' class='alert-box alert-ready'>System Ready</div>
    <div id='attempts'>Failed Attempts: 0/3</div>
  </div>

  <script>
// locked voice
  function speakLockMessage() {
  if ('speechSynthesis' in window) {
    window.speechSynthesis.cancel(); 
    
    let msg = new SpeechSynthesisUtterance("System Locked, Please restart device");
    msg.rate = 0.9;  
    msg.pitch = 1.0; 
    msg.lang = 'en-US';

    msg.onend = function() {
      window.speechSynthesis.speak(msg);
    };
    
    window.speechSynthesis.speak(msg);
  }
}

    function login() {
      let u = document.getElementById('username').value;
      let p = document.getElementById('password').value;
      if(!u || !p) { alert('Please enter both username and password'); return; }
      
      fetch('/login?username=' + encodeURIComponent(u) + '&password=' + encodeURIComponent(p))
      .then(res => res.json())
      .then(data => {
        let statusDiv = document.getElementById('status');
        
        if(data.success) {
          statusDiv.innerText = data.message;
          statusDiv.className = 'alert-box alert-success';
        } else {
          statusDiv.className = 'alert-box alert-danger';
          
          // if system locked
          if(data.locked) {
            document.getElementById('topBanner').style.display = 'flex';
            document.body.classList.add('locked-bg');
            statusDiv.innerText = 'Please restart device!';
            
            document.getElementById('username').disabled = true;
            document.getElementById('password').disabled = true;
            document.getElementById('loginBtn').disabled = true;

            speakLockMessage();
          } else {
            statusDiv.innerText = data.message;
          }
        }
        
        document.getElementById('attempts').innerText = 'Failed Attempts: ' + data.attempts + '/3';
      });
    }

  </script>
</body>
</html>
    )");
  } else if (request.indexOf("GET /login") != -1) {  // login
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();


    if (isLocked) {
      client.println(R"({
        "success": false,
        "status": "LOCKED",
        "message": "SYSTEM LOCKED",
        "attempts": 3,
        "locked": true
      })");
      return;
    }

    String reqUser = "";
    String reqPass = "";

    int uIndex = request.indexOf("username=");
    int pIndex = request.indexOf("password=");

    // /login?username=admin&password=1234 HTTP/1.1

    if (uIndex != -1 && pIndex != -1) {
      int uEnd = request.indexOf("&", uIndex);
      reqUser = request.substring(uIndex + 9, uEnd);

      int pEnd = request.indexOf(" ", pIndex);
      if (pEnd == -1) pEnd = request.indexOf("&", pIndex);
      reqPass = request.substring(pIndex + 9, pEnd);
    }

    // check credentials
    if (reqUser == "admin" && reqPass == "1234") {
      failedAttempts = 0;
      setLED(true, false, false);  // green on
      client.println(R"({
        "success": true,
        "status": "ACCESS_GRANTED",
        "message": "Access Granted",
        "attempts": 0,
        "locked": false
      })");
    } else {
      // system lock state
      failedAttempts++;
      if (failedAttempts >= 3) {
        isLocked = true;
        setLED(false, false, false);  // blink loop handles
        client.println(R"({
          "success": false,
          "status": "SYSTEM_LOCKED",
          "message": "SYSTEM LOCKED",
          "attempts": 3,
          "locked": true
        })");
      } else {
        setLED(false, false, true);  // red on
        String jsonRes = String(R"({
          "success": false,
          "status": "ACCESS_DENIED",
          "message": "Access Denied",
          "attempts": )") + String(failedAttempts)
                         + R"(,
          "locked": false
        })";

        client.println(jsonRes);
      }
    }
  }
}
