#include"voipphone.h"

//bool DEBUG_MODE = false;
Sip *VOIPPhone::sip = NULL;
WiFiUDP VOIPPhone::rtpudp = WiFiUDP();
uint8_t VOIPPhone::amp_gain = AMP_GAIN;
uint8_t VOIPPhone::mic_gain = MIC_GAIN;
bool VOIPPhone::echocompensation = false;
uint8_t VOIPPhone::echodamping = 1;

VOIPPhone::VOIPPhone() {
  
}

int VOIPPhone::begin(const char *sipip,const char *sipuser,const char *sippasswd) {
  
  strcpy(myip,WiFi.localIP().toString().c_str());
  int connOK = rtpudp.begin(1234);
  if (connOK == 0) {
    DebugPrintln("rtpudp (port:1234) could not get socket");
    return VOIPPHONE_ERROR_RTPSOCKET;
  } else {
    DebugPrintln("rtpudp listen on (port:1234)");
  }
  DebugPrint("Init SIP...");
  sip = new Sip(caSipOut, sizeof(caSipOut));
  sip->Init(sipip, sipport , myip, sipport, sipuser, sippasswd, 15); // 15 seconds
  DebugPrintln("[OK]");
  // i2s devices
  int result = initi2smic();
  if(result!=VOIPPHONE_OK) {
    return result;
  }
  result = initi2samp();
  if(result!=VOIPPHONE_OK) {
    return result;
  }
  return VOIPPHONE_OK;
}

//
// Dialing
//

void VOIPPhone::dial(const char *number,const char *id) {
  DebugPrintln("Dialing");
  i2s_start(I2S_PORT1);
  rx_streamisrunning = true;
  sip->Dial(number, id);
}

//
// Loop
//

void VOIPPhone::loop(void) {
  if(WiFi.status() == WL_CONNECTED) {
    handleIncommingRTP();
    handleOutgoingRTP();
    sip->HandleUdpPacket();
  }
}

void VOIPPhone::setMicGain(uint8_t gain) {
  mic_gain = gain;
}

void VOIPPhone::setAmpGain(uint8_t gain) {
  amp_gain = gain;
}

bool VOIPPhone::isBusy(void) {
  return sip->IsBusy();
}


//
// Initialize i2s amplifier
//

int VOIPPhone::initi2samp() {
  esp_err_t err;
  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX), // Receive, not transfer
      .sample_rate = SAMPLE_RATE,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 16,                           // number of buffers
      .dma_buf_len = 60,                    // 8 samples per buffer (minimum)
      .use_apll = false
  };
  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,   // BCLK
      .ws_io_num = 12,    // LRC
      .data_out_num = 27, // DOUT -> DIN
      .data_in_num = -1   // not used
  };
  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  DebugPrintln("Init i2s amp...");
  err = i2s_driver_install(I2S_PORT1, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    return VOIPPHONE_ERROR_AMP_I2S_DRIVER;
  }
  err = i2s_set_pin(I2S_PORT1, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    return VOIPPHONE_ERROR_AMP_PIN;
  }
  DebugPrintln("I2S driver installed.");
  return VOIPPHONE_OK;
}

//
// Initialize i2s microphone
//

int VOIPPhone::initi2smic() {
  esp_err_t err;
  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = SAMPLE_RATE,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 8                              // 8 samples per buffer (minimum)
  };
  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 26,   // BCKL SCK
      .ws_io_num = 25,    // LRCL WS
      .data_out_num = -1, // not used (only for speakers)
      .data_in_num = 33   // DOUT SD
  };
  // Configuring the I2S driver and pins.
  // This pfunction must be called before any I2S driver read/write operations.
  DebugPrintln("Init i2s mems...");
  err = i2s_driver_install(I2S_PORT0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed installing driver: %d\n", err);
    return VOIPPHONE_ERROR_MIC_I2S_DRIVER;
  }
  err = i2s_set_pin(I2S_PORT0, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed setting pin: %d\n", err);
    return VOIPPHONE_ERROR_MIC_PIN;
  }
  DebugPrintln("I2S driver installed.");
  return VOIPPHONE_OK;
}

//
// Read from microphone
//

esp_err_t VOIPPhone::readFromMic(void *dest,size_t size,size_t *bytes_read) {
  return i2s_read(I2S_PORT0,dest,size,bytes_read,portMAX_DELAY); // no timeout
}

