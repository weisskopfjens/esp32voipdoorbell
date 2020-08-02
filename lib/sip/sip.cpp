/* ====================================================================

   Copyright (c) 2018 Juerge Liegner  All rights reserved.
   modifications by Jens Weißkopf

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

   3. Neither the name of the author(s) nor the names of any contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   ====================================================================*/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <MD5Builder.h>
#include "sip.h"

//#define DEBUGLOG
//WiFiUDP sipudp;

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// hardware and api independent Sip class
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

char set = 0;

Sip::Sip(char *pBuf, size_t lBuf)
{
  pbuf = pBuf;
  lbuf = lBuf;
  pDialNr = "";
  pDialDesc = "";
  audioport[0] = '\0';
}

bool Sip::Dial(const char *DialNr, const char *DialDesc, int MaxDialSec)
{
  if (iRingTime)
    return false;

  Serial.println("DEBUG| Dial "+(String)(DialNr));
  audioport[0] = '\0';
  iDialRetries = 0;
  pDialNr = DialNr;
  pDialDesc = DialDesc;
  Invite();
  iDialRetries++;
  iRingTime = Millis();
  iMaxTime = MaxDialSec * 1000;
  Serial.println("DEBUG| Set timeout to "+(String)(MaxDialSec));
  return true;
}

