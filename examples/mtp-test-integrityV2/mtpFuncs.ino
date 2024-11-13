
int ReadAndEchoSerialChar() {
  int ch = DBGSerial.read();
  if (ch >= ' ') DBGSerial.write(ch);
  return ch;
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
  DBGSerial.println("\t2# - Select Drive # for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\tF - Benchmark device (Good for Flash)");
  DBGSerial.println("\te - Erase files on disk with Format");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tw [write size] [size] [file name]");
  DBGSerial.println("\tW [write size] [size] [file name] - with packet index");
  DBGSerial.println("\tr - Reset ");
  DBGSerial.printf("\n\t%s","R - Restart Teensy");
  DBGSerial.printf("\n\t%s","i - Write Index File to disk");
  DBGSerial.printf("\n\t%s","'B, or b': Make Big file half of free space, or remove all Big files");
  DBGSerial.printf("\n\t%s","'S, or t': Make 2MB file , or remove all 2MB files");
  DBGSerial.printf("\n\t%s","'n' No verify on Write- TOGGLE");

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

void eraseFiles()
{
  //DBGSerial.println("Formating not supported at this time");
  DBGSerial.println("\n*** Erase/Format started ***");
  myfs->format(1, '.', DBGSerial);
  Serial.println("Completed, sending device reset event");
  MTP.reset();
}

void format3()
{
  //DBGSerial.println("Formating not supported at this time");
  DBGSerial.println("\n*** Erase/Format Unused started ***");
  myfs->format(2,'.', DBGSerial );
  Serial.println("Completed, sending device reset event");
  MTP.reset();
}

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


uint64_t CommandLineReadNextNumber(int &ch, uint64_t default_num) {
  while (ch == ' ') ch = DBGSerial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint64_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = DBGSerial.read();
  }
  return return_value;
}



void readVerify( char szPath[], char chNow ) {
  uint32_t timeMe = micros();
  file3 = myfs->open(szPath);
  if ( 0 == file3 ) {
    Serial.printf( "\tV\t Fail File open %s\n", szPath );
    errsLFS++;
  }
  char mm;
  char chNow2 = chNow + lowOffset;
  uint32_t ii = 0;
  while ( file3.available() ) {
    file3.read( &mm , 1 );
    rdCnt++;
    //Serial.print( mm ); // show chars as read
    ii++;
    if ( 0 == (ii / lowShift) % 2 ) {
      if ( chNow2 != mm ) {
        Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow2, mm, mm, ii );
        errsLFS++;
        break;
      }
    }
    else {
      if ( chNow != mm ) {
        Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
        errsLFS++;
        break;
      }
    }
  }
  Serial.printf( "  Verify %u Bytes ", ii );
  if (ii != file3.size()) {
    Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu", ii, file3.size() );
    errsLFS++;
  }
  file3.close();
  timeMe = micros() - timeMe;
  Serial.printf( " @KB/sec %5.2f", ii / (timeMe / 1000.0) );
}

bool bigVerify( char szPath[], char chNow ) {
  uint32_t timeMe = micros();
  file3 = myfs->open(szPath);
  uint64_t fSize;
  if ( 0 == file3 ) {
    return false;
  }
  char mm;
  uint32_t ii = 0;
  uint32_t kk = file3.size() / 50;
  fSize = file3.size();
  Serial.printf( "\tVerify %s bytes %llu : ", szPath, fSize );
  while ( file3.available() ) {
    file3.read( &mm , 1 );
    rdCnt++;
    ii++;
    if ( !(ii % kk) ) Serial.print('.');
    if ( chNow != mm ) {
      Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
      errsLFS++;
      break;
    }
    if ( ii > fSize ) { // catch over length return makes bad loop !!!
      Serial.printf( "\n\tFile LEN Corrupt!  FS returning over %u bytes\n", fSize );
      errsLFS++;
      break;
    }
  }
  if (ii != file3.size()) {
    Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu\n", ii, file3.size() );
    errsLFS++;
  }
  else
    Serial.printf( "\tGOOD! >>  bytes %lu", ii );
  file3.close();
  timeMe = micros() - timeMe;
  Serial.printf( "\n\tBig read&compare KBytes per second %5.2f \n", ii / (timeMe / 1000.0) );
  if ( 0 == ii ) return false;
  return true;
}



