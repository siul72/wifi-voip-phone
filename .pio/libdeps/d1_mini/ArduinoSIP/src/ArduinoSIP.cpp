/* ====================================================================

   Copyright (c) 2018 Juergen Liegner  All rights reserved.
   (https://www.mikrocontroller.net/topic/444994)
   
   Copyright (c) 2019 Thorsten Godau (dl9sec)
   (Created an Arduino library from the original code and did some beautification)
   
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
#include <MD5Builder.h>

#include "ArduinoSIP.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware and API independent Sip class
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

Sip::Sip(char *pBuf, size_t lBuf) {

  message_buffer = pBuf;
  buffer_size = lBuf;
  pDialNr = "";
  pDialDesc = "";

}


Sip::~Sip() {
  
}


void Sip::Init(const char * SipServerAddress, int SipPort, const char *MyIp, int MyPort, const char *SipUser, const char *SipPassWd, int MaxDialSec) {
  
 
  
  caRead[0] = 0;
  message_buffer[0] = 0;
  sip_server_address = SipServerAddress;
 
  sip_server_port = SipPort;
  sip_user_name = SipUser;
  sip_user_password = SipPassWd;
  local_ip_address = MyIp;
  local_ip_port = MyPort;
  
  iAuthCnt = 0;
  iRingTime = 0;
  iMaxTime = MaxDialSec * 1000;
  register_status = false;
  Udp.begin(local_ip_port);
 Serial.printf("\r\n SIP Init %s:%i %s:%i user:%s ----\r\n", local_ip_address.c_str(), 
 local_ip_port, sip_server_address.c_str(), sip_server_port, sip_user_name.c_str());
 
}

bool Sip::isRegister(){

  return register_status;
}


bool Sip::Dial(const char *DialNr, const char *DialDesc) {
  
  if ( iRingTime )
    return false;

  iDialRetries = 0;
  pDialNr = DialNr;
  pDialDesc = DialDesc;
  Invite();
  iDialRetries++;
  iRingTime = Millis();

  return true;
}


void Sip::Processing(char *pBuf, size_t lBuf) {
	
  int packetSize = Udp.parsePacket();
  
  if ( packetSize > 0 )
  {
    pBuf[0] = 0;
    packetSize = Udp.read(pBuf, lBuf);
    if ( packetSize > 0 )
    {
      pBuf[packetSize] = 0;
    
 
      IPAddress remoteIp = Udp.remoteIP();
      Serial.printf("\r\n----- read %i bytes from: %s:%i ----\r\n", (int)packetSize, remoteIp.toString().c_str(), Udp.remotePort());
      Serial.print(pBuf);
      Serial.printf("----------------------------------------------------\r\n");
 
    
    }
  }
  
  HandleUdpPacket((packetSize > 0) ? pBuf : 0 );
}


void Sip::HandleUdpPacket(const char *p) {
  
  uint32_t iWorkTime = iRingTime ? (Millis() - iRingTime) : 0;
  
  if ( iRingTime && iWorkTime > iMaxTime )
  {
    // Cancel(3);
    Bye(3);
    iRingTime = 0;
  }

  if ( !p )
  {
    // max 5 dial retry when loos first invite packet
    if ( iAuthCnt == 0 && iDialRetries < 5 && iWorkTime > (iDialRetries * 200) )
    {
      iDialRetries++;
      delay(30);
      Invite();
    }
	
    return;
  }

  if ( strstr(p, "SIP/2.0 401 Unauthorized") == p )
  {

    if (register_status == false) {
      Serial.println("handle Register 401");
      Register(p);

    } else {
        Ack(p);
        // call Invite with response data (p) to build auth md5 hashes
        Invite(p);

    }
    
  }
  else if ( strstr(p, "BYE") == p )
  {
    Ok(p);
    iRingTime = 0;
  }
  else if ( strstr(p, "SIP/2.0 200") == p )		// OK
  {
    ParseReturnParams(p);
    Ack(p);
  }
  else if (    strstr(p, "SIP/2.0 183 ") == p 	// Session Progress
            || strstr(p, "SIP/2.0 180 ") == p )	// Ringing
  {
    ParseReturnParams(p);
  }
  else if ( strstr(p, "SIP/2.0 100 ") == p )	// Trying
  {
    ParseReturnParams(p);
    Ack(p);
  }
  else if (    strstr(p, "SIP/2.0 486 ") == p 	// Busy Here
            || strstr(p, "SIP/2.0 603 ") == p 	// Decline
            || strstr(p, "SIP/2.0 487 ") == p) 	// Request Terminatet
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


void Sip::AddSipLine(const char* constFormat , ... ) {
  
  va_list arglist;
  va_start(arglist, constFormat);
  uint16_t l = (uint16_t)strlen(message_buffer);
  char *p = message_buffer + l;
  vsnprintf(p, buffer_size - l, constFormat, arglist );
  va_end(arglist);
  l = (uint16_t)strlen(message_buffer);
  if ( l < (buffer_size - 2) )
  {
    message_buffer[l] = '\r';
    message_buffer[l + 1] = '\n';
    message_buffer[l + 2] = 0;
  }
}


// Search a line in response date (p) and append on pbuf
bool Sip::AddCopySipLine(const char *p, const char *psearch) {

  char *pa = strstr((char*)p, psearch);

  if ( pa )
  {
    char *pe = strstr(pa, "\r");

    if ( pe == 0 )
      pe = strstr(pa, "\n");

    if ( pe > pa )
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


// Parse parameter value from http formated string
bool Sip::ParseParameter(char *dest, int destlen, const char *name, const char *line, char cq) {

  const char *qp;
  const char *r;

  if ( ( r = strstr(line, name) ) != NULL )
  {
    r = r + strlen(name);
    qp = strchr(r, cq);
    int l = qp - r;
    if ( l < destlen )
    {
      strncpy(dest, r, l);
      dest[l] = 0;
	    return true;
    }
  }
  return false;
}


// Copy Call-ID, From, Via and To from response to caRead using later for BYE or CANCEL the call
bool Sip::ParseReturnParams(const char *p) {
  
  message_buffer[0] = 0;
  
  AddCopySipLine(p, "Call-ID: ");
  AddCopySipLine(p, "From: ");
  AddCopySipLine(p, "Via: ");
  AddCopySipLine(p, "To: ");
  
  if ( strlen(message_buffer) >= 2 )
  {
    strcpy(caRead, message_buffer);
    caRead[strlen(caRead) - 2] = 0;
  }
  
  return true;
}


int Sip::GrepInteger(const char *p, const char *psearch) {

  int param = -1;
  const char *pc = strstr(p, psearch);

  if ( pc )
  {
    param = atoi(pc + strlen(psearch));
  }
  
  return param;
}


void Sip::Ack(const char *p) {
  
  char ca[32];
 
  bool b = ParseParameter(ca, (int)sizeof(ca), "To: <", p, '>');
 
  if ( !b )
    return;

  message_buffer[0] = 0;
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




void Sip::Cancel(int cseq) {
  
  if ( caRead[0] == 0 )
    return;

  message_buffer[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "CANCEL", pDialNr, sip_server_address.c_str());
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s",  cseq, "CANCEL");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}


void Sip::Bye(int cseq) {
  
  if ( caRead[0] == 0 )
    return;

  message_buffer[0] = 0;
  AddSipLine("%s sip:%s@%s SIP/2.0",  "BYE", pDialNr, sip_server_address.c_str());
  AddSipLine("%s",  caRead);
  AddSipLine("CSeq: %i %s", cseq, "BYE");
  AddSipLine("Max-Forwards: 70");
  AddSipLine("User-Agent: sip-client/0.0.1");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  SendUdp();
}


void Sip::Ok(const char *p) {
  
  message_buffer[0] = 0;
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

void Sip::Register(const char *pIn){
  
  char aux[128];
  uint32_t cseq = 1;
  uint32_t expire_time = 3600;
  uint32_t callid = Random();
  tagid = Random();
  branchid = Random();

  if ( pIn ) {
    cseq = 2;
    Serial.printf("Response:%s", pIn);
    if(ParseParameter(aux, 128, " Expires:", pIn)){
        expire_time = atoi(aux);
        Serial.printf("got expire_time=%d from %s\r\n", expire_time, aux);
    } else {
      Serial.printf("default expire_time=%d\r\n", expire_time);
    }

    if(ParseParameter(aux, 128, "Call-ID: ", pIn, '\n')){
        callid = atoi(aux);
        Serial.printf("got Call-ID:=%d from %s\r\n", callid, aux);
    } else {
      Serial.printf("default Call-ID:=%d\r\n", callid);
    }

    if(ParseParameter(aux, 128, ";tag=", pIn, '\n')){
        tagid = atoi(aux);
        Serial.printf("got ;tag=:=%d from %s\r\n", tagid, aux);
    } else {
      Serial.printf("default tagid:=%d\r\n", tagid);
    }
    
  }

  String sendMessage;
  snprintf (aux, 128, "REGISTER sip:%s SIP/2.0\r\n", sip_server_address.c_str());
  sendMessage = aux;
  snprintf (aux, 128, "Via: SIP/2.0/UDP %s:%d;branch=%d\r\n", local_ip_address.c_str(), local_ip_port, branchid);
  sendMessage += aux;
  sendMessage += "Max-Forwards: 70\r\n";
	snprintf (aux, 128, "From: <sip:%s@%s>;tag=%d\r\n", sip_user_name.c_str(),  sip_server_address.c_str(), tagid);
  sendMessage += aux;
  snprintf (aux, 128, "To: <sip:%s@%s>\r\n", sip_user_name.c_str(),  sip_server_address.c_str());
  sendMessage += aux;
  snprintf (aux, 128, "Call-ID: %010d@%s\r\n", callid, local_ip_address.c_str());
  sendMessage += aux;
  snprintf (aux, 128, "CSeq: %d REGISTER\r\n", cseq);
  sendMessage += aux;
  sendMessage += "User-Agent: telluino sip-client 0.0.1\r\n";

  if ( pIn )
  {
    // authentication
    char *caRealm=NULL, *caNonce=NULL, *haResp=NULL;
    caRealm = caRead;
    caNonce = caRead + 128;
    haResp = message_buffer + buffer_size - 34;
    calculateMd5Hash(pIn, caRealm, caNonce, haResp);
    Serial.println("after MD5 calc1");
    Serial.printf("after MD5 calc2 %d\r\n", caRealm);
    Serial.printf("after MD5 calc3 %d\r\n",  caNonce );
    Serial.printf("after MD5 calc4 %d\r\n",   haResp);
    Serial.println("after MD5 calc5");
    snprintf (aux, 128, "Authorization: Digest username=\"%s\", ", sip_user_name.c_str());
    sendMessage += aux;
    Serial.println("after MD5 calc2");
    snprintf(aux, 128, "realm=\"%s\", algorithm=MD5, ", caRealm);
    sendMessage += aux;
    Serial.println("after MD5 calc3");
    snprintf (aux, 128, "uri=\"sip:%s\", nonce=\"%s\", response=\"%s\"\r\n", sip_server_address.c_str(), caNonce, haResp);
    sendMessage += aux;
    Serial.println("after MD5 calc4");
  } else {
    snprintf (aux, 128, "Authorization: Digest username=\"%s\",", sip_user_name.c_str());
    sendMessage += aux;
    snprintf (aux, 128, "realm=\"sip.voipbuster.com\",algorithm=MD5,uri=\"sip:sip.voipbuster.com\",");
    sendMessage += aux;
    snprintf (aux, 128, "nonce=\"4008660829\",response=\"0db633f99d2e2884bb7c66b23c9ead3d\"\r\n");
    sendMessage += aux;

  }
  
  snprintf (aux, 128, "Expires: %d\r\n", expire_time); // Set the desired expiration time 
  sendMessage += aux;
  snprintf (aux, 128, "Contact: <sip:s@%s:%d>\r\n", local_ip_address.c_str(), local_ip_port);
  sendMessage += aux;
  sendMessage += "Content-Length: 0\r\n";
  caRead[0] = 0;
  
  if(sendMessage.length() >= buffer_size) {
    snprintf (aux, 128, "Cant write message, is to big");
    Serial.println(aux);
    return;

  }
  
  strncpy(message_buffer, sendMessage.c_str(), sendMessage.length());
  SendUdp();

}

// Call invite without or with the response from peer
void Sip::Invite(const char *p) {

  // prevent loops
  if ( p && iAuthCnt > 3 )
    return;

  

  int   cseq = 1;
  
  if ( !p )
  {
    iAuthCnt = 0;
	
    if ( iDialRetries == 0 )
    {
      callid = Random();
      tagid = Random();
      branchid = Random();
    }
  }
  else
  {
    cseq = 2;
   
  }
  
  message_buffer[0] = 0;
  AddSipLine("INVITE sip:%s@%s SIP/2.0", pDialNr, sip_server_address);
  AddSipLine("Call-ID: %010u@%s",  callid, local_ip_address);
  AddSipLine("CSeq: %i INVITE",  cseq);
  AddSipLine("Max-Forwards: 70");
  // not needed for fritzbox
  // AddSipLine("User-Agent: sipdial by jl");
  AddSipLine("From: \"%s\"  <sip:%s@%s>;tag=%010u", pDialDesc, sip_user_name, sip_server_address, tagid);
  AddSipLine("Via: SIP/2.0/UDP %s:%i;branch=%010u;rport=%i", local_ip_address, local_ip_port, branchid, local_ip_port);
  AddSipLine("To: <sip:%s@%s>", pDialNr, sip_server_address);
  AddSipLine("Contact: \"%s\" <sip:%s@%s:%i;transport=udp>", sip_user_name, sip_user_name, local_ip_address, local_ip_port);
  
  if ( p )
  {
    // authentication
    char *caRealm, *caNonce, *haResp;
    calculateMd5Hash(p, caRealm, caNonce, haResp);
    AddSipLine("Authorization: Digest username=\"%s\",realm=\"%s\",nonce=\"%s\",uri=\"sip:%s@%s\",response=\"%s\"", sip_user_name, caRealm, caNonce, pDialNr, sip_server_address.c_str(), haResp);
    iAuthCnt++;
  }
  
  AddSipLine("Content-Type: application/sdp");
  // not needed for fritzbox
  // AddSipLine("Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO");
  AddSipLine("Content-Length: 0");
  AddSipLine("");
  caRead[0] = 0;
  SendUdp();
}

void Sip::calculateMd5Hash(const char*pIn, char *caRealm, char *caNonce, char *haResp){
  // using caRead for temp. store realm and nonce
  
  
  char *ha1Hex = message_buffer;
  char *ha2Hex = message_buffer + 33;
  char *pTemp = message_buffer + 66;
  
  Serial.println(">>calculateMd5Hash 1");

  if (!ParseParameter(caRealm, 128, "realm=\"", pIn)){
    Serial.println("unable to parse realm");
    caRead[0] = 0;
    return;    
  } 

  Serial.printf(">>calculateMd5Hash 2 %s \r\n", caRealm);

  if (!ParseParameter(caNonce, 128, "nonce=\"", pIn)){
    Serial.println("unable to parse nonce");
    caRead[0] = 0;
    return;    
  } 

  Serial.printf(">>calculateMd5Hash 3 %s \r\n",caNonce );

  snprintf(pTemp, buffer_size - 100, "%s:%s:%s", sip_user_name.c_str(), caRealm, sip_user_password.c_str());
  MakeMd5Digest(ha1Hex, pTemp);

  Serial.println(">>calculateMd5Hash 4");
  snprintf(pTemp, buffer_size - 100, "INVITE:sip:%s@%s", pDialNr, sip_server_address.c_str());
  MakeMd5Digest(ha2Hex, pTemp);
  Serial.println(">>calculateMd5Hash 5");
  snprintf(pTemp, buffer_size - 100, "%s:%s:%s", ha1Hex, caNonce, ha2Hex);
  MakeMd5Digest(haResp, pTemp);

  Serial.printf(">>calculateMd5Hash 3 %s \r\n", haResp);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hardware dependent interface functions
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t Sip::Millis() {
	
  return (uint32_t)millis() + 1;
}


// Generate a 30 bit random number
uint32_t Sip::Random() {
	
  // return ((((uint32_t)rand())&0x7fff)<<15) + ((((uint32_t)rand())&0x7fff));
  return secureRandom(0x3fffffff);
}


int Sip::SendUdp() {
	
  Udp.beginPacket(sip_server_address.c_str(), sip_server_port);
  Udp.write(message_buffer, strlen(message_buffer));
  Udp.endPacket();
 
  Serial.printf("\r\n----- send %i bytes to %s:%d-----------------------\r\n%s", 
  strlen(message_buffer), sip_server_address.c_str(), sip_server_port, message_buffer);
  Serial.printf("------------------------------------------------\r\n");
 
  return 0;
}


void Sip::MakeMd5Digest(char *pOutHex33, char *pIn) {
  
  MD5Builder aMd5;
  
  aMd5.begin();
  aMd5.add(pIn);
  aMd5.calculate();
  aMd5.getChars(pOutHex33);
}
