#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C


#define BUZZER  5
#define LED_1 15
#define PB_Cancel 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12
#define LDRPIN A0
#define LDRPINR A3
#define SERVMOTOR 16

//Wifi and mqtt clients defining
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);


#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

float time_zone_offset = UTC_OFFSET;

float GAMMA = 0.75;
float lux = 0;
float RL10 = 50;
float MINIMUM_ANGLE = 30;


char TempAr[10];
char LightAr[10];
char Lightpos[10];


int days = 0;
int hours=0;
int minutes=0;
int seconds=0;
int month=0;
int year=0;

unsigned long timeNow = 0;
unsigned long timeLast= 0;
int Light=0; 
int servo_angle=0; 

bool alarm_enabled=true;
int n_alarms=3; 
int alarm_hours[]={0,0,1}; 
int alarm_minutes[]={1,2,10};
bool alarm_triggered[] = {false,false, false};

bool isSheduledON = false;
unsigned long sheduledontime;

int c=262;
int d =296;
int e= 330;
int f= 350;
int g= 392;
int a= 440;
int b= 492;
int C= 532;

int notes[]={c,d,e,f,g,a,b,C};

int n_notes= 8;

int current_mode=0;
int max_modes=5;
String modes[]={ "    1                Set Alarm    1", "    2                Set Alarm    2", "    3                Set Alarm    3", "    4       Disable    Alarm", "    5      Set Time    Zone"};

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);
DHTesp dhtSensor;

//servo moter position
int position = 0;
Servo servo;

// Function declarations
void print_line(String text, int column , int row ,int text_size);
void print_time_temp_humidity();
void update_time();
void update_time_with_set_alarms();
void ring_alarm();
void go_to_menu();
void run_mode(int mode);
void set_time();
void set_alarm(int alarm);


void setup(){
  pinMode(BUZZER,OUTPUT);
  pinMode(LED_1,OUTPUT);
  pinMode(PB_Cancel, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(LDRPIN, INPUT);
  pinMode(LDRPINR, INPUT);
  digitalWrite(BUZZER, LOW);

  dhtSensor.setup(DHTPIN,DHTesp::DHT22);
  servo.attach(SERVMOTOR, 500, 2400);
  
  Serial.begin(115200);
  if (! display.begin(SSD1306_SWITCHCAPVCC,SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 Alocation Failed"));
    for(;;);
  }
  display.display();
  delay(2000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting To Wifi", 0,0,2);
  }
  display.clearDisplay();
  Serial.println("Ip Adress");
  Serial.println(String(WiFi.localIP()));
  print_line(" Connected  To Wifi", 0,0,2);
  Serial.println(" Connected  To Wifi");
  delay(1000);
  
  display.clearDisplay();
  setupmqtt();
  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);
  print_line(" Welcome to Medibox ", 10,20,2);
  delay(2000);
  display.clearDisplay();

  configTime( time_zone_offset,UTC_OFFSET_DST , NTP_SERVER);
}

void loop() {
  if (!mqttClient.connected()){
    connectToBroker();
  }
  mqttClient.loop();
  update_time_with_set_alarms();
  if(digitalRead(PB_OK) == LOW){
    delay(500);
    go_to_menu();
  }


  mqttClient.publish("EE_Medibox-Light-Intensity", LightAr);
  mqttClient.publish("EE_Medibox-Temperature", TempAr);
  mqttClient.publish("EE_Medibox-Light-Position", Lightpos);
  print_time_temp_humidity();

  
  read_from_ldr() ;
  serv_mo();
  checkshedule();
}
void setupmqtt(){
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback);
}

void receiveCallback(char* topic, byte*payload,unsigned int lenght){
  Serial.print("Messege arrived  [");
  Serial.print(topic);
  Serial.print("]");
  char payloadCharAr[lenght];
  for(int i = 0;i<lenght;i++){
    Serial.println((char)payload[i]);
    payloadCharAr[i]=(char)payload[i];

  }
  Serial.print(" \n");
  // minimum angle
  if (strcmp(topic, "EE_Shade-Angle") == 0){
    MINIMUM_ANGLE = atoi(payloadCharAr);
  }
  // control factor
  if (strcmp(topic, "EE_Medibox-Cont-Factor") == 0) {
    GAMMA = atof(payloadCharAr);
  }
  if(strcmp(topic,"EE_Medibox-Alarm")==0){
    buzzeron(payloadCharAr[0]=='t');

  }else if(strcmp(topic,"EE_Sheduled-Alarm")==0){
    if(payloadCharAr[0]=='N'){
      isSheduledON= false;

    }else{
      isSheduledON= true;
      sheduledontime = atol(payloadCharAr);
    }
  }
}




