#include <SD.h>
#include <USBHost_t36.h>
#include <MTP_Teensy.h>
#include <LittleFS.h>

#define CS_PIN  BUILTIN_SDCARD

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBDrive mydrive1(myusb);
USBDrive mydrive2(myusb);
USBDrive mydrive3(myusb);
USBDrive mydrive4(myusb);
USBDrive mydrive5(myusb);
USBFilesystem mydisk1(myusb);
USBFilesystem mydisk2(myusb);
USBFilesystem mydisk3(myusb);
USBFilesystem mydisk4(myusb);
USBFilesystem mydisk5(myusb);

LittleFS_SPI   s2;
LittleFS_SPI   sNand3;
LittleFS_SPI   sNand4;
LittleFS_SPI   sFlash5;
LittleFS_SPI   sFlash6;
LittleFS_SPI   s7;
LittleFS_SPI   s8;

LittleFS_RAM   ram1;
LittleFS_RAM   ram2;
LittleFS_RAM   ram3;

void setup() {
  Serial.begin(9600);
  MTP.begin();
  SD.begin(CS_PIN);
  MTP.addFilesystem(SD, "SD Card");
  Serial.printf("Volume label = %s\n", SD.name());

  myusb.begin();
  MTP.addFilesystem(mydisk1, "USB Disk1");
  MTP.addFilesystem(mydisk2, "USB Disk2");
  MTP.addFilesystem(mydisk3, "USB Disk3");
  MTP.addFilesystem(mydisk4, "USB Disk4");
  MTP.addFilesystem(mydisk5, "USB Disk5");
  
  s2.begin(2, SPI);
  MTP.addFilesystem(s2, "s2");
  sNand3.begin(3, SPI);
  MTP.addFilesystem(sNand3, "sNand3");
  sNand4.begin(4, SPI);
  MTP.addFilesystem(sNand4, "sNand4");
  sFlash5.begin(5, SPI);
  MTP.addFilesystem(sFlash5, "sflash5");
  sFlash6.begin(6, SPI);
  MTP.addFilesystem(sFlash6, "sflash6");
  s7.begin(7, SPI);
  MTP.addFilesystem(s7, "s7");
  s8.begin(8, SPI);
  MTP.addFilesystem(s8, "s8");

  ram1.begin(60000);
  MTP.addFilesystem(ram1, "ram1");
  ram2.begin(30000);
  MTP.addFilesystem(ram2, "ram2");
  ram3.begin(80000);
  MTP.addFilesystem(ram3, "ram3");
  
  // sets the storage for the index file
  //MTP.useFileSystemIndexFileStore(0);
  Serial.println("\nSetup done");
}

void loop() {
  MTP.loop();
  myusb.Task();
  // do other things here...
}

