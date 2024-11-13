#include <MTP_Teensy.h>
#include <LittleFS.h>
#include <SD.h>
#include <USBHost_t36.h>
#include <Entropy.h>

//---------------------------------------------------
// Select drives you want to create
//---------------------------------------------------

#define USE_MEMORY_INDEX  0

#define USE_RAMDISK       1    // RAM Disk, uses T4.1 PSRAM if present
#define USE_RAMDISK_INDEX 0

#define USE_PROGRAM       1    // Program Flash
#define USE_PROGRAM_INDEX 0

#define USE_SD            1    // SD card, SDIO or SPI
#define USE_SD_CSPIN      BUILTIN_SDCARD

#define USE_2ND_SD        1    // another SD card, SPI only
#define USE_2ND_SD_CSPIN  10
#define USE_2ND_SD_PORT   SPI

#define USE_SPI           0    // SPI Flash
#define USE_SPI_CSPIN     22
#define USE_SPI_PORT      SPI

#define USE_QSPI          1    // T4.1 QSPI

#define USE_USB           1    // USB drives on USB host port

#define USE_MEMBOARD4     1    // memory board with 4 chips
#define USE_MEMBOARD8     0    // memory board with 8 chips

#define USE_SW_PU         1 //set to 1 if SPI devices do not have PUs,
                            // https://www.pjrc.com/better-spi-bus-design-in-3-steps/


// =======================================================================
// Global variables
// =======================================================================

#define DBGSerial Serial

FS *myfs;
unsigned int fs_index = 0;
FS *filesystems_list[20];
const char * filesystems_extra_info[20];
unsigned int filesystems_list_count = 0;

File dataFile, myFile;  // Specifes that dataFile is of File type
int record_count = 0;
bool write_data = false;

#define BUFFER_SIZE_INDEX 128
uint8_t write_buffer[BUFFER_SIZE_INDEX];
#define buffer_mult 4
uint8_t buffer_temp[buffer_mult*BUFFER_SIZE_INDEX];

int bytesRead;
uint32_t drive_index = 0;

// These can likely be left unchanged
#define MYBLKSIZE 2048 // 2048
#define SLACK_SPACE  40960 // allow for overhead slack space :: WORKS on FLASH {some need more with small alloc units}
#define size_bigfile 1024*1024*24  //24 mb file

// Various Globals
const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0, warnLFS = 0; // Track warnings or Errors
uint32_t lCnt = 0, LoopCnt = 0; // loop counters
uint64_t rdCnt = 0, wrCnt = 0; // Track Bytes Read and Written
int filecount = 0;
int loopLimit = 0; // -1 continuous, otherwise # to count down to 0
bool bWriteVerify = true;  // Verify on Write Toggle
File file3; // Single FILE used for all functions


// =======================================================================
// Filesystem instances
// =======================================================================

#if USE_RAMDISK==1
LittleFS_RAM ramfs1;
LittleFS_RAM ramfs2;
#endif

#if USE_PROGRAM==1
LittleFS_Program progmfs;
#endif

#if USE_2ND_SD==1
SDClass SD2;
#endif

#if USE_SPI==1
LittleFS_SPI spifs;
#endif

#if USE_QSPI==1
LittleFS_QSPI qspifs;
#endif

#if USE_MEMBOARD4==1 || USE_MEMBOARD8==1
LittleFS_SPI memboard3fs;
LittleFS_SPI memboard4fs;
LittleFS_SPI memboard5fs;
LittleFS_SPI memboard6fs;
#endif

#if USE_MEMBOARD8==1
LittleFS_SPI memboard2fs;
LittleFS_SPI memboard7fs;
LittleFS_SPI memboard8fs;
LittleFS_SPI memboard9fs;
#endif

#if USE_USB==1
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);
USBDrive drive1(myusb);
USBDrive drive2(myusb);
USBDrive drive3(myusb);
USBDrive drive4(myusb);
USBDrive drive5(myusb);
USBFilesystem usbfs1(myusb);
USBFilesystem usbfs2(myusb);
USBFilesystem usbfs3(myusb);
USBFilesystem usbfs4(myusb);
USBFilesystem usbfs5(myusb);
USBFilesystem usbfs6(myusb);
USBFilesystem usbfs7(myusb);
USBFilesystem usbfs8(myusb);
#endif