void bigFile( int doThis ) {
  char myFile[] = "/0_bigfile.txt";
  char fileID = '0' - 1;
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};

  if ( 0 == doThis ) {  // delete File
    Serial.printf( "\nDelete with read verify all #bigfile's\n");
    do {
      fileID++;
      myFile[1] = fileID;
      if ( myfs->exists(myFile) && bigVerify( myFile, fileID) ) {
        filecount--;
        myfs->remove(myFile);
      }
      else break; // no more of these
    } while ( 1 );
  }
  else {  // FILL DISK
    uint32_t resW = 1;
    
    char someData[MYBLKSIZE];
    uint64_t xx, toWrite;
    toWrite = (myfs->totalSize()) - myfs->usedSize();
    if ( toWrite < 65535 ) {
      Serial.print( "Disk too full! DO :: reformat");
      return;
    }
    if ( size_bigfile < toWrite *2 )
      toWrite = size_bigfile;
    else 
      toWrite/=2;
    toWrite -= SLACK_SPACE;
    xx = toWrite;
    Serial.printf( "\nStart Big write of %llu Bytes", xx);
    uint32_t timeMe = millis();
    file3 = nullptr;
    do {
      if ( file3 ) file3.close();
      fileID++;
      myFile[1] = fileID;
      file3 = myfs->open(myFile, FILE_WRITE);
    } while ( fileID < '9' && file3.size() > 0);
    if ( fileID == '9' ) {
      Serial.print( "Disk has 9 halves 0-8! DO :: b or q or F");
      return;
    }
    memset( someData, fileID, MYBLKSIZE );
    uint64_t hh = 0;
    uint64_t kk = toWrite/MYBLKSIZE/60;
    while ( toWrite > MYBLKSIZE && resW > 0 ) {
      resW = file3.write( someData , MYBLKSIZE );
      hh++;
      if ( !(hh % kk) ) Serial.print('.');
      toWrite -= MYBLKSIZE;
    }
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    timeMe = millis() - timeMe;
    file3 = myfs->open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %llu Bytes : file3.size()=%llu", myFile , timeMe / 1000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1.0) );
    Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs->usedSize(), myfs->totalSize());
    if ( myfs->usedSize() == myfs->totalSize() ) {
      Serial.printf("\n\n\tWARNING: DISK FULL >>>>>  Bytes Used: %llu, Bytes Total:%llu\n\n", myfs->usedSize(), myfs->totalSize());
      warnLFS++;
    }
    if ( resW < 0 ) {
      Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
      errsLFS++;
      myfs->remove(myFile);
    }
  }
}

void bigFile2MB( int doThis ) {
  char myFile[] = "/0_2MBfile.txt";
  char fileID = '0' - 1;
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};

  if ( 0 == doThis ) {  // delete File
    Serial.printf( "\nDelete with read verify all #bigfile's\n");
    do {
      fileID++;
      myFile[1] = fileID;
      if ( myfs->exists(myFile) && bigVerify( myFile, fileID) ) {
        filecount--;
        myfs->remove(myFile);
      }
      else break; // no more of these
    } while ( 1 );
  }
  else {  // FILL DISK
    uint32_t resW = 1;
    
    char someData[2048];
    uint32_t xx, toWrite;
    toWrite = 2048 * 1000;
    if ( toWrite > (65535 + (myfs->totalSize() - myfs->usedSize()) ) ) {
      Serial.print( "Disk too full! DO :: q or F");
      return;
    }
    xx = toWrite;
    Serial.printf( "\nStart Big write of %u Bytes", xx);
    uint32_t timeMe = micros();
    file3 = nullptr;
    do {
      if ( file3 ) file3.close();
      fileID++;
      myFile[1] = fileID;
      file3 = myfs->open(myFile, FILE_WRITE);
    } while ( fileID < '9' && file3.size() > 0);
    if ( fileID == '9' ) {
      Serial.print( "Disk has 9 files 0-8! DO :: b or q or F");
      return;
    }
    memset( someData, fileID, 2048 );
    int hh = 0;
    while ( toWrite >= 2048 && resW > 0 ) {
      resW = file3.write( someData , 2048 );
      hh++;
      if ( !(hh % 40) ) Serial.print('.');
      toWrite -= 2048;
    }
    xx -= toWrite;
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    timeMe = micros() - timeMe;
    file3 = myfs->open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
    Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs->usedSize(), myfs->totalSize());
    if ( myfs->usedSize() == myfs->totalSize() ) {
      Serial.printf("\n\n\tWARNING: DISK FULL >>>>>  Bytes Used: %llu, Bytes Total:%llu\n\n", myfs->usedSize(), myfs->totalSize());
      warnLFS++;
    }
    if ( resW < 0 ) {
      Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
      errsLFS++;
      myfs->remove(myFile);
    }
  }
}

