#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <EEPROM.h>
#include <mrf24j40.h>
#include <avr/wdt.h>

#define PIN_CLK 13 //SPI Clock            DI
#define PIN_SDI 12 //SPI MISO             DI
#define PIN_SDO 11 //SPI MOSI             DO
#define PIN_CS 9 //SPI SS                DI
#define PIN_INT 3 //MRF24J40 Interrupt    DO
#define PIN_LED 10 //blinky led

EthernetUDP Udp;
MRFClass mrf(PIN_CS, PIN_INT, 1);  //Create wireless object

#define UDP_TX_PACKET_MAX_SIZE 64

byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
unsigned int localPort = 5151;      // local port to listen on
IPAddress server_ip(192, 168, 1, 200);

bool WD_en;
volatile uint8_t int_mrf;

void setup() {
  Serial.begin(19200);
  
  initialize_mrf();
  initialize_ethernet();
  
  WD_en=1;
  wdt_enable(WDTO_4S);
}

void initialize_ethernet() {
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }

  // print your local IP address:
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print("."); 
  }
  Serial.println();

  Udp.begin(localPort);
}

void initialize_mrf() {
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV16);
  SPI.begin();

  mrf.reset();
  mrf.init();

  Serial.println("PAN COORD");

  attachInterrupt(1, mrf_isr, FALLING);   
  interrupts();
}

void mrf_isr() {
  int_mrf=1;
}

void rx_udp_packet(int packetSize) {
  byte packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
  IPAddress remote;

//  Serial.print("Received packet of size ");
//  Serial.println(packetSize);
  //Serial.print("From ");
  remote = Udp.remoteIP();
//  for (int i =0; i < 4; i++)
//  {
//    Serial.print(remote[i], DEC);
//    if (i < 3)
//    {
//      Serial.print(".");
//    }
//  }
//  Serial.print(", port ");
//  Serial.println(Udp.remotePort());

  // read the packet into packetBufffer
  Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);
  //Serial.println("Contents:");
  //Serial.println(packetBuffer);

  switch(packetBuffer[0]) {
  case 1: //upd to mrf packet
    mrf.udp_to_mrf(packetSize,packetBuffer);
    break;
  case 2: //reset uC
    WD_en=0;
    break;
  default:
    Serial.println("unknown UDP packet type");
  }
}

void tx_udp_packet() {
  int i;
  
  //Serial.println("tx udp packet");
  //Serial.println(mrf._udp_pending);
  
  Udp.beginPacket(server_ip, 5151);
  for(i=0;i<mrf._udp_pending;i++) {
    Udp.write(mrf.udp_buffer[i]);
    //Serial.println(mrf.udp_buffer[i]);
  }
  
  Udp.endPacket();
  mrf._udp_pending = 0;
}

void loop() {
  int packetSize;

  packetSize = Udp.parsePacket();
  if(packetSize > 0) {
    rx_udp_packet(packetSize);
    //Serial.println("udp packet");
  }

  mrf.PC_loop();

  if(int_mrf) {
    byte last_interrupt = mrf.get_interrupts();
    
    if(last_interrupt & MRF_I_RXIF) {
      //Serial.println("rxxing...");
      mrf.rx_toBuffer();
    }
    if(last_interrupt & MRF_I_TXNIF) {
      //Serial.println("txxing...");
      mrf.tx_status();
    }
    if(last_interrupt & MRF_I_SECIF) {
      //Serial.println("decrypting...");
      mrf.decrypt();
    }
    
    if(last_interrupt & 0xE6) {
      Serial.print("ERR: INT 0x");
      Serial.println(last_interrupt & 0xE6);
    }
    
    int_mrf=0;
  }

  if(mrf._rx_count>0) {
    mrf.rx_packet();
    mrf._rx_count--;
  }

  if(mrf._udp_pending>0) {
    tx_udp_packet();
  }
  
  if(WD_en) {
    wdt_reset();
  }
}