void print_line(String text, int column , int row ,int text_size){
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column,row); //(column,row)
  display.println(text);

  display.display();
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.println("Attempt to connect mqtt");
    if(mqttClient.connect("ESP32-24244555444")){
      Serial.println("Connected to mqtt");
      mqttClient.subscribe("EE_Shade-Angle");
      mqttClient.subscribe("EE_Medibox-Cont-Factor");
      mqttClient.subscribe("EE_Medibox-Alarm");
      mqttClient.subscribe("EE_Sheduled-Alarm");
    }else{
      Serial.println("Connection failed");
      Serial.println(mqttClient.state());
      delay(5000);

    }
  }
}

void buzzeron(bool on){
  if(on){
    tone(BUZZER,256);
  }else{
    noTone(BUZZER);
  }
}

unsigned long getTime(){
  timeClient.update();
  return timeClient.getEpochTime();

}

void read_from_ldr() {
  int analogValueL = analogRead(LDRPIN);
  int analogValueR = analogRead(LDRPINR);
  int analogValue=0;
  

  if(analogValueL<analogValueR){
    analogValue=analogValueL;
    Light=10;
    String(Light).toCharArray(Lightpos,6);


  }else{
    analogValue=analogValueR;
    Light=100;
    String(Light).toCharArray(Lightpos,6);

  }

  //simplify light intensity(range 0-1)
  float voltage = analogValue / 1024. * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float maxlux = pow(RL10 * 1e3 * pow(10, GAMMA) / 322.58, (1 / GAMMA));
  lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA))/maxlux;
  String(lux).toCharArray(LightAr,6);
  Serial.println(lux);

}

void serv_mo(){
  //finding position of servo motor
  if(Light<50){
    position = (MINIMUM_ANGLE*1.5) + ((180 - MINIMUM_ANGLE) * lux * GAMMA);  
  }else{
    position = (MINIMUM_ANGLE*0.5) + ((180 - MINIMUM_ANGLE) * lux * GAMMA);
  }
  Serial.println(position);
  if(position<180){
    servo_angle=position; 
  }else{
    servo_angle=180;

  }
  
  Serial.println(servo_angle);
  servo.write(servo_angle);
}

void checkshedule(){
  if(isSheduledON){
    unsigned long currentTime= getTime();
    if(currentTime>sheduledontime){
      buzzeron(true);
      isSheduledON= false;
      mqttClient.publish("EE_Medibox_Alarm_Switching", "1");
      mqttClient.publish("EE_Medibox_Alarm_Switching", "0");
      Serial.println("Scheduled ON");
      Serial.println(currentTime);
      Serial.println(sheduledontime);
    }

  }

}
//////////////////////////////////////////////////////////////////////////////////////////////
void print_time_temp_humidity() {
    
    display.clearDisplay();

    print_line("Date: " + String(days) + "/" + String(month) + "/" + String(year), 0, 0, 1);

    
    print_line("Time: " + String(hours) + ":" + String(minutes) + ":" + String(seconds), 0, 10, 1);

   
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    
    if (data.temperature > 32) {
        print_line("TEMP HIGH: " + String(data.temperature) + "C", 0, 30, 1);
    } else if (data.temperature < 26) {
        print_line("TEMP LOW: " + String(data.temperature) + "C", 0, 30, 1);
    }

    
    if (data.humidity > 80) {
        print_line("HUMIDITY HIGH: " + String(data.humidity) + "%", 0, 40, 1);
    } else if (data.humidity < 60) {
        print_line("HUMIDITY LOW: " + String(data.humidity) + "%", 0, 40, 1);
    }
    if(alarm_enabled == true){
      print_line("Alarm Enabled", 0,50,1);
    }

    else if(alarm_enabled == false){
      print_line("Alarm Disabled", 0,50,1);
    }
    String(data.temperature,2).toCharArray(TempAr, 10);
    
    display.display();
}

void update_time(){
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour,3,"%H", &timeinfo);
  hours= atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute,3,"%M", &timeinfo);
  minutes= atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond,3,"%S", &timeinfo);
  seconds= atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay,3,"%d", &timeinfo);
  days= atoi(timeDay);

  char timeMonth[3];
  strftime(timeMonth,3,"%m", &timeinfo);
  month= atoi(timeMonth);

  char timeYear[3];
  strftime(timeYear,3,"%Y", &timeinfo);
  year= atoi(timeYear)+4;
}

