/*
  microBoxEsp.cpp - Library for Linux-Shell like interface
                    for Arduino with esp8266 support.
  Created by Sebastian Duell, 06.02.2015.
  More info under http://sebastian-duell.de
  Released under GPLv3.
*/

#include <microBoxEsp.h>
#include <esp8266.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

microBoxEsp microbox;
const prog_char fileDate[] PROGMEM = __DATE__;

CMD_ENTRY microBoxEsp::Cmds[] =
{
    {"cat", microBoxEsp::CatCB},
    {"cd", microBoxEsp::ChangeDirCB},
    {"echo", microBoxEsp::EchoCB},
    {"exit", microBoxEsp::ExitCB},
    {"loadpar", microBoxEsp::LoadParCB},
    {"ll", microBoxEsp::ListLongCB},
    {"ls", microBoxEsp::ListDirCB},
    {"savepar", microBoxEsp::SaveParCB},
    {"watch", microBoxEsp::watchCB},
    {"watchcsv", microBoxEsp::watchcsvCB},
    {NULL, NULL}
};

const char microBoxEsp::dirList[][5] PROGMEM =
{
    "bin", "dev", "etc", "proc", "sbin", "var", "lib", "sys", "tmp", "usr", ""
};

microBoxEsp::microBoxEsp()
{
    bufPos = 0;
    watchMode = false;
    csvMode = false;
    loginState = STATE_LOGIN_DISCONNECTED;
    blockRead = 0;
    watchTimeout = 0;
    escSeq = 0;
    historyWrPos = 0;
    historyBufSize = 0;
    serAvail = 0;
    historyCursorPos = -1;
}

microBoxEsp::~microBoxEsp()
{
}

void microBoxEsp::begin(PARAM_ENTRY *pParams, const char *hostName, const char *loginPassword, char *histBuf, int historySize, HardwareSerial *serial)
{
    esp8266.begin(serial);
    pSerial = serial;
    historyBuf = histBuf;
    if(historyBuf != NULL && historySize != 0)
    {
        historyBufSize = historySize;
        historyBuf[0] = 0;
        historyBuf[1] = 0;
    }

    Params = pParams;
    machName = hostName;
    password = loginPassword;
    ParmPtr[0] = NULL;
    currentDir[0] = '/';
    currentDir[1] = 0;
}

bool microBoxEsp::AddCommand(const char *cmdName, void (*cmdFunc)(char **param, uint8_t parCnt))
{
    uint8_t idx = 0;

    while((Cmds[idx].cmdFunc != NULL) && (idx < (MAX_CMD_NUM-1)))
    {
        idx++;
    }
    if(idx < (MAX_CMD_NUM-1))
    {
        Cmds[idx].cmdName = cmdName;
        Cmds[idx].cmdFunc = cmdFunc;
        idx++;
        Cmds[idx].cmdFunc = NULL;
        Cmds[idx].cmdName = NULL;
        return true;
    }
    return false;
}

bool microBoxEsp::isTimeout(unsigned long *lastTime, unsigned long intervall)
{
    unsigned long m;

    m = millis();
    if(((m - *lastTime) >= intervall) || (*lastTime > m))
    {
        *lastTime = m;
        return true;
    }
    return false;
}

void microBoxEsp::ShowPrompt()
{
    strcpy_P(cmdBuf, PSTR("root@"));
    strcat(cmdBuf, machName);
    strcat_P(cmdBuf, PSTR(":"));
    strcat(cmdBuf, currentDir);
    strcat_P(cmdBuf, PSTR(">"));

    esp8266.print(cmdBuf);

    cmdBuf[0] = 0;
}

uint8_t microBoxEsp::ParseCmdParams(char *pParam)
{
    uint8_t idx = 0;

    ParmPtr[idx] = pParam;
    if(pParam != NULL)
    {
        idx++;
        while((pParam = strchr(pParam, ' ')) != NULL)
        {
            pParam[0] = 0;
            pParam++;
            ParmPtr[idx++] = pParam;
        }
    }
    return idx;
}

