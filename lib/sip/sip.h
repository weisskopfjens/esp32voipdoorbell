#ifndef SIP_h
#define SIP_h

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

class Sip
{
    WiFiUDP     udp;
    char       *pbuf;
    size_t      lbuf;
    char        caRead[256];


    const char *pSipIp;
    int         iSipPort;
    const char *pSipUser;
    const char *pSipPassWd;
    const char *pMyIp;
    int         iMyPort;
    const char *pDialNr;
    const char *pDialDesc;

    uint32_t    callid;
    uint32_t    tagid;
    uint32_t    branchid;

    int         iAuthCnt;
    uint32_t    iRingTime;
    uint32_t    iMaxTime;
    int         iDialRetries;
    int         iLastCSeq;
    void        AddSipLine(const char* constFormat , ... );
    bool        AddCopySipLine(const char *p, const char *psearch);   
    bool        ParseParameter(char *dest, int destlen, const char *name, const char *line, char cq = '\"');
    bool        ParseReturnParams(const char *p);
    int         GrepInteger(const char *p, const char *psearch);
    void        Ack(const char *pIn);
    void        Cancel(int seqn);
    void        Bye(int cseq);
    void        Ok(const char *pIn);
    void        Invite(const char *pIn = 0);

    uint32_t    Millis();
    uint32_t    Random();
    int         SendUdp();
    void        MakeMd5Digest(char *pOutHex33, char *pIn);

  public:
    Sip(char *pBuf, size_t lBuf);
    void        Init(const char *SipIp, int SipPort, const char *MyIp, int MyPort, const char *SipUser, const char *SipPassWd);
    void        HandleUdpPacket();
    bool        Dial(const char *DialNr, const char *DialDesc = "", int MaxDialSec = 10);
    bool        IsBusy() {
      return iRingTime != 0;
    }
    const char* GetSIPServerIP(void);
    char        audioport[7];
    
};
#endif