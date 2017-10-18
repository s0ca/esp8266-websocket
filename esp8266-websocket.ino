#include <ESP8266WiFi.h>
#include <Hash.h>
#include <rBase64.h>
#include <map>
#include <string>

static const std::string  w_ssid("elevator_test");
static const std::string  w_password("azertyuiop");
static WiFiServer         w_server(81);

#define SWITCH            (int)5 // D1 = GPIO5
static char               pinBuff[32] = {-1};

#define ENDL              std::string("\r\n")
static const std::string  http_headers("HTTP/1.1 200 OK"+ENDL+"Content-Type: text/html"+ENDL+"Refresh: 5; URL=/"+ENDL+ENDL);
static const std::string  ws_headers("HTTP/1.1 101 Switching Protocols"+ENDL+"Upgrade: websocket"+ENDL+"Connection: Upgrade"+ENDL);
static const std::string  wsocket_magic("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

class WS_Client {
  public:
    WiFiClient                              w_client;
    std::map<std::string, std::string>      request;

    WS_Client(WiFiClient c, std::map<std::string, std::string> r) : w_client(c), request(r) {
      // Sec-WebSocket-Accept token generation + http header construct
      uint8_t hash[20];
      sha1((this->request.find("Sec-WebSocket-Key: ")->second + wsocket_magic).c_str(), hash);
      std::string hs(ws_headers+"Sec-WebSocket-Accept: "+rbase64.encode(hash, 20).c_str()+ENDL+ENDL);
      
      Serial.println("[Debug] web socket handShake");
      Serial.println(hs.c_str());
      Serial.println("[Debug] == END==");

      // send websocket handshake
      c.print(hs.c_str());
    };
};

static std::map<std::string, WS_Client>  ws_clients;

void setup() {
  // Serial Init
  Serial.begin(115200);
  Serial.printf("Initialisation en cours ...\n");
  // Wifi init
  WiFi.mode(WIFI_AP);
  WiFi.softAP(w_ssid.c_str(), w_password.c_str());
  Serial.printf("Wifi Started !\nssid: %s\tpwd: %s\nip :%d.%d.%d.%d\n", \
                w_ssid.c_str(), w_password.c_str(), \
                WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
  // http sever started at port 81 (see. line 9)
  w_server.begin();
  Serial.printf("Server started at port: 81\n");
  // GPIO setting
  pinMode(SWITCH, INPUT_PULLUP);
}

const std::string ws_header[] = {
  "GET ",
  "Connection: ",
  "Host: ",
  "Sec-WebSocket-Key: ",
  "Sec-WebSocket-Version: ",
  "Upgrade: "
};

static char       buff[32] = {-1};

void loop() {
  // GPIO reading + convertion to standard c null terminated string
  sprintf(pinBuff, "%d", digitalRead(SWITCH));
  // convertion from c string to c++ string
  const std::string  pinStatus(pinBuff);

  WiFiClient client = w_server.available();
  if (client) {
    // Geting and filtering http headers
    std::map<std::string, std::string>  request;
    Serial.println("[Debug] == Received request ==");
    for (String r = client.readStringUntil('\n'); r != NULL; r = client.readStringUntil('\n')) {
      Serial.println("[Debug] "+r);
      for (size_t i = 0; i != sizeof(ws_header) / sizeof(std::string); ++i) {
        if (strncmp(r.c_str(), ws_header[i].c_str(), ws_header[i].length()) == 0) {
          request.insert(std::pair<std::string, std::string>(ws_header[i], std::string(r.c_str()).substr(ws_header[i].length(), r.length() - ws_header[i].length())));
          break ;
        }
      }
    }
    Serial.println("[Debug] == END==");

    // Debug Saved request
    Serial.println("[Debug] == Saved request ==");
    for (auto it = request.begin(); it != request.end(); ++it) {
      Serial.println(("[Debug] " + it->first + it->second).c_str());
    }
    Serial.println("[Debug] == END==");

    // Saving / filetring websocket clients
    std::map<std::string, std::string>::iterator it;
    if (((it = request.find("GET ")) != request.end() && strncmp(it->second.c_str(), "/", 1) != 0) \
      || ((it = request.find("Connection: ")) != request.end() && strncmp(it->second.c_str(), "Upgrade", 7) != 0) \
      || ((it = request.find("Upgrade: ")) != request.end() && strncmp(it->second.c_str(), "websocket", 9) != 0) \
      || (it = request.find("Sec-WebSocket-Key: ")) == request.end()) {
      // if not http or websocket send http answer
      client.print((http_headers+pinStatus+ENDL).c_str());
    } else {
      // Saving websocket clients by ip + websocket handshake in WS_client constructor
      sprintf(buff, "%d.%d.%d.%d", client.remoteIP()[0], client.remoteIP()[1], client.remoteIP()[2], client.remoteIP()[3]);
      const std::string remoteIP(buff);
      ws_clients.insert(std::pair<std::string, WS_Client>(remoteIP, WS_Client(client, request)));
    }
  }

  // Saved clients reading/sending
  for (auto it = ws_clients.begin(); it != ws_clients.end(); ++it) {
    sprintf(buff, "%d.%d.%d.%d", it->second.w_client.remoteIP()[0], it->second.w_client.remoteIP()[1], it->second.w_client.remoteIP()[2], it->second.w_client.remoteIP()[3]);
    const std::string remoteIP(buff);

    if (remoteIP == "0.0.0.0") {
      // remove closed connections
      it->second.w_client.stop();
      ws_clients.erase(it);
      continue ;
    }
    // reading not tested / tbi
    for (String r = it->second.w_client.readStringUntil('\n'); r != NULL; r = it->second.w_client.readStringUntil('\n')) {
      Serial.println(r);
      Serial.println(".");
    }
    // sending tbi
    //it->second.w_client.print((ws_headers+ENDL+pinStatus+ENDL).c_str());
  }
}