void microBoxEsp::ExecCommand()
{
    bool found = false;
    esp8266.println();
    if(bufPos > 0)
    {
        uint8_t i=0;
        uint8_t dstlen;
        uint8_t srclen;
        char *pParam;

        cmdBuf[bufPos] = 0;
        pParam = strchr(cmdBuf, ' ');
        if(pParam != NULL)
        {
            pParam++;
            srclen = pParam - cmdBuf - 1;
        }
        else
            srclen = bufPos;

        AddToHistory(cmdBuf);
        historyCursorPos = -1;

        while(Cmds[i].cmdName != NULL && found == false)
        {
            dstlen = strlen(Cmds[i].cmdName);
            if(dstlen == srclen)
            {
                if(strncmp(cmdBuf, Cmds[i].cmdName, dstlen) == 0)
                {
                    (*Cmds[i].cmdFunc)(ParmPtr, ParseCmdParams(pParam));
                    found = true;
                    ShowPrompt();
                }
            }
            i++;
        }
        if(!found)
        {
            ErrorDir(F("/bin/sh"));
            ShowPrompt();
        }
    }
    else
        ShowPrompt();
}

void microBoxEsp::BlockreadSend()
{
    if(blockRead)
    {
        if(blockRead == 0xff)
            blockRead = 0;
        if(bufPos && (bufPos-blockRead) && (bufPos>blockRead) && (loginState == STATE_LOGIN_LOGGEDIN || loginState == STATE_LOGIN_USERNAME))
            esp8266.write((uint8_t*)cmdBuf+blockRead, bufPos-blockRead);
        blockRead = 0;
    }
}

void microBoxEsp::cmdParser()
{
    uint8_t conState;

    conState = esp8266.GetStatus();
    if(conState == STATUS_ESP_CONNECTED && loginState == STATE_LOGIN_DISCONNECTED)
    {
        loginState = STATE_LOGIN_USERNAME;
        esp8266.print(F("\xff\xfd\x01\xff\xfb\x01\xff\xfb\x03")); // Send telnet Do Echo, Suppress SGa, Will Echo
        delay(1000);
        strcpy(cmdBuf, machName);
        strcat_P(cmdBuf, PSTR(" login: "));
        esp8266.print(cmdBuf);
        esp8266.clearBuffer();
    }
    else if(conState == STATUS_ESP_DISCONNECTED && loginState != STATE_LOGIN_DISCONNECTED)
    {
        loginState = STATE_LOGIN_DISCONNECTED;
        esp8266.clearBuffer();
        serAvail = 0;
        bufPos = 0;
        currentDir[0] = '/';
        currentDir[1] = 0;
    }

    if(serAvail == 0)
    {
        BlockreadSend();
        serAvail = esp8266.ReadResponse();
        if(serAvail > 1)
        {
            if(bufPos)
                blockRead = bufPos;
            else
                blockRead = 0xff;
        }
        else
            blockRead = 0;
    }

    if(watchMode)
    {
        if(serAvail > 0)
        {
            watchMode = false;
            csvMode = false;
        }
        else
        {
            if(isTimeout(&watchTimeout, 500))
                Cat_int(cmdBuf);

            return;
        }
    }
    while(serAvail > 0 && esp8266.SerialAvailable())
    {
        unsigned char ch;
        serAvail--;
        ch = esp8266.read();

        if(loginState == STATE_LOGIN_LOGGEDIN)
            if(HandleEscSeq(ch))
                continue;

        if(ch == 0x7F || ch == 0x08)
        {
            if(bufPos > 0)
            {
                esp8266.clearBuffer(serAvail);
                serAvail = 0;
                BlockreadSend();
                esp8266.SendHeader(6);
                pSerial->write(ch);
                pSerial->print(F(" \x1B[1D"));
                esp8266.WaitForSendComplete();
                bufPos--;
                cmdBuf[bufPos] = 0;
            }
        }
        else if(ch == '\t' && !blockRead && loginState == STATE_LOGIN_LOGGEDIN)
        {
            HandleTab();
        }
        else if(ch != '\r')
        {
            if(bufPos < (MAX_CMD_BUF_SIZE-1))
            {
                if(ch != '\n')
                {
                    if(!blockRead && (loginState == STATE_LOGIN_LOGGEDIN || loginState == STATE_LOGIN_USERNAME))
                        esp8266.write((uint8_t*)&ch, 1);
                    cmdBuf[bufPos++] = ch;
                    cmdBuf[bufPos] = 0;
                }
            }
            if(ch == '\n')
            {
                BlockreadSend();
                if(loginState == STATE_LOGIN_LOGGEDIN)
                    ExecCommand();
                else
                    HandleLogin();
                bufPos = 0;
                //			 cmdBuf[bufPos] = 0;
            }
        }
    }
}

