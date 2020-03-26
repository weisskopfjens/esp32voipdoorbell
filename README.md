# ESP32voipdoorbell
A voip based door bell / phone realized with ESP32 and s2i mems + s2i dac

**It is still under development.**

Following things are already working:
* Connect to a SIP server ( fritzbox router )
* Initate a call ( SIP/SDP protocol )
* 3 seperate buttons. Each initiate a call to a diffrent phonenumber.
* A two way voice transmission between the doorphone and the called party. (RTP protocol, G.711 alaw codec )
* Web based configuration. Parameters and wifi credentials.
* Web based firmware update.

Planned features:
* Sitch a connected relais with a MFV tone.
* WS2812b LEDs for lightning the door bell lables and for giving an optical feedback.
* A wellcome message for visitors.

Used hardware:
* ESP32 dev board
* i2s MEMS mic type INMP441
* MAX98357 I2S audio amplifier
* 3x WS2812b LEDs

Requirements:
* ConfigManager@>=1.5.2
* ArduinoJson@>=5.13.4
* clickButton@2012.08.29
* NTP@1.3.1
* WS2812FX@1.2.3
* Adafruit NeoPixel@1.3.5

[Project homepage](https://jensweisskopf.de/wp/esp-32-als-voip-tuersprechstelle/)

## default PINOUT for ESP32voipdoorbell

|ESP32 GPIO   | Connected to | notes             |
|-------------|--------------|-------------------|
|4            | Button 1     | second pin to gnd |
|2            | Button 2     | second pin to gnd |
|15           | Button 3     | second pin to gnd |
|I2S Amplifier|              |                   |
|14           | BCLK         |                   |
|27           | LRC          |                   |
|12           | DIN          |                   |
|I2S          |              |                   |
|33           | BCKL SCK     |                   |
|32           | LRCL WS      |                   |
|35           | DOUT SD      |                   |
|             |              |                   |