void update_time_with_set_alarms(void){
  update_time();
  print_time_temp_humidity();
  if(alarm_enabled==true){
    for(int i=0;i<n_alarms;i++){
      if(alarm_triggered[i]==false && alarm_hours[i]==hours && alarm_minutes[i]==minutes){
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

void ring_alarm(){
  display.clearDisplay();
  print_line("  Medicine    Time",0,2,2);

  digitalWrite(LED_1,HIGH);
  bool break_happen = false;

  while( break_happen == false && digitalRead(PB_Cancel) == HIGH){
    for (int i=0; i< n_notes; i++){
      if(digitalRead(PB_Cancel) == LOW){
        delay(200);
        break_happen = true;
        break;
      }
        

      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
  digitalWrite(LED_1,LOW);
  display.clearDisplay();
}

void go_to_menu (){
  while (digitalRead(PB_Cancel)== HIGH){
    display.clearDisplay();
    print_line(modes[current_mode],0,0,2);

    int pressed = digitalRead(PB_UP);
    if( pressed == LOW){
      delay(200);
      current_mode +=1;
      current_mode = current_mode % max_modes;
    } 

    else if( digitalRead(PB_DOWN) == LOW){
      delay(200);
      current_mode -=1;
      if( current_mode <0){
        current_mode= max_modes-1;
      }
    } 
    
    else if( digitalRead(PB_OK) == LOW){
      delay(200);
      
      run_mode(current_mode);
    }
  }
}

void run_mode(int mode){
  switch(mode) {
    case 0:
      set_alarm(1);
      break;
    case 1:
      set_alarm(2);
      break;
    case 2:
      set_alarm(3);
      break;
    case 3:
      if(alarm_enabled == true){
      alarm_enabled= false;
      display.clearDisplay();
      print_line("   Alarm    Disabled", 0,0,2);
      delay(1000);
    }

    else if(alarm_enabled == false){
      alarm_enabled= true;
      display.clearDisplay();
      print_line("    Alarm    Enabled", 0,0,2);
      delay(1000);
    }
      break;
    case 4:
      set_time();
      break;
    default:
      
      break;
  }
}

void set_time() {
  float temp_offset = time_zone_offset;
  while(true){
    display.clearDisplay();
    print_line("Set Time  Offset: " + String(temp_offset),0,0,2);
    int pressed= digitalRead(PB_UP);
    if( pressed == LOW){
      delay(200);
      temp_offset +=1;
      if (temp_offset > 14.0) {
        temp_offset = 0.0;
      }
    } 

    else if( digitalRead(PB_DOWN) == LOW){
      delay(200);
      temp_offset -=0.1;
      if( temp_offset <0.0){
        temp_offset= 14.0;
      }
    } 
    
    else if( digitalRead(PB_OK) == LOW){
      delay(200);
      time_zone_offset=temp_offset;
      break;
    }
    
    else if( digitalRead(PB_Cancel) == LOW){
      delay(200);
      break;
    }
  }
  configTime( time_zone_offset*3600,UTC_OFFSET_DST*3600 , NTP_SERVER);
  display.clearDisplay();
  print_line(" Time is     set", 0,0,2);
  delay(1000);
}

void set_alarm(int alarm) {
  int temp_hours = alarm_hours[alarm];
  while(true){
    display.clearDisplay();
    print_line("Enter     hour:  " + String(temp_hours),0,0,2);
    int pressed= digitalRead(PB_UP);
    if( pressed == LOW){
      delay(200);
      temp_hours +=1;
      temp_hours = temp_hours % 24;
    } 

    else if( digitalRead(PB_DOWN) == LOW){
      delay(200);
      temp_hours -=1;
      if( temp_hours <0){
        temp_hours= 23;
      }
    } 
    
    else if( digitalRead(PB_OK) == LOW){
      delay(200);
      alarm_hours[alarm]= temp_hours; 
      break;
    }
    
    else if( digitalRead(PB_Cancel) == LOW){
      delay(200);
      break;
    }
  }
  int temp_minutes = alarm_minutes[alarm];
  while(true){
    display.clearDisplay();
    print_line("Enter     minute:" + String(temp_minutes),0,0,2);
    int pressed= digitalRead(PB_UP);
    if( pressed == LOW){
      delay(200);
      temp_minutes +=1;
      temp_minutes = temp_minutes % 60;
    } 

    else if( digitalRead(PB_DOWN) == LOW){
      delay(200);
      temp_minutes -=1;
      if( temp_minutes <0){
        temp_minutes= 59;
      }
    } 
    
    else if( digitalRead(PB_OK) == LOW){
      delay(200);
      alarm_minutes[alarm]= temp_minutes; 
      break;
    }
    
    else if( digitalRead(PB_Cancel) == LOW){
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line(" Alarm is    set", 0,0,2);
  delay(1000);
  alarm_triggered[alarm] = false;
}