void microBoxEsp::PasswordPrompt()
{
    esp8266.println();
    esp8266.print(F("Password:"));
}

void microBoxEsp::HandleLogin()
{
    if(loginState == STATE_LOGIN_USERNAME)
    {
        if(strcmp_P(cmdBuf, PSTR("root")) == 0)
        {
            loginState++;
        }
        else
        {
            loginState = STATE_LOGIN_WRONG_USER_PASSWORD1;
        }

        PasswordPrompt();
    }
    else
    {
        if(loginState < STATE_LOGIN_LOGGEDIN && strcmp(cmdBuf, password) == 0)
        {
            loginState = STATE_LOGIN_LOGGEDIN;
            esp8266.println();
            ShowPrompt();
        }
        else
        {
            loginState++;
            if(loginState != STATE_LOGIN_LOGGEDIN && loginState < STATE_LOGIN_USER_ERROR)
                PasswordPrompt();
            else
                Exit();
        }
    }
}

bool microBoxEsp::HandleEscSeq(unsigned char ch)
{
    bool ret = false;

    if(ch == 27)
    {
        escSeq = ESC_STATE_START;
        ret = true;
    }
    else if(escSeq == ESC_STATE_START)
    {
        if(ch == 0x5B)
        {
            escSeq = ESC_STATE_CODE;
            ret = true;
        }
        else
            escSeq = ESC_STATE_NONE;
    }
    else if(escSeq == ESC_STATE_CODE)
    {
        if(ch == 0x41) // Cursor Up
        {
            HistoryUp();
        }
        else if(ch == 0x42) // Cursor Down
        {
            HistoryDown();
        }
        else if(ch == 0x43) // Cursor Right
        {
        }
        else if(ch == 0x44) // Cursor Left
        {
        }
        escSeq = ESC_STATE_NONE;
        blockRead = 0;
        ret = true;
    }
    return ret;
}


uint8_t microBoxEsp::ParCmp(uint8_t idx1, uint8_t idx2, bool cmd)
{
    uint8_t i=0;

    const char *pName1;
    const char *pName2;

    if(cmd)
    {
        pName1 = Cmds[idx1].cmdName;
        pName2 = Cmds[idx2].cmdName;
    }
    else
    {
        pName1 = Params[idx1].paramName;
        pName2 = Params[idx2].paramName;
    }

    while(pName1[i] != 0 && pName2[i] != 0)
    {
        if(pName1[i] != pName2[i])
            return i;
        i++;
    }
    return i;
}

int8_t microBoxEsp::GetCmdIdx(char* pCmd, int8_t startIdx)
{
    while(Cmds[startIdx].cmdName != NULL)
    {
        if(strncmp(Cmds[startIdx].cmdName, pCmd, strlen(pCmd)) == 0)
        {
            return startIdx;
        }
        startIdx++;
    }
    return -1;
}