void Sip::Cancel(int cseq)
{
  if (caRead[0] == 0)
    return;
  pbuf[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "CANCEL", pDialNr, pSipIp);
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s",  cseq, "CANCEL");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}

void Sip::Bye(int cseq)
{
  audioport[0] = '\0';
  if (caRead[0] == 0)
    return;
  pbuf[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "BYE", pDialNr, pSipIp);
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s", cseq, "BYE");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}

void Sip::Ack(const char *p)
{
  char ca[32];
  bool b = ParseParameter(ca, (int)sizeof(ca), "To: <", p, '>');
  if (!b)
    return;

  pbuf[0] = 0;
  AddSipLine("ACK %s SIP/2.0", ca);
  AddCopySipLine(p, "Call-ID: ");
  int cseq = GrepInteger(p, "\nCSeq: ");
  AddSipLine("CSeq: %i ACK",  cseq);
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}

void Sip::Ok(const char *p)
{
  pbuf[0] = 0;
  AddSipLine("SIP/2.0 200 OK");
  AddCopySipLine(p, "Call-ID: ");
  AddCopySipLine(p, "CSeq: ");
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}

void Sip::Init(const char *SipIp, int SipPort, const char *MyIp, int MyPort, const char *SipUser, const char *SipPassWd)
{
  udp.begin(SipPort);
  caRead[0] = 0;
  pbuf[0] = 0;
  pSipIp = SipIp;
  iSipPort = SipPort;
  pSipUser = SipUser;
  pSipPassWd = SipPassWd;
  pMyIp = MyIp;
  iMyPort = MyPort;
  iAuthCnt = 0;
  iRingTime = 0;
  
  
}

void Sip::AddSipLine(const char* constFormat , ... )
{
  va_list arglist;
  va_start( arglist, constFormat);
  uint16_t l = (uint16_t)strlen(pbuf);
  char *p = pbuf + l;
  vsnprintf(p, lbuf - l, constFormat, arglist );
  va_end( arglist );
  l = (uint16_t)strlen(pbuf);
  if (l < (lbuf - 2))
  {
    pbuf[l] = '\r';
    pbuf[l + 1] = '\n';
    pbuf[l + 2] = 0;
  }
}

// call invite without or with the response from peer
void Sip::Invite(const char *p)
{
  // prevent loops
  if (p && iAuthCnt > 3)
    return;

  // using caRead for temp. store realm and nonce
  char *caRealm = caRead;
  char *caNonce = caRead + 128;

  char *haResp = 0;
  int   cseq = 1;
  if (!p)
  {
    iAuthCnt = 0;
    if (iDialRetries == 0)
    {
      callid = Random();
      tagid = Random();
      branchid = Random();
    }
  }
  else
  {
    cseq = 2;
    if (   ParseParameter(caRealm, 128, " realm=\"", p)
           && ParseParameter(caNonce, 128, " nonce=\"", p))
    {
      // using output buffer to build the md5 hashes
      // store the md5 haResp to end of buffer
      char *ha1Hex = pbuf;
      char *ha2Hex = pbuf + 33;
      haResp = pbuf + lbuf - 34;
      char *pTemp = pbuf + 66;

      snprintf(pTemp, lbuf - 100, "%s:%s:%s", pSipUser, caRealm, pSipPassWd);
      MakeMd5Digest(ha1Hex, pTemp);

      snprintf(pTemp, lbuf - 100, "INVITE:sip:%s@%s", pDialNr, pSipIp);
      MakeMd5Digest(ha2Hex, pTemp);

      snprintf(pTemp, lbuf - 100, "%s:%s:%s", ha1Hex, caNonce, ha2Hex);
      MakeMd5Digest(haResp, pTemp);
    }
    else
    {
      caRead[0] = 0;
      return;
    }
  }
  pbuf[0] = 0;
  AddSipLine("INVITE sip:%s@%s SIP/2.0", pDialNr, pSipIp);
  AddSipLine("Call-ID: %010u@%s",  callid, pMyIp);
  AddSipLine("CSeq: %i INVITE",  cseq);
  AddSipLine("Max-Forwards: 70");
  // not needed for fritzbox
  AddSipLine("User-Agent: sipdial by jw");
  AddSipLine("From: \"%s\"  <sip:%s@%s>;tag=%010u", pDialDesc, pSipUser, pSipIp, tagid);
  AddSipLine("Via: SIP/2.0/UDP %s:%i;branch=%010u;rport=%i", pMyIp, iMyPort, branchid, iMyPort);
  AddSipLine("To: <sip:%s@%s>", pDialNr, pSipIp);
  AddSipLine("Contact: \"%s\" <sip:%s@%s:%i;transport=udp>", pSipUser, pSipUser, pMyIp, iMyPort);
  if (p)
  {
    // authentication
    AddSipLine("Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"sip:%s@%s\", response=\"%s\"", pSipUser, caRealm, caNonce, pDialNr, pSipIp, haResp);
    iAuthCnt++;
  }
  AddSipLine("Content-Type: application/sdp");
  // not needed for fritzbox
  AddSipLine("Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
  AddSipLine("Content-Length: %d",95+strlen(pMyIp)+strlen(pMyIp));
  AddSipLine("");
  AddSipLine("v=0");
  AddSipLine("o=- 0 4 IN IP4 %s",pMyIp);
  AddSipLine("s=sipcall");
  AddSipLine("c=IN IP4 %s",pMyIp);
  AddSipLine("t=0 0");
  AddSipLine("m=audio 1234 RTP/AVP 8");
  AddSipLine("a=rtpmap:8 PCMA/8000");
  caRead[0] = 0;//m=audio 7078 RTP/AVP 8
  SendUdp();
/*
v=0
o=- 0 4 IN IP4 
s=sipcall
c=IN IP4 
t=0 0
m=audio 1234 RTP/AVP 0
a=rtpmap:0 PCMA/8000
*/
}

// parse parameter value from http formated string
bool Sip::ParseParameter(char *dest, int destlen, const char *name, const char *line, char cq)
{
  const char *qp;
  const char *r;
  if ((r = strstr(line, name)) != NULL)
  {
    r = r + strlen(name);
    qp = strchr(r, cq);
    int l = qp - r;
    if (l < destlen)
    {
      strncpy(dest, r, l);
      dest[l] = 0;
      return true;
    }
  }
  return false;
}

// search a line in response date (p) and append on
// pbuf
bool Sip::AddCopySipLine(const char *p, const char *psearch)
{
  char *pa = strstr((char*)p, psearch);
  if (pa)
  {
    char *pe = strstr(pa, "\r");
    if (pe == 0)
      pe = strstr(pa, "\n");
    if (pe > pa)
    {
      char c = *pe;
      *pe = 0;
      AddSipLine("%s", pa);
      *pe = c;
      return true;
    }
  }
  return false;
}

int Sip::GrepInteger(const char *p, const char *psearch)
{
  int param = -1;
  const char *pc = strstr(p, psearch);
  if (pc)
  {
    param = atoi(pc + strlen(psearch));
  }
  return param;
}

// copy Call-ID, From, Via and To from response
// to caRead
// using later for BYE or CANCEL the call
bool Sip::ParseReturnParams(const char *p)
{
  pbuf[0] = 0;
  AddCopySipLine(p, "Call-ID: ");
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  if (strlen(pbuf) >= 2)
  {
    strcpy(caRead, pbuf);
    caRead[strlen(caRead) - 2] = 0;
  }
  return true;
}

void Sip::HandleUdpPacket()
{
  char *p;
  char caSipIn[2048];
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    caSipIn[0] = 0;
    packetSize = udp.read(caSipIn, sizeof(caSipIn));
    if (packetSize > 0) {
      caSipIn[packetSize] = 0;
    }
  }
  if (packetSize > 0) {
    p = caSipIn;
  } else {
    p = 0;
  }

  uint32_t iWorkTime = iRingTime ? (Millis() - iRingTime) : 0;
  if (iRingTime && iWorkTime > iMaxTime)
  {
    // Cancel(3);
    Bye(3);
    iRingTime = 0;
  }

  if (!p)
  {
    // max 5 dial retry when loos first invite packet
    if (iAuthCnt == 0 && iDialRetries < 5 && iWorkTime > (iDialRetries * 200))
    {
      iDialRetries++;
      delay(30);
      Invite();
    }
    return;
  }

  if (strstr(p, "SIP/2.0 401 Unauthorized") == p)
  {
    Serial.println("DEBUG| SIP/2.0 401 Unauthorized received");
    Ack(p);
    // call Invite with response data (p) to build auth md5 hashes
    Invite(p);
  }
  else if (strstr(p, "BYE") == p)
  {
    Serial.println("DEBUG| SIP/BYE received");
    audioport[0] = '\0';
    Ok(p);
    iRingTime = 0;
  }
  else if (strstr(p, "SIP/2.0 200") == p)    // OK
  {   
    Serial.println("DEBUG| SIP/2.0 200 OK received");
    iMaxTime = 60 * 1000; // Timeout to 60 sec
    Serial.println("DEBUG| Set timeout to 60 sec");
    ParseReturnParams(p);
    Ack(p);
  }
  else if (   strstr(p, "SIP/2.0 183 ") == p // Session Progress
              || strstr(p, "SIP/2.0 180 ") == p ) // Ringing
  {
    //
    // Determine the audio port of the SIP server (Fritzbox RTP port)
    //
    // for example:
    // m=audio 7078 RTP/AVP 8
    //
    // Todo: sscanf maybe a better method to do this
    //
    Serial.println("DEBUG| SIP/2.0 183 or 180 received");
    char *sdpportptr;
    sdpportptr = strstr(p, " RTP/AVP 8");
    if(sdpportptr == NULL) {
      Serial.println("DEBUG| RTP/AVP 8 not found");
      audioport[0] = '\0';
      return;
    } else {
       Serial.println("DEBUG| RTP/AVP 8 found");
    }
    sdpportptr--;
    int i=0;
    while(*sdpportptr!=' ' || i>8) {
      i++;
      sdpportptr--;
    }
    sdpportptr++;
    if(i<7) {
      i=0;
      while(*sdpportptr!=' ') {
        audioport[i]=*sdpportptr;
        sdpportptr++;
        i++;
      }
      audioport[i]='\0';
      Serial.println("DEBUG| Audio Port:"+(String)(audioport));
    }
    ParseReturnParams(p);
  }
  else if (strstr(p, "SIP/2.0 100 ") == p)   // Trying
  {
    ParseReturnParams(p);
    Ack(p);
  }
  else if (   strstr(p, "SIP/2.0 486 ") == p // Busy Here
              || strstr(p, "SIP/2.0 603 ") == p // Decline
              || strstr(p, "SIP/2.0 487 ") == p) // Request Terminatet
  {
    Ack(p);
    iRingTime = 0;
  }
  else if (strstr(p, "INFO") == p)
  {
    iLastCSeq = GrepInteger(p, "\nCSeq: ");
    Ok(p);
  }
}

const char * Sip::GetSIPServerIP(void) {
  return pSipIp;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// hardware dependent interface functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

int Sip::SendUdp(void)
{
  udp.beginPacket(pSipIp, iSipPort);
  udp.write((uint8_t *)pbuf, strlen(pbuf));
  udp.endPacket();
  return 0;
}

// generate a 30 bit random number
uint32_t Sip::Random()
{
  return esp_random();
}

uint32_t Sip::Millis()
{
  return (uint32_t)millis() + 1;
}

void Sip::MakeMd5Digest(char *pOutHex33, char *pIn)
{
  MD5Builder aMd5;
  aMd5.begin();
  aMd5.add(pIn);
  aMd5.calculate();
  aMd5.getChars(pOutHex33);
}

