#include <SD.h>
#include <LittleFS.h>
#include <MTP_Teensy.h>

#define DBGSerial Serial

FS *myfs;
File dataFile;
int record_count = 0;
bool write_data = false;
uint8_t current_store = 0;

LittleFS_RAM      ramdisk;
LittleFS_Program  progdisk;
LittleFS_SPI      flashchip;
LittleFS_SPI      flash2chip;

SDClass sd;

#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY36) || defined(ARDUINO_TEENSY35)
const int SD_ChipSelect = BUILTIN_SDCARD;
#else
const int SD_ChipSelect = 10;
#endif
const int Flash_ChipSelect = 5;
const int Flash2_ChipSelect = 6;


void setup()
{
  // let msusb stuff startup as soon as possible
  //usbmsc.begin();
  for (uint8_t pin = 3; pin < 7; pin++) {
    pinMode(pin, OUTPUT);
    digitalWriteFast(pin, HIGH);
  }

  DBGSerial.begin(9600);
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }

  DBGSerial.print(CrashReport);
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(1000);
  
  // mandatory to begin the MTP session.
  MTP.begin();

  // Add a RAM Disk
#if defined(__IMXRT1062__)
  if (ramdisk.begin(65536)) {
    MTP.addFilesystem(ramdisk, "RAM");
    // storage.setIndexStore(istore);
    DBGSerial.println("Ram disk initialized");
  }
#endif
  // Add the SD Card
  if (sd.begin(SD_ChipSelect)) {
    MTP.addFilesystem(sd, "SD_Card");
    DBGSerial.println("SD Card initialized");
  } else {
    Serial.println("SD Failed to initialize");
  }

  // Add a disk with unused Program memory
  if (progdisk.begin(400000)) {
    MTP.addFilesystem(progdisk, "Program");
    DBGSerial.println("Program Storage initialized");
  }

  // Add a SPI Flash chip
  if (flashchip.begin(Flash_ChipSelect)) {
    MTP.addFilesystem(flashchip, "Flash");
    DBGSerial.println("SPI Flash initialized");
  } else {
    Serial.printf("Little FS %u failed\n", Flash_ChipSelect);
  }

  // Add a SPI Flash chip
  if (flash2chip.begin(Flash2_ChipSelect)) {
    MTP.addFilesystem(flash2chip, "Flash2");
    DBGSerial.println("SPI Flash initialized");
  } else {
    Serial.printf("Little FS2 %u failed\n", Flash2_ChipSelect);
  }

  DBGSerial.println("\nSetup done");
}


int ReadAndEchoSerialChar() {
  int ch = DBGSerial.read();
  if (ch >= ' ') DBGSerial.write(ch);
  return ch;
}

void loop()
{
  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    uint8_t storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = DBGSerial.read();

    uint32_t fsCount;
    switch (command) {
    case '1':
      // first dump list of storages:
      MTP.printFilesystemsInfo();
      //Serial.println("\nIndex List");
      //MTP.printIndexList();
      break;
    case '2':
      myfs = MTP.getFilesystemByIndex(storage_index);
      current_store = storage_index;
      if (myfs) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index,
          MTP.getNameByIndex(storage_index));
      } else {
        DBGSerial.printf("Storage Index %u out of range\n", storage_index);
      }
      break;
    case 'l': listFiles(); break;
    case 's':
      DBGSerial.println("\nLogging Data!!!");
      write_data = true;   // sets flag to continue to write data until new command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append data to
      // to the file created.
      dataFile = myfs->open("datalog.txt", FILE_WRITE);
      logData();
      break;
    case 'x': stopLogging(); break;
    case'r':
      DBGSerial.println("Reset");
      MTP.reset();
      break;
    case 'd': dumpLog(); break;
    case '\r':
    case '\n':
    case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
  } else {
    MTP.loop();
    //#if USE_MSC > 0
    //usbmsc.checkUSBStatus(false);
    //#endif
  }

  if (write_data) logData();
}

void logData()
{
  // make a string for assembling the data to log:
  String dataString = "";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    // print to the serial port too:
    DBGSerial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    DBGSerial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging()
{
  DBGSerial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  DBGSerial.printf("Records written = %d\n", record_count);
  MTP.reset();
}


void dumpLog()
{
  DBGSerial.println("\nDumping Log!!!");
  // open the file.
  dataFile = myfs->open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      DBGSerial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    DBGSerial.println("error opening datalog.txt");
  }
}

void menu()
{
  DBGSerial.println();
  DBGSerial.println("Menu Options:");
  DBGSerial.println("\t1 - List Drives (Step 1)");
  DBGSerial.println("\t2 - Select Drive for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\te - Erase files on disk");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tr - Reset MTP");
  DBGSerial.println("\th - Menu");
  DBGSerial.println();
}

void listFiles()
{
  DBGSerial.print("\n Space Used = ");
  DBGSerial.println(myfs->usedSize());
  DBGSerial.print("Filesystem Size = ");
  DBGSerial.println(myfs->totalSize());

  printDirectory(myfs);
}
#if USE_MSC > 0
extern PFsLib pfsLIB;
#endif

void printDirectory(FS *pfs) {
  DBGSerial.println("Directory\n---------");
  printDirectory(pfs->open("/"), 0);
  DBGSerial.println();
}

void printDirectory(File dir, int numSpaces) {
  while (true) {
    File entry = dir.openNextFile();
    if (! entry) {
      //DBGSerial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    DBGSerial.print(entry.name());
    if (entry.isDirectory()) {
      DBGSerial.println("/");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      printSpaces(36 - numSpaces - strlen(entry.name()));
      DBGSerial.print("  ");
      DBGSerial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    DBGSerial.print(" ");
  }
}


uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ') ch = DBGSerial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = DBGSerial.read();
  }
  return return_value;
}
