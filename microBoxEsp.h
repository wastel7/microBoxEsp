/*
  microBoxEsp.h - Library for Linux-Shell like interface
                    for Arduino with esp8266 support.
  Created by Sebastian Duell, 06.02.2015.
  More info under http://sebastian-duell.de
  Released under GPLv3.
*/

#ifndef _BASHCMD_H_
#define _BASHCMD_H_

#include <Arduino.h>
#include <esp8266.h>

#define MAX_CMD_NUM 20

#define MAX_CMD_BUF_SIZE 40
#define MAX_PATH_LEN 10

#define PARTYPE_INT    0x01
#define PARTYPE_DOUBLE 0x02
#define PARTYPE_STRING 0x04
#define PARTYPE_RW     0x10
#define PARTYPE_RO     0x00

#define ESC_STATE_NONE 0
#define ESC_STATE_START 1
#define ESC_STATE_CODE 2

#define STATE_LOGIN_DISCONNECTED    0
#define STATE_LOGIN_CONNECTED       1
#define STATE_LOGIN_USERNAME        2
#define STATE_LOGIN_PASSWORD1       3
#define STATE_LOGIN_PASSWORD2       4
#define STATE_LOGIN_PASSWORD3       5
#define STATE_LOGIN_LOGGEDIN        6
#define STATE_LOGIN_WRONG_USER_PASSWORD1       7
#define STATE_LOGIN_WRONG_USER_PASSWORD2       8
#define STATE_LOGIN_WRONG_USER_PASSWORD3       9
#define STATE_LOGIN_USER_ERROR                 10

typedef struct
{
    const char *cmdName;
    void (*cmdFunc)(char **param, uint8_t parCnt);
}CMD_ENTRY;

typedef struct
{
    const char *paramName;
    void *pParam;
    uint8_t parType;
    uint8_t len;
    void (*setFunc)(uint8_t id);
    void (*getFunc)(uint8_t id);
    uint8_t id;
}PARAM_ENTRY;

class microBoxEsp
{
public:
    microBoxEsp();
    ~microBoxEsp();
    void begin(PARAM_ENTRY *pParams, const char *hostName, const char *loginPassword, char *histBuf = NULL, int historySize=0, HardwareSerial *serial=&Serial);
    void cmdParser();
    bool isTimeout(unsigned long *lastTime, unsigned long intervall);
    bool AddCommand(const char *cmdName, void (*cmdFunc)(char **param, uint8_t parCnt));

private:
    static void ListDirCB(char **pParam, uint8_t parCnt);
    static void ListLongCB(char **pParam, uint8_t parCnt);
    static void ChangeDirCB(char **pParam, uint8_t parCnt);
    static void EchoCB(char **pParam, uint8_t parCnt);
    static void ExitCB(char **pParam, uint8_t parCnt);
    static void CatCB(char **pParam, uint8_t parCnt);
    static void watchCB(char **pParam, uint8_t parCnt);
    static void watchcsvCB(char **pParam, uint8_t parCnt);
    static void LoadParCB(char **pParam, uint8_t parCnt);
    static void SaveParCB(char **pParam, uint8_t parCnt);

    void ListDir(char **pParam, uint8_t parCnt, bool listLong=false);
    void ChangeDir(char **pParam, uint8_t parCnt);
    void Echo(char **pParam, uint8_t parCnt);
    void Exit();
    void Cat(char **pParam, uint8_t parCnt);
    void watch(char **pParam, uint8_t parCnt);
    void watchcsv(char **pParam, uint8_t parCnt);

private:
    void ShowPrompt();
    uint8_t ParseCmdParams(char *pParam);
    void ErrorDir(const __FlashStringHelper *cmd);
    char *GetDir(char *pParam, bool useFile);
    char *GetFile(char *pParam);
    void PrintParam(uint8_t idx);
    int8_t GetParamIdx(char *pParam, bool partStr = false, int8_t startIdx = 0);
    int8_t GetCmdIdx(char* pCmd, int8_t startIdx = 0);
    uint8_t Cat_int(char *pParam);
    void ListDirHlp(bool dir, const char *name = NULL, bool listLong = true, bool rw = true, uint16_t len=4096);
    uint8_t ParCmp(uint8_t idx1, uint8_t idx2, bool cmd=false);
    void HandleTab();
    void HistoryUp();
    void HistoryDown();
    void HistoryPrintHlpr();
    void AddToHistory(char *buf);
    void ExecCommand();
    bool HandleEscSeq(unsigned char ch);
    double parseFloat(char *pBuf);
    void HandleLogin();
    void PasswordPrompt();
    void ReadWriteParamEE(bool write);
    void BlockreadSend();

private:
    char currentDir[MAX_PATH_LEN];

    char cmdBuf[MAX_CMD_BUF_SIZE];
    char dirBuf[15];
    char *ParmPtr[10];
    uint8_t bufPos;
    bool watchMode;
    bool csvMode;
    uint8_t escSeq;
    unsigned long watchTimeout;
    const char* machName;
    int historyBufSize;
    char *historyBuf;
    int historyWrPos;
    int historyCursorPos;
    bool loggedIn;
    HardwareSerial *pSerial;
    uint8_t serAvail;
    uint8_t blockRead;
    uint8_t loginState;
    const char *password;

    static CMD_ENTRY Cmds[MAX_CMD_NUM];
    PARAM_ENTRY *Params;
    static const char dirList[][5] PROGMEM;
};

extern microBoxEsp microbox;

#endif