void microBoxEsp::HandleTab()
{
    int8_t idx, idx2;
    char *pParam = NULL;
    uint8_t i, len = 0;
    uint8_t parlen, matchlen, inlen;

    for(i=0;i<bufPos;i++)
    {
        if(cmdBuf[i] == ' ')
            pParam = cmdBuf+i;
    }
    if(pParam != NULL)
    {
        pParam++;
        if(*pParam != 0)
        {
            idx = GetParamIdx(pParam, true, 0);
            if(idx >= 0)
            {
                parlen = strlen(Params[idx].paramName);
                matchlen = parlen;
                idx2=idx;
                while((idx2=GetParamIdx(pParam, true, idx2+1))!= -1)
                {
                    matchlen = ParCmp(idx, idx2);
                    if(matchlen < parlen)
                        parlen = matchlen;
                }
                pParam = GetFile(pParam);
                inlen = strlen(pParam);
                if(matchlen > inlen)
                {
                    len = matchlen - inlen;
                    if((bufPos + len) < MAX_CMD_BUF_SIZE)
                    {
                        strncat(cmdBuf, Params[idx].paramName + inlen, len);
                        bufPos += len;
                    }
                    else
                        len = 0;
                }
            }
        }
    }
    else if(bufPos)
    {
        pParam = cmdBuf;

        idx = GetCmdIdx(pParam);
        if(idx >= 0)
        {
            parlen = strlen(Cmds[idx].cmdName);
            matchlen = parlen;
            idx2=idx;
            while((idx2=GetCmdIdx(pParam, idx2+1))!= -1)
            {
                matchlen = ParCmp(idx, idx2, true);
                if(matchlen < parlen)
                    parlen = matchlen;
            }
            inlen = strlen(pParam);
            if(matchlen > inlen)
            {
                len = matchlen - inlen;
                if((bufPos + len) < MAX_CMD_BUF_SIZE)
                {
                    strncat(cmdBuf, Cmds[idx].cmdName + inlen, len);
                    bufPos += len;
                }
                else
                    len = 0;
            }
        }
    }
    if(len > 0)
    {
        esp8266.print(pParam + inlen);
    }
}

void microBoxEsp::HistoryUp()
{
    if(historyBufSize == 0 || historyWrPos == 0)
        return;

    if(historyCursorPos == -1)
        historyCursorPos = historyWrPos-2;

    while(historyBuf[historyCursorPos] != 0 && historyCursorPos > 0)
    {
        historyCursorPos--;
    }
    if(historyCursorPos > 0)
        historyCursorPos++;

    strcpy(cmdBuf, historyBuf+historyCursorPos);
    HistoryPrintHlpr();
    if(historyCursorPos > 1)
        historyCursorPos -= 2;
}

void microBoxEsp::HistoryDown()
{
    int pos;
    if(historyCursorPos != -1 && historyCursorPos != historyWrPos-2)
    {
        pos = historyCursorPos+2;
        pos += strlen(historyBuf+pos) + 1;

        strcpy(cmdBuf, historyBuf+pos);
        HistoryPrintHlpr();
        historyCursorPos = pos - 2;
    }
}

void microBoxEsp::HistoryPrintHlpr()
{
    uint8_t i;
    uint8_t len;

    len = strlen(cmdBuf);

    i = len + bufPos;
    if(len<bufPos)
        i += 3;
    if(i>0)
    {
        esp8266.SendHeader(i);
        for(i=0;i<bufPos;i++)
            pSerial->print(F("\b"));
        pSerial->print(cmdBuf);
        if(len<bufPos)
        {
            pSerial->print(F("\x1B[K"));
        }
        esp8266.WaitForSendComplete();
    }
    bufPos = len;
}

void microBoxEsp::AddToHistory(char *buf)
{
    uint8_t len;
    int blockStart = 0;

    len = strlen(buf);
    if(historyBufSize > 0)
    {
        if(historyWrPos+len+1 >= historyBufSize)
        {
            while(historyWrPos+len-blockStart >= historyBufSize)
            {
                blockStart += strlen(historyBuf + blockStart) + 1;
            }
            memmove(historyBuf, historyBuf+blockStart, historyWrPos-blockStart);
            historyWrPos -= blockStart;
        }
        strcpy(historyBuf+historyWrPos, buf);
        historyWrPos += len+1;
        historyBuf[historyWrPos] = 0;
    }
}

