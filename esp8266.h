#ifndef _ESP8266_H_
#define _ESP8266_H_

#define __PROG_TYPES_COMPAT__
#include <Arduino.h>
#include <avr/pgmspace.h>

#define STATUS_ESP_DISCONNECTED 0
#define STATUS_ESP_CONNECTED 1

#define ESP_CMD_RESET F("AT+RST")
#define ESP_CMD_INIT1 F("AT+CIPMUX=1")
#define ESP_CMD_INIT2 F("AT+CIPSERVER=1,23")
#define ESP_CMD_CLOSE F("AT+CIPCLOSE=")
#define ESP_CMD_SEND F("AT+CIPSEND=0,")
#define ESP_CMD_STATUS F("AT+CIPSTATUS")

#define ESP_RESP_LINK F("Link")
#define ESP_RESP_UNLINK F("Unlink")
#define ESP_RESP_IPD F("+IPD,")
#define ESP_RESP_STATUS F("STATUS:")
#define ESP_RESP_CIPSTATUS F("+CIPSTATUS:")

#define ESP_REC_BUF_SIZE 20
#define ESP_IPD_BUF_SIZE 40


class Esp8266
{
public:
    Esp8266();
    ~Esp8266();
    void begin(HardwareSerial *serial);
    void ConfigSettings(bool apMode, char *ssid, char *key);
    uint8_t GetStatus();
    void Disconnect(const __FlashStringHelper *chan);

    char read();
    void print(const __FlashStringHelper *buffer);
    void print(const char *buffer);
    void print(int val);
    void print(double val, int digits);
    void println(const __FlashStringHelper *buffer);
    void println(const char *buffer);
    void println();
    void write(const uint8_t *buffer, size_t size);
    bool SendHeader(int size);
    uint8_t GetIntLen(int val);
    void WaitForSendComplete();
    uint8_t ReadResponse(const prog_char *resp = NULL, unsigned long timeout = 0);
    void clearBuffer(uint8_t avail = 0);
    bool SerialAvailable();

private:
    uint8_t GetRecLen();
    void SendInit(bool resetOnly=false);
    void ReadIpd(uint8_t len);

private:
    HardwareSerial *pSerial;
    char recBuf[ESP_REC_BUF_SIZE];
    char ipdBuf[ESP_IPD_BUF_SIZE];
    uint8_t bufPos;
    uint8_t ipdWritePos;
    uint8_t ipdReadPos;
    uint8_t status;
    int discard;
    bool initFinished;

    const prog_char* resp_ready;
    const prog_char* resp_OK;
    const prog_char* resp_SendOK;
    const prog_char* resp_BG;
};

extern Esp8266 esp8266;

#endif
