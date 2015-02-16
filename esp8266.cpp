#include <esp8266.h>

Esp8266 esp8266;

Esp8266::Esp8266()
{
    bufPos = 0;
    ipdWritePos = 0;
    ipdReadPos = 0;
    discard = 0;
    resp_ready = (const prog_char*)(F("ready"));
    resp_OK = (const prog_char*)(F("OK"));
    resp_BG = (const prog_char*)(F(">"));
    resp_SendOK = (const prog_char*)(F("SEND OK"));
}

Esp8266::~Esp8266()
{
}

void Esp8266::begin(HardwareSerial *serial)
{
    pSerial = serial;
    SendInit();
}

void Esp8266::ConfigSettings(bool apMode, char *ssid, char *key)
{
    SendInit(true);

    pSerial->print(F("AT+CWMODE="));
    if(apMode)
        pSerial->println(F("2"));  // Set AP mode
    else
        pSerial->println(F("1")); // Set STA mode

    ReadResponse(resp_OK, 2000);

    pSerial->print(F("AT+CW"));
    if(apMode)
        pSerial->print(F("S"));  // Set AP mode
    else
        pSerial->print(F("J")); // Set STA mode

    pSerial->print(F("AP=\""));
    pSerial->print(ssid);
    pSerial->print(F("\",\""));
    pSerial->print(key);
    if(apMode)
        pSerial->println(F("\",8,4"));
    else
        pSerial->println(F("\""));
    ReadResponse(resp_OK, 30000);
    SendInit();
}

void Esp8266::SendInit(bool resetOnly)
{
    initFinished = false;
    pSerial->println(ESP_CMD_RESET);
    ReadResponse(resp_ready, 2500);

    if(!resetOnly)
    {
        pSerial->println(ESP_CMD_INIT1);
        ReadResponse(resp_OK, 1000);
        pSerial->println(ESP_CMD_INIT2);
        ReadResponse(resp_OK, 1000);
    }
    initFinished = true;
    status = STATUS_ESP_DISCONNECTED;
}

void Esp8266::WaitForSendComplete()
{
    ReadResponse(resp_SendOK, 2000);
}

uint8_t Esp8266::ReadResponse(const prog_char *resp, unsigned long timeout)
{
    unsigned char ch;
    unsigned long start = millis();

    if(!resp && ipdWritePos)
        return ipdWritePos;

    do
    {
        while(pSerial->available())
        {
            ch = pSerial->read();
            if(discard)
            {
                discard--;
                if(!discard)
                {
                    Disconnect(F("1"));
                }
                continue;
            }
            if(ch == '\n')
            {
                bufPos = 0;
                continue;
            }
            if(ch != '\r')
            {
                if(bufPos < (ESP_REC_BUF_SIZE-1))
                {
                    recBuf[bufPos++] = ch;
                    recBuf[bufPos] = 0;
                    if(strstr_P(recBuf, (const prog_char*)ESP_RESP_LINK) != NULL)
                    {
                        status = STATUS_ESP_CONNECTED;
                    }
                    else if(strstr_P(recBuf, (const prog_char*)ESP_RESP_UNLINK) != NULL)
                    {
                        status = STATUS_ESP_DISCONNECTED;
                    }
                    else if(strstr_P(recBuf, resp_ready) != NULL && initFinished)
                    {
                        SendInit();
                    }
                    else if(strstr_P(recBuf, (const prog_char*)ESP_RESP_IPD) != NULL)
                    {
                        if(ch == ':')
                        {
                            uint8_t len = GetRecLen();
                            if(resp)
                            {
                                ReadIpd(len);
                                continue;
                            }
                            else
                                return len;
                        }
                    }
                    else if(resp)
                    {
                        if(strncmp_P(recBuf, resp, strlen_P(resp)) == 0)
                        {
                            return 1;
                        }
                    }
                }
            }
        }
    }while(timeout && (millis() < start+timeout));

    return 0;
}

void Esp8266::ReadIpd(uint8_t len)
{
    char ch;
    do
    {
        while(!pSerial->available());
        ch = pSerial->read();
        if(ipdWritePos < ESP_IPD_BUF_SIZE)
            ipdBuf[ipdWritePos++] = ch;
    }while(--len);
}

char Esp8266::read()
{
    if(ipdReadPos < ipdWritePos)
    {
        char ret = ipdBuf[ipdReadPos++];
        if(ipdReadPos == ipdWritePos)
        {
            ipdReadPos = 0;
            ipdWritePos = 0;
        }
        return ret;
    }
    else
        return pSerial->read();
}

bool Esp8266::SerialAvailable()
{
    if(ipdReadPos < ipdWritePos)
        return true;
    else
        return pSerial->available();
}

uint8_t Esp8266::GetStatus()
{
    return status;
}

void Esp8266::clearBuffer(uint8_t avail)
{
    while(avail--)
    {
        read();
    }
    avail = esp8266.ReadResponse();
    while(avail--)
    {
        read();
    }
    ipdReadPos = 0;
    ipdWritePos = 0;
}

void Esp8266::Disconnect(const __FlashStringHelper *chan)
{
    pSerial->print(ESP_CMD_CLOSE);
    pSerial->println(chan);
    ReadResponse(resp_OK, 1000);
}

void Esp8266::print(const __FlashStringHelper *buffer)
{
    if(status)
    {
        if(SendHeader(strlen_P((const prog_char*)buffer)))
        {
            pSerial->print(buffer);
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::print(const char *buffer)
{
    write((const uint8_t *)buffer, strlen(buffer));
}

void Esp8266::print(int val)
{
    if(status)
    {
        if(SendHeader(GetIntLen(val)))
        {
            pSerial->print(val);
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::print(double val, int digits)
{
    if(status)
    {
        if(SendHeader(GetIntLen((int)val) + digits + 1))
        {
            pSerial->print(val, digits);
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::println(const __FlashStringHelper *buffer)
{
    if(status)
    {
        if(SendHeader(strlen_P((const prog_char*)buffer)+2))
        {
            pSerial->println(buffer);
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::println(const char *buffer)
{
    if(status)
    {
        if(SendHeader(strlen(buffer)+2))
        {
            pSerial->println(buffer);
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::println()
{
    if(status)
    {
        if(SendHeader(2))
        {
            pSerial->println();
            ReadResponse(resp_SendOK, 2000);
        }
    }
}

void Esp8266::write(const uint8_t *buffer, size_t size)
{
    if(status)
    {
        if(size)
        {
            if(SendHeader(size))
            {
                pSerial->write(buffer, size);
                ReadResponse(resp_SendOK, 2000);
            }
        }
    }
}

bool Esp8266::SendHeader(int size)
{
    if(size)
    {
        pSerial->print(ESP_CMD_SEND);
        pSerial->println(size);
        if(ReadResponse(resp_BG, 6000))
            return true;
    }
    return false;
}

uint8_t Esp8266::GetIntLen(int val)
{
    uint8_t n = 1;

    if(val >9999)
        n = 5;
    else if(val >999)
        n = 4;
    else if(val >99)
        n = 3;
    else if(val >9)
        n = 2;

    return n;
    /*
    do
    {
        n++;
        val /= 10;
    }while(val);
    return n;*/
}

uint8_t Esp8266::GetRecLen()
{
    int len;
    uint8_t pos = bufPos-1;

    while(recBuf[pos-1] != ',')
        pos--;

    recBuf[bufPos] = 0;
    bufPos = 0;
    len = atoi(recBuf + pos);
    if(recBuf[5] != '0')
    {
        discard = len;
        return 0;
    }
    return len;
}