void microBoxEsp::ErrorDir(const __FlashStringHelper *cmd)
{
    esp8266.print(cmd);
    esp8266.println(F(": File or directory not found\n"));
}

char *microBoxEsp::GetDir(char *pParam, bool useFile)
{
    uint8_t i=0;
    uint8_t len;
    char *tmp;

    dirBuf[0] = 0;
    if(pParam != NULL)
    {
        if(currentDir[1] != 0)
        {
            if(pParam[0] != '/')
            {
                if(!(pParam[0] == '.' && pParam[1] == '.'))
                {
                    return NULL;
                }
                else
                {
                    pParam += 2;
                    if(pParam[0] == 0)
                    {
                        dirBuf[0] = '/';
                        dirBuf[1] = 0;
                    }
                    else if(pParam[0] != '/')
                        return NULL;
                }
            }
        }
        if(pParam[0] == '/')
        {
            if(pParam[1] == 0)
            {
                dirBuf[0] = '/';
                dirBuf[1] = 0;
            }
            pParam++;
        }

        if((tmp=strchr(pParam, '/')) != 0)
        {
            len = tmp-pParam;
        }
        else
            len = strlen(pParam);
        if(len > 0)
        {
            while(pgm_read_byte_near(&dirList[i][0]) != 0)
            {
                if(strncmp_P(pParam, dirList[i], len) == 0)
                {
                    if(strlen_P(dirList[i]) == len)
                    {
                        dirBuf[0] = '/';
                        dirBuf[1] = 0;
                        strcat_P(dirBuf, dirList[i]);
                        return dirBuf;
                    }
                }
                i++;
            }
        }
    }
    if(dirBuf[0] != 0)
        return dirBuf;
    return NULL;
}

char *microBoxEsp::GetFile(char *pParam)
{
    char *file;
    char *t;

    file = pParam;
    while((t=strchr(file, '/')) != NULL)
    {
        file = t+1;
    }
    return file;
}

void microBoxEsp::ListDirHlp(bool dir, const char *name, bool listLong, bool rw, uint16_t len)
{
    uint8_t sendlen = 0;

    cmdBuf[1] = 'r';
    cmdBuf[3] = 0;
    if(dir)
        cmdBuf[0] = 'd';
    else
        cmdBuf[0] = '-';

    if(rw)
        cmdBuf[2] = 'w';
    else
        cmdBuf[2] = '-';

    strcat_P(cmdBuf, PSTR("xr-xr-x\t2 root\troot\t"));
    if(listLong)
        sendlen = strlen(cmdBuf) + esp8266.GetIntLen(len) + strlen_P(fileDate) + 2;
    if(name != NULL)
        sendlen += strlen(name)+2;
    esp8266.SendHeader(sendlen);
    if(listLong)
    {
        pSerial->print(cmdBuf);

        pSerial->print((uint16_t)len);
        pSerial->print(F(" "));
        pSerial->print((const __FlashStringHelper*)fileDate);
        pSerial->print(F(" "));
    }
    if(name != NULL)
        pSerial->println(name);

    esp8266.WaitForSendComplete();
    cmdBuf[0] = 0;
}

void microBoxEsp::ListDir(char **pParam, uint8_t parCnt, bool listLong)
{
    uint8_t i=0;
    char *dir;

    if(parCnt != 0)
    {
        dir = GetDir(pParam[0], false);
        if(dir == NULL)
        {
            if(listLong)
                ErrorDir(F("ll"));
            else
                ErrorDir(F("ls"));
            return;
        }
    }
    else
    {
        dir = currentDir;
    }

    if(dir[1] == 0)
    {
        cmdBuf[0] = 0;
        while(pgm_read_byte_near(&dirList[i][0]) != 0)
        {
            if(listLong)
            {
                ListDirHlp(true);
                esp8266.println((__FlashStringHelper*)dirList[i]);
            }
            else
            {
                strcat_P(cmdBuf, dirList[i]);
                strcat_P(cmdBuf, PSTR("\t"));
            }
            i++;
        }
        esp8266.println(cmdBuf);
    }
    else if(strcmp_P(dir, PSTR("/bin")) == 0)
    {
        while(Cmds[i].cmdName != NULL)
        {
            ListDirHlp(false, Cmds[i].cmdName, listLong);
            i++;
        }
    }
    else if(strcmp_P(dir, PSTR("/dev")) == 0)
    {
        while(Params[i].paramName != NULL)
        {
            uint8_t size;
            if(Params[i].parType&PARTYPE_INT)
                size = sizeof(int);
            else if(Params[i].parType&PARTYPE_DOUBLE)
                size = sizeof(double);
            else
                size = Params[i].len;

            ListDirHlp(false, Params[i].paramName, listLong, Params[i].parType&PARTYPE_RW, size);
            i++;
        }
    }
}