// =======================================================================
// Program
// =======================================================================

void setup()
{
  pullup_cs_pins();
  Serial.begin(9600);

#if USE_MEMORY_INDEX == 1
  // TODO MTP.useMemoryForIndexList(buffer, sizeof(buffer));
  // Lets add the Program memory version:
  // checks that the LittFS program has started with the disk size specified
  //MTP.useFileSystemIndexFileStore(MTPStorage::INDEX_STORE_MEM_FILE); // TODO...
#endif

#if USE_PROGRAM == 1
  if (progmfs.begin(600*1024)) {
    MTP.addFilesystem(progmfs, "");
    add_to_filesystems_list(progmfs);
    uint64_t totalSize = progmfs.totalSize();
    uint64_t usedSize  = progmfs.usedSize();
    Serial.printf("Program Storage %llu %llu\n", totalSize, usedSize);
    #if USE_PROGRAM_INDEX == 1
    MTP.useFilesystemForIndexList(progmfs);
    #endif
  } else {
    Serial.println("Program Storage failed or missing");
  }
#endif

#if USE_RAMDISK == 1
  uint32_t ramsize = 45*1024;
  if (ramfs1.begin(ramsize)) {
    Serial.printf("RAMDISK Storage %u bytes\n", ramsize);
    MTP.addFilesystem(ramfs1, "RAM1");
    add_to_filesystems_list(ramfs1);
    // attempt to create large RAM disk only if smaller was ok
    ramsize = 1500*1024;
    if (ramfs2.begin(ramsize)) {
      Serial.printf("RAMDISK Storage %u bytes\n", ramsize);
      MTP.addFilesystem(ramfs2, "RAM2");
      add_to_filesystems_list(ramfs2);
      #if USE_RAMDISK_INDEX == 1
      MTP.useFilesystemForIndexList(ramfs2);
      #endif
    } else {
      Serial.printf("RAMDISK Storage %u bytes failed\n", ramsize);
      #if USE_RAMDISK_INDEX == 1
      MTP.useFilesystemForIndexList(ramfs1);
      #endif
    }
  } else {
    Serial.printf("RAMDISK Storage %u bytes failed\n", ramsize);
  }
#endif

#if USE_SD == 1
  #if defined SD_SCK
  SPI.setMOSI(SD_MOSI);
  SPI.setMISO(SD_MISO);
  SPI.setSCK(SD_SCK);
  #endif
  bool sdok = SD.begin(USE_SD_CSPIN);
  MTP.addFilesystem(SD, "SD Card");
  add_to_filesystems_list(SD, "SD Card");
  if (sdok) {
    Serial.printf("SD Storage \"%s\"\n", SD.name());
  } else {
    Serial.printf("SD Storage, no card detected\n");
  }
#endif

#if USE_2ND_SD == 1
  bool sd2ok = SD2.sdfs.begin(SdSpiConfig(USE_2ND_SD_CSPIN,
    SHARED_SPI, SD_SCK_MHZ(16), &(USE_2ND_SD_PORT)));
  MTP.addFilesystem(SD2, "SD #2 Card");
  add_to_filesystems_list(SD2, "SD 2nd Card");
  if (sd2ok) {
    Serial.printf("SD Storage #2 \"%s\"\n", SD2.name());
  } else {
    Serial.printf("SD Storage #2, no card detected\n");
  }
#endif

#if USE_SPI == 1
  if (spifs.begin(USE_SPI_CSPIN, USE_SPI_PORT)) {
    MTP.addFilesystem(spifs, "SPI");
    add_to_filesystems_list(spifs, "SPI");
    Serial.printf("SPI Storage \"%s\" on CS=%u size=%llu\n",
      spifs.name(), USE_SPI_CSPIN, spifs.totalSize());
  } else {
    Serial.printf("SPI Storage on CS=%u failed to start\n", USE_SPI_CSPIN);
  }
#endif

#if USE_QSPI == 1
  if (qspifs.begin()) {
    MTP.addFilesystem(qspifs, "QSPI");
    add_to_filesystems_list(qspifs, "QSPI");
    Serial.printf("QSPI Storage \"%s\" size=%llu\n",
      qspifs.name(), qspifs.totalSize());
  } else {
    Serial.printf("QSPI Storage failed to start\n");
  }
#endif

#if USE_MEMBOARD4==1 || USE_MEMBOARD8==1
  if (memboard3fs.begin(3)) {
     MTP.addFilesystem(memboard3fs, "");
     add_to_filesystems_list(memboard3fs, "MEMBOARD CS pin 3");
  }
  if (memboard4fs.begin(4)) {
     MTP.addFilesystem(memboard4fs, "");
     add_to_filesystems_list(memboard4fs, "MEMBOARD CS pin 4");
  }
  if (memboard5fs.begin(5)) {
     MTP.addFilesystem(memboard5fs, "");
     add_to_filesystems_list(memboard5fs, "MEMBOARD CS pin 5");
  }
  if (memboard6fs.begin(6)) {
     MTP.addFilesystem(memboard6fs, "");
     add_to_filesystems_list(memboard6fs, "MEMBOARD CS pin 6");
  }
#endif

#if USE_MEMBOARD8==1
  if (memboard2fs.begin(2)) {
     MTP.addFilesystem(memboard2fs, "");
     add_to_filesystems_list(memboard2fs, "MEMBOARD CS pin 2");
  }
  if (memboard7fs.begin(7)) {
     MTP.addFilesystem(memboard7fs, "");
     add_to_filesystems_list(memboard7fs, "MEMBOARD CS pin 7");
  }
  if (memboard8fs.begin(8)) {
     MTP.addFilesystem(memboard8fs, "");
     add_to_filesystems_list(memboard8fs, "MEMBOARD CS pin 8");
  }
  if (memboard9fs.begin(9)) {
     MTP.addFilesystem(memboard9fs, "");
     add_to_filesystems_list(memboard9fs, "MEMBOARD CS pin 9");
  }
#endif

#if USE_USB == 1
  DBGSerial.println("\nInitializing USB drives...");
  myusb.begin();
  MTP.addFilesystem(usbfs1, "USB1");
  MTP.addFilesystem(usbfs2, "USB2");
  MTP.addFilesystem(usbfs3, "USB3");
  MTP.addFilesystem(usbfs4, "USB4");
  MTP.addFilesystem(usbfs5, "USB5");
  MTP.addFilesystem(usbfs6, "USB6");
  MTP.addFilesystem(usbfs7, "USB7");
  MTP.addFilesystem(usbfs8, "USB8");
  add_to_filesystems_list(usbfs1, "USB");
  add_to_filesystems_list(usbfs2, "USB");
  add_to_filesystems_list(usbfs3, "USB");
  add_to_filesystems_list(usbfs4, "USB");
  add_to_filesystems_list(usbfs5, "USB");
  add_to_filesystems_list(usbfs6, "USB");
  add_to_filesystems_list(usbfs7, "USB");
  add_to_filesystems_list(usbfs8, "USB");
  DBGSerial.println("USB initialized.");
#endif

  while (!Serial && !DBGSerial.available() && millis() < 5000) {}

  if (CrashReport) {
    Serial.print(CrashReport);
  }
  //This is mandatory to begin the d session.
  MTP.begin();
  //delay(2000);
  delay(500);

  //Serial.printf("USBPHY1_TX = %08X\n", USBPHY1_TX);
  //USBPHY1_TX = 0x10090901;
  //Serial.printf("USBPHY1_TX = %08X\n", USBPHY1_TX);
  //DBGSerial.println("\nSetup done");

  myfs = get_from_filesystems_list(fs_index);
  menu();
}