void writeIndexFile() 
{
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};
  // open the file.
  Serial.println("Write Large Index File");
  uint32_t timeMe = micros();
  file3 = myfs->open("LargeIndexedTestfile.txt", FILE_WRITE_BEGIN);
  if (file3) {
    file3.truncate(); // Make sure we wipe out whatever was written earlier
    for (uint32_t i = 0; i < 43000*4; i++) {
      memset(write_buffer, 'A'+ (i & 0xf), sizeof(write_buffer));
      file3.printf("%06u ", i >> 2);  // 4 per physical buffer
      file3.write(write_buffer, i? 120 : 120-12); // first buffer has other data...
      file3.printf("\n");
      if ( !(i % 1024) ) Serial.print('.');

    }
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    
    timeMe = micros() - timeMe;
    file3 = myfs->open("LargeIndexedTestfile.txt", FILE_WRITE);
    if ( file3.size() > 0 ) {
       Serial.printf( " Total time to write %d byte: %5.2f seconds\n", file3.size(), (timeMe / 1000.0));
       Serial.printf( "\n\tBig write KBytes per second %5.2f \n", file3.size() / (timeMe / 1000.0) );
    }
    if ( file3 != 0 ) file3.close();
    Serial.println("\ndone.");
    
  }
}

void benchmark() {
  unsigned long buf[1024];
  for(uint8_t i = 0; i<10; i++){
    myFile = myfs->open("WriteSpeedTest.bin", FILE_WRITE_BEGIN);
    if (myFile) {
      const int num_write = 128;
      Serial.printf("Writing %d byte file... ", num_write * 4096);
      elapsedMillis t=0;
      for (int n=0; n < num_write; n++) {
        for (int i=0; i<1024; i++) buf[i] = random();
        myFile.write(buf, 4096);
      }
      myFile.close();
      int ms = t;
      DBGSerial.printf(" %d ms, bandwidth = %d bytes/sec", ms, num_write * 4096 * 1000 / ms);
      myfs->remove("WriteSpeedTest.bin");
    }
    DBGSerial.println();
    delay(2000);
  }
  DBGSerial.println("Bandwidth test finished");
}

#if 0
const char *getFSPN(uint32_t ii) {
// media name is now handled internally by MTP - this is no longer needed
  return "";
}
#endif

char test_file_name[128] = "write_test_file.txt";
uint32_t test_file_write_size = 512;
uint64_t test_file_size = 1048576; // 1 megabyte
uint8_t test_file_buffer[1024*16];

void test_write_file(int ch) {
  test_file_write_size = CommandLineReadNextNumber(ch, test_file_write_size);
  test_file_size = CommandLineReadNextNumber(ch, test_file_size);
  if (ch >= ' ') {
    Serial.setTimeout(0);
    /*int cb = */ Serial.readBytesUntil( '\n', test_file_name, sizeof(test_file_name));
  }
  File testFile; // Specifes that dataFile is of File type

  myfs->remove(test_file_name);  // try to remove files before as to force it to reallocate the file...
  
  testFile = myfs->open(test_file_name, FILE_WRITE_BEGIN);
  if (!testFile) {
    Serial.printf("Failed to open %s\n", test_file_name);
    return;
  }
  Serial.printf("Start write file: %s length:%llu Write Size:%u\n", test_file_name, test_file_size, test_file_write_size);  

  uint32_t cb_write = test_file_write_size;
  uint64_t cb_left = test_file_size;
  uint8_t fill_char = '0';
  test_file_buffer[cb_write - 2] = '\n'; // make in to text...
  uint32_t percent_left_prev = (cb_left * 100ul) / test_file_size; 
  elapsedMillis em;
  while (cb_left) {
    if (cb_left < cb_write) cb_write = cb_left;
    memset(test_file_buffer, cb_write - 1, fill_char);
    size_t cb_written = testFile.write(test_file_buffer, cb_write);
    if (cb_written != cb_write) {
        uint64_t cb_output = test_file_size - cb_left;
        Serial.printf("\n!Write failed cb left:%llu  output:%llu(0x%llx)\n", cb_left, cb_output, cb_output);
        break;
    }
    fill_char = (fill_char == '9')? '0' : fill_char + 1;
    cb_left -= cb_write;
    uint32_t percent_left = (cb_left * 100ul) / test_file_size;
    while (percent_left < percent_left_prev) {
        percent_left_prev--;
        Serial.write('.');
    }

  }  
  testFile.close();
  Serial.printf("\nTime to write: %u\n", (uint32_t)em);
  
}

//-----------------------------------------------------