esp_err_t VOIPPhone::writeToAmp(const void *src,size_t size, size_t *bytes_written) {
  return i2s_write(I2S_PORT1,src,size,bytes_written,portMAX_DELAY);
}

//
// Send RTP packet every 20ms
//
void VOIPPhone::tx_rtp(){
  uint8_t temp[160];
  static uint16_t sequenceNumber = 0;
  const uint32_t ssrc = 3124150;
  static uint32_t timestamp = 0;
  RTPPacket txRTPpkg;
  uint8_t packetBuffer[255];

  for(int i=0;i<160;i++) {
    //8192 13bit
    //16777216 u24bit
    SAMPLE_T sample = 0;
    size_t num_bytes_read;
    //i2s_read(I2S_PORT0,&sample,sizeof(SAMPLE_T),&num_bytes_read,portMAX_DELAY); // no timeout
    readFromMic(&sample,sizeof(SAMPLE_T),&num_bytes_read);
    if (num_bytes_read > 0) {
      if(echocompensation) {
        temp[i] = ALaw_Encode(MIC_CONVERT(sample)*mic_gain/echodamping);
      } else {
        temp[i] = ALaw_Encode(MIC_CONVERT(sample)*mic_gain);
      }
      
    }
  }
  if(sequenceNumber==0) {
    timestamp = 0;
  }
  txRTPpkg = RTPPacket(temp, sequenceNumber++, ssrc, timestamp+=160,8,160);
  int length = txRTPpkg.serialize(packetBuffer);
  IPAddress tempip;
  tempip.fromString(sip->GetSIPServerIP()); 
  int beginOK = rtpudp.beginPacket(tempip, atoi(sip->audioport) );
  if (beginOK == 0) {
    //rtpudp.stop();
  } else {
    rtpudp.write(packetBuffer,length);
    //int endOK = rtpudp.endPacket();
    rtpudp.endPacket();
  }
}

//
// Handle Rx RTP packages Port 1234
//
void VOIPPhone::handleIncommingRTP(void) {
  int16_t buffer[500];
  RTPPacket rxRTPpkg;
  
  packetSize = rtpudp.parsePacket();
  if (packetSize > 0 && rx_streamisrunning) {
    packetSize = rtpudp.read(rtpbuffer, sizeof(rtpbuffer));
    rxRTPpkg = RTPPacket();
    rtppkgsize = rxRTPpkg.deserialize(rtpbuffer, packetSize);
    for(int i=0;i<rtppkgsize;i++) {
      buffer[i] = (ALaw_Decode((*(rxRTPpkg.getPayload()+i))))*amp_gain;
      // echocompensation. calculate level.
      if(echocompensation) {
        level = (level + abs(buffer[i])) / 2;
      }
    }
    if(echocompensation && (level > ec_threshold_level)) {
      echodamping = echodamping_value;
    } else {
      echodamping = 1;
    }
    size_t num_bytes_write;
    writeToAmp(buffer,sizeof(int16_t)*rtppkgsize,&num_bytes_write);
  } else {
    rtpudp.flush();
  }
}

//
// Starting Tx RTP stream if call established
//
void VOIPPhone::handleOutgoingRTP(void) {
  if( sip->audioport[0] != '\0' && !tx_streamisrunning ) {
    tx_streamisrunning = true;
    DebugPrint("Starting stream...");
    i2s_start(I2S_PORT0); //mems
    tx_streamticker.attach_ms(20, VOIPPhone::tx_rtp);
    DebugPrintln("[OK]");
  } else if( sip->audioport[0] == '\0' && tx_streamisrunning ) {
    // Hangup. Stopping i2s transmission and stopping tx rtp stream
    tx_streamisrunning = false;
    rx_streamisrunning = false;
    rtppkgsize = -1;
    DebugPrintln("Stream stopped.");
    tx_streamticker.detach();
    DebugPrint("Stopping i2s transmission...");
    i2s_stop(I2S_PORT0); //mems
    i2s_stop(I2S_PORT1); //amp
    DebugPrintln("[OK]");
  }
}

void VOIPPhone::setEchoCompensation(bool enable, long threshold_level, uint8_t damping) {
  echocompensation = enable;
  ec_threshold_level = threshold_level;
  echodamping_value = damping;
}