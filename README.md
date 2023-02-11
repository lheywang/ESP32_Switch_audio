# ESP32_Switch_audio
A small audio_switch, controlled via WiFi. He has two functionnality : 
- Selection from one 3.5mm jack, and output it to the fourth 3.5mm jack.
- Speaker switch : from one input, you can enable or disable up to 4 pairs of speaker, independently.

Recommendations :
- The switch use an 5V Micro-USB type B as power supply (on the ESP32 Devboard). The PCB take power from the VUSB / 5V pin.
  So, take care to use a USB power who can deliver at least 1A (1.5A is recommended).
 
- As i said, the system take power from the micro USB port on ESP32 Devboard. So take care to buy an ESP32 who HAVE this port. Not all boards have one.

- The system does not include any amps, you will need it !
- The board is designed for something like 60W of power. The relay can handle up to 250VAC / 8A, so you have some freedom with power.


How to use it ?
To deploy the system, it needs 2 pieces of software : Arduino IDE (1.8.9) and ESP32 Sketch Data Upload (not supported on Arduino 2.x versions).
1) Modify the code for you (insert your wifi SSID and password)
2) flash the data using the tool on the SPIFFS. Don't forget to press the button "boot" on the ESP32 board to enable flash.
3) flash the code. Don't forget to press the button "boot" on the ESP32 board to enable flash.
4) Using en serial monitor, check if the ESP32 can make WiFi connection, and note the local IP printed on the monitor. You will need this to acces to the web page, using your brothers
5) Put the switch in place, and enjoy !



Hardware notes :
- The system is designed to maintain a correct impedance for the amps.
Pairs 1 and 2, when both activated are wired in serial, as like as Pairs 3 and 4.
These two pairs of output are wired in parallel. 
These design to achieve 2 speaker in serial, and 2 in parallel.

In the worst case, you will get 1/2 of the speaker impedance, and in the other case you will get the double of the nominal impedance.
Take care to wire it correctly : 

- 1 pairs : anywhere
- 2 pairs : 1 + 2 / 3 + 3 (1 + 3 / 2 + 4 will work, at 1/2 of the impedance, you choose)
- 3 pairs : 1 + 2 + 3
- 4 pairs : 1 + 2 + 3 + 4

I use 7 sealed relay, to prevent damage from external conditions (audio signal cannot clean the switches, so they need to be sealed)