uint8_t packet_buffer[512] = "0000000000 - ";
void test_write_file_with_packet_numbers(int ch) {
  test_file_write_size = CommandLineReadNextNumber(ch, test_file_write_size);
  test_file_size = CommandLineReadNextNumber(ch, test_file_size);
  if (ch >= ' ') {
    Serial.setTimeout(0);
    /* int cb = */ Serial.readBytesUntil( '\n', test_file_name, sizeof(test_file_name));
  }
  File testFile; // Specifes that dataFile is of File type

  myfs->remove(test_file_name);  // try to remove files before as to force it to reallocate the file...
  
  testFile = myfs->open(test_file_name, FILE_WRITE_BEGIN);
  if (!testFile) {
    Serial.printf("Failed to open %s\n", test_file_name);
    return;
  }

  Serial.printf("Start write packet indexed file: %s length:%llu Write Size:%u\n", test_file_name, test_file_size, test_file_write_size);  

  uint32_t cb_write = test_file_write_size;
  uint64_t cb_left = test_file_size;
  uint16_t packet_size = 500;  // first packet is 500 bytes.

  uint8_t fill_char = 'A';
  memset(packet_buffer, '0', 10);
  for (int i = 13; i < 512; i++) {
    packet_buffer[i] = fill_char;
    fill_char = (fill_char == 'Z')? 'a' : (fill_char == 'z')? '0' : (fill_char == '9')? 'A' : fill_char + 1;
    if (fill_char == 127) fill_char = '!';
  }
  
  fill_char = packet_buffer[packet_size-1];
  packet_buffer[511] = '\n';
  Serial.println((char*)packet_buffer);
  packet_buffer[packet_size-1] = '\n';
  test_file_buffer[cb_write - 2] = '\n'; // make in to text...
  uint32_t percent_left_prev = (cb_left * 100ul) / test_file_size; 
  elapsedMillis em;
  uint32_t packet_number = 0;
  uint32_t packet_index = 0xfffffffful;
  uint32_t buffer_index = 0;

 if (cb_left < cb_write) cb_write = cb_left;

  while (cb_left--) {
    if (packet_index >= packet_size) {
        packet_number++;
        char num_buf[11];
        int ch_num = sprintf(num_buf, "%lu", packet_number);
        memcpy(&packet_buffer[10-ch_num], num_buf, ch_num);
        packet_index = 0;
        if (packet_number == 2) {
            packet_buffer[packet_size-1] = fill_char; // restore 
            packet_size = 512;
            Serial.println((char*)packet_buffer);
          } 
    }
    test_file_buffer[buffer_index++] = packet_buffer[packet_index++];
    if (buffer_index == cb_write) {
        if (cb_write != test_file_write_size )  packet_buffer[cb_write - 1] = '\n';
        size_t cb_written = testFile.write(test_file_buffer, cb_write);
        if (cb_written != cb_write) {
            uint64_t cb_output = test_file_size - cb_left;
            Serial.printf("\n!Write failed cb left:%llu  output:%llu(0x%llx)\n", cb_left, cb_output, cb_output);
            break;
        }
         if (cb_left < cb_write) cb_write = cb_left;
         buffer_index = 0;
        uint32_t percent_left = (cb_left * 100ul) / test_file_size;
        while (percent_left < percent_left_prev) {
            percent_left_prev--;
            Serial.write('.');
        }
        }
  }
 testFile.close();
  Serial.printf("\nTime to write: %u\n", (uint32_t)em);
  
}


// MTP no longer exposes internal "store" or StorageID numbers.
// To implement a list of filesystem the user can choose, we
// must keep the list.

void add_to_filesystems_list(FS &fs, const char *extra_info) {
  if (filesystems_list_count >= sizeof(filesystems_list)/sizeof(FS *)) return;
  unsigned int index = filesystems_list_count++;
  filesystems_list[index] = &fs;
  filesystems_extra_info[index] = extra_info;
}

void add_to_filesystems_list(FS &fs) {
  add_to_filesystems_list(fs, nullptr);
}

FS *get_from_filesystems_list(unsigned int index) {
  if (index >= filesystems_list_count) return nullptr;
  return filesystems_list[index];
}

void print_filesystem_list() {
  for (unsigned int i=0; i < filesystems_list_count; i++) {
    FS *fs = filesystems_list[i];
    float size = (float)fs->totalSize() / 1024.0f;
    const char *unit = "kB";
    if (size > 1024.0f) {
      size = size / 1024.0f;
      unit = "MB";
    }
    if (size > 1024.0f) {
      size = size / 1024.0f;
      unit = "GB";
    }
    Serial.printf(" FS #%u  Present=%s  Size=%.0f %s  Name=%s", i,
      (fs->mediaPresent() ? "Yes" : "No "),
      size, unit,
      fs->name());
    const char *extra_info = filesystems_extra_info[i];
    if (extra_info) {
      Serial.printf("  Info=%s", extra_info);
    }
    Serial.println();
  }
}


