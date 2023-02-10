#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspUdp.h>
#include "SoftwareSerial.h"
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
//#include <avr/wdt.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define DHTTYPE DHT11
#define DHTPIN A0
#define SOIL A1

#define NP 9

#define AS 8
#define BB 7
#define BA 6
#define AA 5
#define AB 4

#define RX 2
#define TX 3

#define SEND_DELAY 5*1000

const char* ssid = "joongang203";           // your network SSID (name)
const char* password = "38163816!";           // your network password
const char* mqtt_server = "192.168.0.70";
const int mqtt_port = 1883;

const char* clientID = "AAAABBBB";
const char* outTopic = "outTopic5";
const char* inTopic = "inTopic5";

int status = WL_IDLE_STATUS;   // the Wifi radio's status

const char s_state[2][4] = { "on", "off" };
const int ON = 1;
const int OFF = 0;
//char buf[10];
const float fMax_hum = 60.0;
const float fMax_temp = 30.0;
const float fMin_temp = 22.0;
const int Soil_moisture_reference = 50;

float humi = 0.0;
float temp = 0.0;
int Soil_moisture = 0;
int np_r = 0;
int np_g = 0;
int np_b = 0;
int f_a = 0;
int f_b = 0;
int p_on = 1;
int p_off = 0;



int speakerPin = 11;

int numTones = 10;
int tones[] = {261, 277, 294, 311, 330, 349, 370, 392, 415, 440};

boolean reqLed = false;
boolean reqFan = false;
boolean reqPIEZO = false;

unsigned long lastMsg = 0;
char msg[50];


// Initialize the Ethernet client object
WiFiEspClient espClient;

PubSubClient client(espClient);

SoftwareSerial soft(RX,TX); // RX, TX

DHT dht(DHTPIN, DHTTYPE, DHTPIN);

Adafruit_NeoPixel RGB_LED = Adafruit_NeoPixel(12, NP, NEO_GRB);

LiquidCrystal_I2C lcd(0x27, 16, 2);

//const char t_led_panel_state[] = "S_p/S_sf/S_led";
//const char t_led_panel[] = "P_p/P_sf/P_led";
//const char t_temp[] = "phoenix/smartfarm/temp";
//const char t_cf[] = "p/sf/fan";
//const char t_humi[] = "phoenix/smartfarm/humi";

void setup_wifi() {
   if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, password);
  }

  // you're connected now, so print out the data
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // initialize serial for debugging
  // Serial.begin(9600);
  Serial.begin(115200);
  // initialize serial for ESP module
  //soft.begin(9600);
  Serial3.begin(115200);
  // initialize ESP module
  //WiFi.init(&soft);
  WiFi.init(&Serial3);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  dht.begin();
  lcd.init();
  lcd.backlight();
//
  RGB_LED.begin();                             // Neopixel 초기화
  RGB_LED.setBrightness(200);                  //RGB_LED 밝기조절
  RGB_LED.show();

  pinMode(AS, OUTPUT);
  pinMode(AA, OUTPUT);              
  pinMode(AB, OUTPUT);
  pinMode(BA, OUTPUT);
  pinMode(BB, OUTPUT);
}

void loop() {
  //Serial.println("loop");
  digitalWrite(AS, LOW);
  if (!client.connected()) {
    reconnect();
  }
  delay(50);
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > SEND_DELAY) {
    lastMsg = now;
    doSmartFarm();
    publishes();
  }
  digitalWrite(AS, HIGH);
  delay(50);
}

String int2String(int val) {
  String sval = "";

  int val2 = (val/10)%10;
  int val3 = (val/100)%10;

  if (val3 >= 1) {
    sval = String(val);
  } else {
    if(val2 >= 1) {
      sval = String(val);
    }
  }
  return sval;
}

void WarrningLCD() {
  lcd.clear();
  //delay(200);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Not enough water");
  lcd.setCursor(0, 1);
  lcd.print("supply water!!");
  //delay(200);
}

void FanONOFF(int OnOff_A, int OnOff_B, boolean request = false) {
  if (reqFan && !request) {//둘다 true가 되면 리턴
    return;
  }

  f_a = OnOff_A; f_b = OnOff_B;

  Serial.println("FanONOFF"+String(OnOff_A)+String(OnOff_B));

  if (OnOff_A == 1) {
    digitalWrite(AA, HIGH);
    digitalWrite(AB, LOW);
  } else {
    digitalWrite(AA, LOW);
    digitalWrite(AB, LOW);
  }

//  if (OnOff_B == 1) {
//    digitalWrite(BA, HIGH);
//    digitalWrite(BB, LOW);
//  } else {
//    digitalWrite(BA, LOW);
//    digitalWrite(BB, HIGH);
//  }

  if (request) {
    reqFan = true;
  }
}

void RGB_Color(int r, int g, int b, int wait, boolean request = false) {
  if (reqLed && !request) {
    return;
  }
  np_r = r; np_g = g; np_b = b;
  Serial.println("rgb_color"+String(r)+String(g)+String(b));
  for (int i=0; i<RGB_LED.numPixels(); i++) {
    RGB_LED.setPixelColor(i, RGB_LED.Color(r, g, b));
    RGB_LED.show();
    //delay(wait);
  }
  if (request) {
    reqLed = true;
  }
}

