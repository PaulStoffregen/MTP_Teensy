/*
  LittleFS  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include <MTP_Teensy.h>
#include <USBHost_t36.h>

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

// Add in MTPD objects

// Add USBHost objectsUsbFs
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub(myusb);

// MSC objects.
USBDrive drive1(myusb);
USBDrive drive2(myusb);
USBDrive drive3(myusb);

USBFilesystem msFS1(myusb);
USBFilesystem msFS2(myusb);
USBFilesystem msFS3(myusb);
USBFilesystem msFS4(myusb);
USBFilesystem msFS5(myusb);

// Quick and dirty
USBFilesystem *pmsFS[] = {&msFS1, &msFS2, &msFS3, &msFS4, &msFS5};
const int nfs_usb = sizeof(pmsFS)/sizeof(USBFilesystem *);
char usb_filesystem_list_display_name[nfs_usb][20];


#define CNT_MSC  (sizeof(pmsFS)/sizeof(pmsFS[0]))
uint32_t pmsfs_store_ids[CNT_MSC] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
char  pmsFS_display_name[CNT_MSC][20];

USBDrive *pdrives[] {&drive1, &drive2, &drive3};
#define CNT_DRIVES  (sizeof(pdrives)/sizeof(pdrives[0]))
bool drive_previous_connected[CNT_DRIVES] = {false, false, false};

FS *mscDisk;

#include <LittleFS.h>
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;

#include "BogusFS.h"
BogusFS bogusfs;


#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif


void setup() {
  pinMode(5, OUTPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);

  // startup mtp.. SO to not timeout...
  MTP.begin();
// lets initialize a RAM drive.
#if defined ARDUINO_TEENSY41
  if (external_psram_size)
    LFSRAM_SIZE = 4 * 1024 * 1024;
#endif
  if (lfsram.begin(LFSRAM_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    // FIXME: MTP.addFilesystem() no longer returns internal store numbers
    uint32_t istore = MTP.addFilesystem(lfsram, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  mscDisk = &lfsram;  // so we don't start of with NULL pointer

  MTP.addFilesystem(bogusfs, "Bogus");

  Serial.println("\nInitializing USB MSC drives...");
  myusb.begin();
  for (int i=0; i < nfs_usb; i++) {
    snprintf(usb_filesystem_list_display_name[i], 20, "USB Disk %d", i + 1);
    MTP.addFilesystem(*usb_filesystem_list[i], usb_filesystem_list_display_name[i]);
  }

  Serial.println("MSC and MTP initialized.");

  menu();
}

void loop() {
  myusb.Task();
  MTP.loop();

  if (Serial.available()) {
    uint8_t command = Serial.read();
    int ch = Serial.read();
    while (ch == ' ')
      ch = Serial.read();

    switch (command) {
    case 'l':
      listFiles();
      break;
    case 'e':
      eraseFiles();
      break;
    case 's': {
      Serial.println("\nLogging Data!!!");
      write_data = true; // sets flag to continue to write data until new
      // command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append
      // data to
      // to the file created.
      dataFile = mscDisk->open("datalog.txt", FILE_WRITE);
      logData();
    } break;
    case 'x':
      stopLogging();
      break;
    case '1': 
      // first dump list of storages:
      MTP.printFilesystemsInfo();
      //Serial.println("\nIndex List");
      //MTP.printIndexList();
      break;
    case '2':
      uint32_t drive_index = CommandLineReadNextNumber(ch, 0);
      Serial.printf("Drive # %d Selected\n", drive_index);
      mscDisk = MTP.getFilesystemByIndex(drive_index);
      break;
    case 'd':
      dumpLog();
      break;
    case 'r':
      Serial.println("Send Device Reset Event");
      MTP.reset();
      break;
    case 'w':
      test_write_file(ch);
      break;  
    default:
      menu();
      break;
    }
    while (Serial.read() != -1)
      ; // remove rest of characters.
  }

  if (write_data)
    logData();
}

void logData() {
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
    Serial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging() {
  Serial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  Serial.printf("Records written = %d\n", record_count);
  MTP.reset();
}

void dumpLog() {
  Serial.println("\nDumping Log!!!");
  // open the file.
  dataFile = mscDisk->open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}


void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List USB Drives (Step 1)");
  Serial.println("\t2 - Select USB Drive for Logging (Step 2)");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
  Serial.println("\ts - Start Logging data (Restarting logger will append "
                 "records to existing log)");
  Serial.println("\tx - Stop Logging data");
  Serial.println("\td - Dump Log");
  Serial.println("\tw [write size] [size] [file name]");
  Serial.println("\tr - reset MTP");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles() {
  Serial.print("\n Space Used = ");
  Serial.println(mscDisk->usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(mscDisk->totalSize());

  File root = mscDisk->open("/");
  printDirectory(root, 0);
  root.close();
}

void eraseFiles() {
  /*
    PFsVolume partVol;
    if (!partVol.begin(sdx[1].sdfs.card(), true, 1)) {
      Serial.println("Failed to initialize partition");
      return;
    }
    if (pfsLIB.formatter(partVol)) {
      Serial.println("\nFiles erased !");
      MTP.reset();
    }
    */
}

void printDirectory(File dir, int numSpaces) {
  DateTimeFields dtf;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // Serial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    Serial.print(entry.name());
    printSpaces(36 - numSpaces - strlen(entry.name()));

    if (entry.getCreateTime(dtf)) {
      Serial.printf(" C: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
    }

    if (entry.getModifyTime(dtf)) {
      Serial.printf(" M: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
    }
    if (entry.isDirectory()) {
      Serial.println("  /");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}

uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ')
    ch = Serial.read();
  if ((ch < '0') || (ch > '9'))
    return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = Serial.read();
  }
  return return_value;
}

char test_file_name[128] = "write_test_file.txt";
uint32_t test_file_write_size = 512;
uint32_t test_file_size = 1048576; // 1 megabyte
uint8_t test_file_buffer[1024*16];

void test_write_file(int ch) {
  test_file_write_size = CommandLineReadNextNumber(ch, test_file_write_size);
  test_file_size = CommandLineReadNextNumber(ch, test_file_size);
  if (ch >= ' ') {
    Serial.setTimeout(0);
    int cb = Serial.readBytesUntil( '\n', test_file_name, sizeof(test_file_name));
  }
  File testFile; // Specifes that dataFile is of File type

  mscDisk->remove(test_file_name);  // try to remove files before as to force it to reallocate the file...
  
  testFile = mscDisk->open(test_file_name, FILE_WRITE_BEGIN);
  if (!testFile) {
    Serial.printf("Failed to open %s\n", test_file_name);
    return;
  }
  Serial.printf("Start write file: %s length:%u Write Size:%u\n", test_file_name, test_file_size, test_file_write_size);  

  uint32_t cb_write = test_file_write_size;
  uint32_t cb_left = test_file_size;
  uint8_t fill_char = '0';
  test_file_buffer[cb_write - 2] = '\n'; // make in to text...

  elapsedMillis em;
  while (cb_left) {
    if (cb_left < cb_write) cb_write = cb_left;
    memset(test_file_buffer, cb_write - 1, fill_char);
    testFile.write(test_file_buffer, cb_write);
    fill_char = (fill_char == '9')? '0' : fill_char + 1;
    cb_left -= cb_write;
  }  
  testFile.close();
  Serial.printf("Time to write: %u\n", (uint32_t)em);
  
}