void microBoxEsp::ChangeDir(char **pParam, uint8_t parCnt)
{
    char *dir;

    if(pParam[0] != NULL)
    {
        dir = GetDir(pParam[0], false);
        if(dir != NULL)
        {
            strcpy(currentDir, dir);
            return;
        }
    }
    ErrorDir(F("cd"));
}

void microBoxEsp::PrintParam(uint8_t idx)
{
    if(Params[idx].getFunc != NULL)
        (*Params[idx].getFunc)(Params[idx].id);

    if(Params[idx].parType&PARTYPE_INT)
        esp8266.print(*((int*)Params[idx].pParam));
    else if(Params[idx].parType&PARTYPE_DOUBLE)
        esp8266.print(*((double*)Params[idx].pParam), 8);
    else
        esp8266.print(((char*)Params[idx].pParam));

    if(csvMode)
    {
        esp8266.print(';');
    }
    else
        esp8266.println();
}

int8_t microBoxEsp::GetParamIdx(char *pParam, bool partStr, int8_t startIdx)
{
    int8_t i=startIdx;
    char *dir;
    char *file;

    if(pParam != NULL)
    {
        dir = GetDir(pParam, true);
        if(dir == NULL)
            dir = currentDir;
        if(dir != NULL)
        {
            if(strcmp_P(dir, PSTR("/dev")) == 0)
            {
                file = GetFile(pParam);
                if(file != NULL)
                {
                    while(Params[i].paramName != NULL)
                    {
                        if(partStr)
                        {
                            if(strncmp(Params[i].paramName, file, strlen(file))== 0)
                            {
                                return i;
                            }
                        }
                        else
                        {
                            if(strcmp(Params[i].paramName, file)== 0)
                            {
                                return i;
                            }
                        }
                        i++;
                    }
                }
            }
        }
    }
    return -1;
}

// Taken from Stream.cpp
double microBoxEsp::parseFloat(char *pBuf)
{
    boolean isNegative = false;
    boolean isFraction = false;
    long value = 0;
    unsigned char c;
    double fraction = 1.0;
    uint8_t idx = 0;

    c = pBuf[idx++];
    // ignore non numeric leading characters
    if(c > 127)
        return 0; // zero returned if timeout

    do{
        if(c == '-')
            isNegative = true;
        else if (c == '.')
            isFraction = true;
        else if(c >= '0' && c <= '9')  {      // is c a digit?
            value = value * 10 + c - '0';
            if(isFraction)
                fraction *= 0.1;
        }
        c = pBuf[idx++];
    }
    while( (c >= '0' && c <= '9')  || c == '.');

    if(isNegative)
        value = -value;
    if(isFraction)
        return value * fraction;
    else
        return value;
}