void loop()
{
#if USE_USB == 1
  myusb.Task();
#endif
  MTP.loop();

  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    if (command == '2') {
      fs_index = CommandLineReadNextNumber(ch, 0);
      myfs = get_from_filesystems_list(fs_index);
    }
    while (ch == ' ') ch = DBGSerial.read();

    switch (command) {
    case '1':
      // view lots of info
      Serial.println("\nMTP internal filesystems info:");
      MTP.printFilesystemsInfo();
      Serial.println("\nIndex List:");
      MTP.printIndexList();
      Serial.println("\nFilesystems Selected with '2 {number}' command:");
      print_filesystem_list();
      break;
    case '2':
      if (myfs) {
        DBGSerial.printf("Filesystem #%u \"%s\" selected\n", fs_index, myfs->name());
      } else {
        DBGSerial.printf("Filesystem #%u does not exist :(\n", fs_index);
      }
      break;
    case 'l': listFiles(); break;
    case 'e': eraseFiles(); break;
    case 's':
      DBGSerial.println("\nLogging Data!!!");
      write_data = true;   // sets flag to continue to write data until new command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append data to
      // to the file created.
      dataFile = myfs->open("datalog.txt", FILE_WRITE);
      logData();
      break;
    case 'f':
      format3();
      break;
    case 'x': stopLogging(); break;
    case'r':
      DBGSerial.println("Reset");
      MTP.reset();
      break;
    case 'd': dumpLog(); break;
    case 'b':
      if (!myfs) { mysf_null_message(); break; }
      bigFile( 0 ); // delete
      command = 0;
      break;
    case 'B':
      if (!myfs) { mysf_null_message(); break; }
      bigFile( 1 ); // CREATE
      command = 0;
      break;
    case 't':
      if (!myfs) { mysf_null_message(); break; }
      bigFile2MB( 0 ); // CREATE
      command = 0;
      break;
    case 'S':
      if (!myfs) { mysf_null_message(); break; }
      bigFile2MB( 1 ); // CREATE
      command = 0;
     break;
    case 'n': // No Verify on write
      bWriteVerify = !bWriteVerify;
      bWriteVerify ? DBGSerial.print(" Write Verify on: ") : DBGSerial.print(" Write Verify off: ");
     command = 0;
      break;
    case 'i':
      if (!myfs) { mysf_null_message(); break; }
      writeIndexFile();
      break;
    case 'R':
      DBGSerial.print(" RESTART Teensy ...");
      delay(100);
      SCB_AIRCR = 0x05FA0004;
      break;
    case 'F':
      if (!myfs) { mysf_null_message(); break; }
      Entropy.Initialize();
      randomSeed(Entropy.random());
      benchmark();
      break;
    case 'w':
      if (!myfs) { mysf_null_message(); break; }
      test_write_file(ch);
      break;
    case 'W':
      if (!myfs) { mysf_null_message(); break; }
      test_write_file_with_packet_numbers(ch);
      break;
    case '\r':
    case '\n':
    case '?':
    case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
  }

  if (write_data) logData();
}


