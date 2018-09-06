
#include <stdlib.h>
#include <SoftwareSerial.h>

#define DEBUG (0)

#define FAN_BTN_PIN   5
#define FAN_MAINTAIN_SIGANL_PIN 3
#define FAN_MAINTAIN_LED_PIN    2 

#define FAN_PWR_LEVEL_CLOSE   0
#define FAN_PWR_LEVEL_1       1
#define FAN_PWR_LEVEL_2       2
#define FAN_PWR_LEVEL_3       3

SoftwareSerial sser(8asz, 7);  // RX TX

const char* cmd_pwr_level = "pwr_level:";
static uint8_t _local_pwr_level = FAN_PWR_LEVEL_CLOSE;

void setup() {
  pinMode(FAN_BTN_PIN, OUTPUT);
  digitalWrite(FAN_BTN_PIN, HIGH); 
  pinMode(FAN_MAINTAIN_SIGANL_PIN, INPUT);
  pinMode(FAN_MAINTAIN_LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  sser.begin(9600);

  Serial.println("Start...");
}

void loop() {
  uint8_t new_pwr_level;
  String command = "";
  char _command[32] = {'\0'};  
  
  while(!sser.available())
  {
     if(LOW == digitalRead(FAN_MAINTAIN_SIGANL_PIN))
    {
      delay(5);
      if(LOW == digitalRead(FAN_MAINTAIN_SIGANL_PIN))
      { 
        sser.println("Need Maintain");
        digitalWrite(FAN_MAINTAIN_LED_PIN, HIGH);
        while(LOW == digitalRead(FAN_MAINTAIN_SIGANL_PIN));
      }
    } 
    else
    {
      digitalWrite(FAN_MAINTAIN_LED_PIN, LOW); 
    }
  }
  
  command = sser.readStringUntil('\n');
#if (DEBUG==1)
  Serial.print("Recv command: ");
  Serial.println(command);
#endif
  command.toCharArray(_command, (uint8_t)command.length());
  new_pwr_level = parsePwrLevel(_command);
#if (DEBUG==1)
  Serial.print("new_pwr_level: ");
  Serial.println(new_pwr_level);
#endif
  if(_local_pwr_level != new_pwr_level)
  {    
    set_pwr_level(new_pwr_level);
    _local_pwr_level = new_pwr_level;  
  }  
}

uint8_t parsePwrLevel(uint8_t *message)
{
  int pwr_level = _local_pwr_level;
  char *p;
  char *s;
  
  if(NULL != (p = strstr(message, cmd_pwr_level)))
  {
    if( NULL != (s = strtok(message, ":")))
    {      
      s = strtok(NULL, ':');
      pwr_level = atoi(s);
      if(!(0 <= pwr_level && pwr_level < 4))
      {        
#if (DEBUG==1)        
        Serial.print("Received ");
        Serial.print(pwr_level);
        Serial.println(", invalid pwr level, return _local_pwr_level");
#endif       
        pwr_level = _local_pwr_level;
      } 
    }         
  } 
  return pwr_level;
}

void set_pwr_level(uint8_t new_pwr_level)
{
  bool forward;
  int8_t diff = new_pwr_level - _local_pwr_level;
  if(0 == diff) 
  { 
#if (DEBUG==1)    
    Serial.println("Same level as before.");
#endif  
    return;
  }  
  forward = diff > 0;
  if(forward)
  {
     fan_pulse(diff);
  } 
  else
  {
    fan_pulse(4 + diff);
  }
}

void fan_pulse(uint8_t pulse)
{  
  for(uint8_t i = 0; i < pulse; i++)
  {
    digitalWrite(FAN_BTN_PIN, LOW);
    delay(500);
    digitalWrite(FAN_BTN_PIN, HIGH);
    delay(100);    
  }  
}