// echo 82.00 > /dev/param
void microBoxEsp::Echo(char **pParam, uint8_t parCnt)
{
    uint8_t idx;

    if((parCnt == 3) && (strcmp_P(pParam[1], PSTR(">")) == 0))
    {
        idx = GetParamIdx(pParam[2]);
        if(idx != -1)
        {
            if(Params[idx].parType & PARTYPE_RW)
            {
                if(Params[idx].parType & PARTYPE_INT)
                {
                    int val;

                    val = atoi(pParam[0]);
                    *((int*)Params[idx].pParam) = val;
                }
                else if(Params[idx].parType & PARTYPE_DOUBLE)
                {
                    double val;

                    val = parseFloat(pParam[0]);
                    *((double*)Params[idx].pParam) = val;
                }
                else
                {
                    if(strlen(pParam[0]) < Params[idx].len)
                        strcpy((char*)Params[idx].pParam, pParam[0]);
                }
                if(Params[idx].setFunc != NULL)
                    (*Params[idx].setFunc)(Params[idx].id);
            }
            else
                esp8266.println(F("echo: File readonly"));
        }
        else
        {
            ErrorDir(F("echo"));
        }
    }
    else
    {
        for(idx=0;idx<parCnt;idx++)
        {
            esp8266.print(pParam[idx]);
            esp8266.print(F(" "));
        }
        esp8266.println();
    }
}

void microBoxEsp::Cat(char **pParam, uint8_t parCnt)
{
    Cat_int(pParam[0]);
}

uint8_t microBoxEsp::Cat_int(char *pParam)
{
    int8_t idx;

    idx = GetParamIdx(pParam);
    if(idx != -1)
    {
        PrintParam(idx);
        return 1;
    }
    else
        ErrorDir(F("cat"));
    
    return 0;
}

void microBoxEsp::watch(char **pParam, uint8_t parCnt)
{
    if(parCnt == 2)
    {
        if(strncmp_P(pParam[0], PSTR("cat"), 3) == 0)
        {
            if(Cat_int(pParam[1]))
            {
                strcpy(cmdBuf, pParam[1]);
                watchMode = true;
            }
        }
    }
}

void microBoxEsp::watchcsv(char **pParam, uint8_t parCnt)
{
    watch(pParam, parCnt);
    if(watchMode)
        csvMode = true;
}

void microBoxEsp::Exit()
{
    esp8266.Disconnect(F("0"));
    loginState = STATE_LOGIN_DISCONNECTED;
}

void microBoxEsp::ReadWriteParamEE(bool write)
{
    uint8_t i=0;
    uint8_t psize;
    int pos=0;

    while(Params[i].paramName != NULL)
    {
        if(Params[i].parType&PARTYPE_INT)
            psize = sizeof(uint16_t);
        else if(Params[i].parType&PARTYPE_DOUBLE)
            psize = sizeof(double);
        else
            psize = Params[i].len;

        if(write)
            eeprom_write_block(Params[i].pParam, (void*)pos, psize);
        else
            eeprom_read_block(Params[i].pParam, (void*)pos, psize);
        pos += psize;
        i++;
    }
}

void microBoxEsp::ListDirCB(char **pParam, uint8_t parCnt)
{
    microbox.ListDir(pParam, parCnt);
}

void microBoxEsp::ListLongCB(char **pParam, uint8_t parCnt)
{
    microbox.ListDir(pParam, parCnt, true);
}

void microBoxEsp::ChangeDirCB(char **pParam, uint8_t parCnt)
{
    microbox.ChangeDir(pParam, parCnt);
}

void microBoxEsp::EchoCB(char **pParam, uint8_t parCnt)
{
    microbox.Echo(pParam, parCnt);
}

void microBoxEsp::ExitCB(char **pParam, uint8_t parCnt)
{
    microbox.Exit();
}

void microBoxEsp::CatCB(char **pParam, uint8_t parCnt)
{
    microbox.Cat(pParam, parCnt);
}

void microBoxEsp::watchCB(char **pParam, uint8_t parCnt)
{
    microbox.watch(pParam, parCnt);
}

void microBoxEsp::watchcsvCB(char **pParam, uint8_t parCnt)
{
    microbox.watchcsv(pParam, parCnt);
}

void microBoxEsp::LoadParCB(char **pParam, uint8_t parCnt)
{
    microbox.ReadWriteParamEE(false);
}

void microBoxEsp::SaveParCB(char **pParam, uint8_t parCnt)
{
    microbox.ReadWriteParamEE(true);
}