void mysf_null_message()
{
  Serial.println("This command accesses a filesystem, but none is selected.");
  Serial.println("Please use '2 {number}' to select a filesystem.");
  Serial.println();
}


void pullup_cs_pins()
{
#if USE_SW_PU == 1
#if USE_SPI == 1
  pinMode(USE_SPI_CSPIN, OUTPUT);
  digitalWriteFast(USE_SPI_CSPIN, HIGH);
#endif
#if USE_MEMBOARD4==1 || USE_MEMBOARD8==1
  pinMode(3, OUTPUT);
  digitalWriteFast(3, HIGH);
  pinMode(4, OUTPUT);
  digitalWriteFast(4, HIGH);
  pinMode(5, OUTPUT);
  digitalWriteFast(5, HIGH);
  pinMode(6, OUTPUT);
  digitalWriteFast(6, HIGH);
#endif
#if USE_MEMBOARD8==1
  pinMode(2, OUTPUT);
  digitalWriteFast(2, HIGH);
  pinMode(7, OUTPUT);
  digitalWriteFast(7, HIGH);
  pinMode(8, OUTPUT);
  digitalWriteFast(8, HIGH);
  pinMode(9, OUTPUT);
  digitalWriteFast(9, HIGH);
#endif
#endif // USE_SW_PU
}