void PIEZO_ONOFF(int Off_P,int On_P,boolean request = false) {
  if (reqPIEZO && !request){
    return;
  }
  p_off = Off_P; p_on = On_P;  

  if (Off_P == 1) {
    noTone(speakerPin);
  }else{
    for (int i = 0; i < numTones; i++)
      tone(speakerPin, tones[i]);
      delay(100);
  }
  if(request){
  reqPIEZO = true;
  }
}

void PIEZO(){
   for (int i = 0; i < numTones; i++)
  {
    tone(speakerPin, tones[i]);
    delay(100);
  }
  noTone(speakerPin);
}

void doSmartFarm() {
  humi = dht.readHumidity();
  temp = dht.readTemperature();
  Soil_moisture = analogRead(A1);
  int C_Soil_moisture = map(Soil_moisture, 1023, 0, 0, 100);

  String strC_Soil = int2String(C_Soil_moisture);

  if(C_Soil_moisture < Soil_moisture_reference){
    PIEZO_ONOFF(1, 0);
    WarrningLCD();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("T : "); lcd.print((int)temp); lcd.print("C ");
    lcd.setCursor(8, 0);
    lcd.print("H : "); lcd.print((int)humi); lcd.print("% ");
    lcd.setCursor(0, 1);
    lcd.print("Soil_M : "); lcd.print(strC_Soil); lcd.print("   ");
  }

  if ((temp > fMax_temp && humi > fMax_hum) || temp > fMax_temp) {
    FanONOFF(ON, ON);
    RGB_Color(0, 0, 0, 10);   
  } else if(humi > fMax_hum) {
    FanONOFF(ON, ON);
    RGB_Color(50, 50, 50, 10);
  } else if(temp < fMin_temp) {
    FanONOFF(OFF, OFF);
    RGB_Color(50, 50, 50, 10);
  } else {
    FanONOFF(OFF, OFF);
    RGB_Color(0, 0, 0, 10);
  }
}
 
  
 
const char* getLedPanel() {
  String str;
  str.concat('1');
  str.concat('/');
  str.concat(np_r);
  str.concat('/');
  str.concat(np_g);
  str.concat('/');
  str.concat(np_b);
  str.toCharArray(msg, str.length()+1);
  return msg;
}

const char* getHumiTemp() {
  String str;
  str.concat('h');
  str.concat('/');
  str.concat(String(humi));
  str.concat('/');
  str.concat('t');
  str.concat('/');
  str.concat(String(temp));
  str.toCharArray(msg, str.length()+1);
  return msg;
}
const char* getFan() {
  String str;
  str.concat("f_a");
  str.concat('/');
  str.concat((digitalRead(AA)==HIGH || digitalRead(AB)==HIGH) ? s_state[0] : s_state[1]);
  str.concat('/');
  str.concat("f_b");
  str.concat('/');
  str.concat((digitalRead(BA)==HIGH || digitalRead(BB)==HIGH) ? s_state[0] : s_state[1]);
  str.toCharArray(msg, str.length()+1);
  return msg;
}

const char* getPIEZO(){
  String str;
  str.concat("p");
  str.concat('/');
  str.concat(s_state[1]);
  str.concat('/');
  str.concat("p");
  str.concat('/');
  str.concat(s_state[0]);
  str.toCharArray(msg, str.length()+1);
  return msg;
}

void publishes() {
  Serial.println("Sending msgs....");
  client.publish(outTopic, getLedPanel());
  client.publish(outTopic, getHumiTemp());
  client.publish(outTopic, getFan());
  client.publish(outTopic, getPIEZO());
}

void subscribes() {
  client.subscribe(inTopic);
}

void getDataFromPayload(String str, String buf[]) {
  int cnt = 0;
  while (str.length() > 0) {
    int index = str.indexOf('/');// '/'가 몇번째에 있는지 
    if (index == -1) {
      buf[cnt++] = str;
      break;
    } else {
      buf[cnt++] = str.substring(0, index);
      str = str.substring(index+1);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  payload[length] = '\0';
  String str = (char*)payload;
  String buf[4];
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("], ");
  Serial.println(str);

  if (strcmp(inTopic, (char*)topic) !=0) {
    return;
  }
  if (str.startsWith("led")) {//led로 시작되는 스트링이면
    getDataFromPayload(str, buf);
    RGB_Color(buf[1].toInt(), buf[2].toInt(), buf[3].toInt(), 10, true);
  } else if (str.startsWith("f_a")) {
    getDataFromPayload(str, buf);
    FanONOFF(buf[1]=="on" ? ON : OFF, buf[3]=="on" ? ON : OFF, true);
  } else if(str.startsWith("p")){
    getDataFromPayload(str, buf);
    PIEZO_ONOFF(buf[1]=="off" ? ON : OFF, buf[3]=="on" ? OFF : ON, true);
  } else if (str.startsWith("req/init")) {
    reqLed = reqFan = reqPIEZO = false;

 }
}
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientID)) {
      Serial.println("connected");
      client.publish(outTopic, clientID);
      subscribes();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds ");
      delay(5000);
    }
  }
}
