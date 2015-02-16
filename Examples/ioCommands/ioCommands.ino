#include <microBoxEsp.h>

char historyBuf[100];
char hostname[] = "ioBash";
char password[] = "password";

PARAM_ENTRY Params[]=
{
  {"hostname", hostname, PARTYPE_STRING | PARTYPE_RW, sizeof(hostname), NULL, NULL, 0}, 
  {"password", password, PARTYPE_STRING | PARTYPE_RW, sizeof(password), NULL, NULL, 0},
  {NULL, NULL}
};

void getMillis(char **param, uint8_t parCnt)
{
  Serial.println(millis());
}

void freeRam(char **param, uint8_t parCnt) 
{
  extern int __heap_start, *__brkval;
  int v;
  esp8266.print((int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
  esp8266.println();
}

void writePin(char **param, uint8_t parCnt)
{
    uint8_t pin, pinval;
    if(parCnt == 2)
    {
        pin = atoi(param[0]);
        pinval = atoi(param[1]);
        digitalWrite(pin, pinval);
    }
    else
        esp8266.println(F("Usage: writepin pinNum pinvalue"));
}

void readPin(char **param, uint8_t parCnt)
{
    uint8_t pin;
    if(parCnt == 1)
    {
        pin = atoi(param[0]);
        esp8266.print(digitalRead(pin));
        esp8266.println();
    }
    else
        esp8266.println(F("Usage: readpin pinNum"));
}

void setPinDirection(char **param, uint8_t parCnt)
{
    uint8_t pin, pindir;
    if(parCnt == 2)
    {
        pin = atoi(param[0]);
        if(strcmp(param[1], "out") == 0)
            pindir = OUTPUT;
        else
            pindir = INPUT;

        pinMode(pin, pindir);
    }
    else
        esp8266.println(F("Usage: setpindir pinNum in|out"));
}

void readAnalogPin(char **param, uint8_t parCnt)
{
    uint8_t pin;
    if(parCnt == 1)
    {
        pin = atoi(param[0]);
        esp8266.print(analogRead(pin));
        esp8266.println();
    }
    else
        esp8266.println(F("Usage: readanalog pinNum"));
}

void writeAnalogPin(char **param, uint8_t parCnt)
{
    uint8_t pin, pinval;
    if(parCnt == 2)
    {
        pin = atoi(param[0]);
        pinval = atoi(param[1]);
        analogWrite(pin, pinval);
    }
    else
        esp8266.println(F("Usage: writeanalog pinNum pinvalue"));
}

void setup()
{
  Serial.begin(115200);
  microbox.begin(&Params[0], hostname, password, historyBuf, 100);
  microbox.AddCommand("free", freeRam);
  microbox.AddCommand("millis", getMillis);
  microbox.AddCommand("readanalog", readAnalogPin);
  microbox.AddCommand("readpin", readPin);
  microbox.AddCommand("setpindir", setPinDirection);
  microbox.AddCommand("writeanalog", writeAnalogPin);
  microbox.AddCommand("writepin", writePin);
// Uncomment below to configure esp8266 module, configure call is only needed once
//  esp8266.ConfigSettings(false,"myssid", "mykey");
}

void loop()
{ 
  microbox.cmdParser();
}
