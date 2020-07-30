#ifndef _VOIPPHONE_h_
#define _VOIPPHONE_h_

#include <Arduino.h>
#include <driver/i2s.h>
#include <Ticker.h>
#include "sip.h"
#include "RTPPacket.h"
#include "g711.h"

extern bool DEBUG_MODE;

#define DebugPrintln(a) (DEBUG_MODE ? Serial.println(a) : false)
#define DebugPrint(a) (DEBUG_MODE ? Serial.print(a) : false)

#define MIC_BITS            24 // valid number of bits in I2S data
#define MIC_GAIN            2
#define AMP_GAIN            6
#define SAMPLE_RATE         8000 // Hz 
#define SAMPLE_BITS         32    // bits
#define SAMPLE_T            int32_t
#define MIC_CONVERT(s)      ((s >> (SAMPLE_BITS - MIC_BITS))/2048) //result 13bit
#define DAC_CONVERT(s)      ((s >> (SAMPLE_BITS - MIC_BITS))/65536) //result 8bit
#define VOIPPHONE_OK                1
#define VOIPPHONE_ERROR_RTPSOCKET       -1
#define VOIPPHONE_ERROR_AMP_I2S_DRIVER  -2
#define VOIPPHONE_ERROR_AMP_PIN         -3
#define VOIPPHONE_ERROR_MIC_I2S_DRIVER  -4
#define VOIPPHONE_ERROR_MIC_PIN         -5
#define VOIPPHONE_ERROR_AMP_I2S_DRIVER  -2


class VOIPPhone {
  public:
    VOIPPhone();
    int begin(const char *sipip,const char *sipuser,const char *sippasswd);
    void dial(const char *number,const char *id);
    void loop(void);
    void setMicGain(uint8_t gain);
    void setAmpGain(uint8_t gain);
    bool isBusy(void);
    void setEchoCompensation(bool enable, long threshold_level, uint8_t damping);
  private:
    int initi2samp(void);
    int initi2smic(void);
    static esp_err_t readFromMic(void *dest,size_t size,size_t *bytes_read);
    esp_err_t writeToAmp(const void *src,size_t size, size_t *bytes_written);
    static void tx_rtp(void);
    void handleIncommingRTP(void);
    void handleOutgoingRTP(void);
    void handleSIP(void);

    static const i2s_port_t I2S_PORT0 = I2S_NUM_0; //mems
    const i2s_port_t I2S_PORT1 = I2S_NUM_1; //amplifier

    static WiFiUDP rtpudp;
   
    char caSipOut[2048];
    uint8_t rtpbuffer[2048];
    static Sip *sip;
    static uint8_t amp_gain;
    static uint8_t mic_gain;
    static uint8_t echodamping;
    static bool echocompensation;
    long level;
    long ec_threshold_level;
    uint8_t echodamping_value;
    Ticker tx_streamticker;
    bool tx_streamisrunning = false;
    bool rx_streamisrunning = false;
    int rtppkgsize = -1;
    size_t packetSize;
    // sip params
    int sipport = 5060;          // Port so lassen
    char myip[18];
    //char sipip_cstr[18];
};



#endif
