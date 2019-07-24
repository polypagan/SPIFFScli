#include <Streaming.h>  // my preferred method

#include "FS.h"
#include "SPIFFS.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define FORMAT_SPIFFS_IF_FAILED true

// simple inline functions
// syntax is case-insensitive, all uppercase internally
// filenames are case-sensitive, however...
inline char upcase(char c) { return c &= 0xdf; }

// true if space or tab, not if newline
inline boolean iswhite(char c) { return ((c == ' ') || (c == '\t')); }

// global variables
// parser states (X means eXpect):
enum {X0, X1, X2, X3 };
int state = X0; // could be in nextChar, global for debugging

const int MAXNAME = 14; // "/12345678.123\0"

char Name1[MAXNAME], Name2[MAXNAME];
char verb;
int nargs;

// routine for debugging
void trace(char *msg) {
  Serial << msg << "state:" << state << " verb:" << verb << " Name1:" << Name1 << " Name2:" << Name2 << endl;
}

// instruct user
void usage() {

  Serial << "L)ist" << endl
         << "R)emove <filename>" << endl
         << "M)ove <file1> <file2>" << endl
         << "D)ump <filename>" << endl
         << "C)p <file1> <file2>" << endl
         << "T)ouch <filename>" << endl;
}

// confirm action -- very carefully...
boolean confirm(const char *cmd, const char *name) {
char a = 'n';

  while (Serial.available() != 0) {
    Serial.read();                // empty the input
  }
  
  Serial << "Really " << cmd << " " << name << " (y/N)?" << endl;

  while (Serial.available() == 0) {
    yield();                      // wait for user to answer
  }
  a = Serial.read();              // read first char of answer

//  Serial << "Serial.read() returns:" << a << "(0x" << _HEX(a) << ")" << endl;
  
  while (Serial.available() != 0) {    // toss any cruft
      Serial.read();
  }
  
  if (upcase(a) == 'Y') return true;  // 'y' or 'Y' means yes,
  else return false;                  // anything else is a 'no'.
}

// file system operations
// most are lifted from SPIFFS_Test Example

// list the directory (root)
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }
    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\t\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
    root.close();   // don't we need this?    
}
void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

void touchFile(fs::FS &fs, const char *path) {

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    file.close();  
}

void copyFile(fs::FS &fs, const char *path1, const char *path2) {
  static uint8_t buf[512];
  size_t len = 0;
  size_t toRead;

  if (fs.exists(path2)) {
    if (! confirm("overwrite",path2)) return;
  }
  
  File file1 = fs.open(path1, FILE_READ);
  if (!file1) {
    Serial << "Failed to open " << path1 << " for reading..." << endl;
    return;
    }
  
  File file2 = fs.open(path2, FILE_WRITE);
  if(!file2){
    Serial << "Failed to open " << path2 << "  for writing" << endl;
    file1.close();
    return;
    }
  
   if(file2 && !file2.isDirectory()){
        len = file1.size();
        while(len){
            toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file1.read(buf, toRead);
            file2.write(buf, toRead);
            }
            len -= toRead;
        }
    file1.close();
    file2.close();

}
// here begineth the HexASCII dump code

// display printable ASCII character, or '.'
void ASCIIprint(uint8_t d) {

  if ((d > 0x1f) && (d < 0x80)) {
    Serial.printf("%c",d);
  } else {
    Serial.print('.');
  }
}

// print formatted line to screen
void printHexASCIIline(long *pos, uint8_t *buf, int number) {
int i;

  Serial.printf("%08x: ",*pos);

  for (i=0; i<number; i++) {
    Serial.printf("%02x ", buf[i]);
    if ((i == 7) || (i == 15))Serial.print(' ');   // extra space after 8th & 16th
    *pos = *pos + 1;                     // keep count (sidestep operator precedence)
  }

  if (i < 16) {               // this attempts to make the last line look right.
    for (i=(16-i); i; i--) {
      Serial.print("   ");          // pad "missing" points
    if ((i == 8) || (i == 0))Serial.print(' ');   // extra space after 8th & 16th
    }
  }
  
  Serial.print('|');
  for (i=0; i<number; i++) {
    ASCIIprint(buf[i]);
  }
  Serial.println('|');
}

// dump file as hex plus printable ASCII
void hexDump(fs::FS &fs, const char *path) {
uint8_t buf[16];
size_t len, toRead;
long position;

  File file = fs.open(path, FILE_READ);
  if (!file) {
    Serial << "Failed to open " << path << " for reading..." << endl;
    return;
    }

  position = 0L;
  
  len = file.size();

  while (len) {
    toRead = len;
    if (toRead > 16)toRead = 16;

    file.read(buf, toRead);

    printHexASCIIline(&position, buf, toRead);

    len -= toRead;
  }
  file.close();
}
void doCmd() {
boolean b;

//  trace("doCmd()");
  
  switch (verb) {
    case 'L' :    // L)ist directory
    case 'l' :
//      Serial << "ls (nargs:" <<  nargs << ")" << endl;
      listDir(SPIFFS, "/", 0);
      break;

    case 'R' :    // R)emove file
    case 'r' :
      Serial << "rm " << Name1 << endl;
      if (nargs < 1) {
        Serial << "No filename!" << endl;
        break;
      }
      b = confirm("rm ", Name1);
      Serial << "confirm() returns:" << b << endl;
      if (b) {
        deleteFile(SPIFFS, Name1);
      }
      break;

    case 'M' :    // M)ove (rename) file
    case 'm' :
      Serial << "mv " << Name1 << " " << Name2 << endl;
      if (nargs < 2) {
        Serial << "Need two filenames!" << endl;
        break;
      }
        renameFile(SPIFFS, Name1, Name2);
      break;

    case 'D' :    // D)ump file in hex & ASCII
    case 'd' :
      Serial << "hexDump " << Name1 << endl;
      if (nargs < 1) {
        Serial << "Need file name!" << endl;
        break;
      }
      hexDump(SPIFFS, Name1);
      break;

    case 'C' :    // C)opy file1 to file2
    case 'c' :
      Serial << "cp " << Name1 << " " << Name2 << endl;
      if (nargs < 2) {
        Serial << "Need both from and to filenames!" << endl;
        break;
      }
        copyFile(SPIFFS, Name1, Name2);
      break;

    case 'T' :    // T)ouch file
    case 't' :
      Serial << "touch " << Name1 << endl;
      if (nargs < 1) {
        Serial << "Need name of file to touch!" << endl;
        break;
      }
        touchFile(SPIFFS, Name1);
      break;

    default :
//      Serial << "Verb is " << verb << "(0x" << _HEX(verb) << ")" << endl;
      usage();
      break;
  }
}

// add char (unless it's '/') to array of char if there's space, terminate
void pack(char *buf, char c) {
  int i;

//  trace("pack()");
  if (c != '/') {
    for (i=0; i < MAXNAME; i++) {
      if (buf[i] == '\0') break;
    }
  
//  Serial << "i:" << i;
    if (i < (MAXNAME-1)) {
      buf[i++] = c;
      buf[i] = '\0';
    }
//  Serial << " buf:" << buf << endl;
  }
}

// parse series of characters
void nextChar(char c) {
//static int state = X0;  // todo: put this back (make static local)

// todo: init names with '/', make pack() ignore '/'

//trace("nextChar()");
//Serial << "c:" << c << "(0x" << _HEX(c) << ") state:" << state << endl;

  switch (state) {
    case X0 :                     // start a new line
      Name1[0] = Name2[0] = '/';
      Name1[1] = Name2[1] = '\0'; // init file name prefixes
      nargs = 0;                  // no arguments yet
      if (isalpha(c)) {           // get verb
        verb = c;
        state = X1;               // ignore until terminator
      }
      break;

    case X1 :                     // wait for terminator
     if (c == '\n') {
     doCmd();                     // execute it if possible
     state = X0;                  // loop back for more.     }
     } 
     else if (iswhite(c)) {       // terminator (whitespace)
        state = X2;
     }
     break;

   case X2 :                      // handle first filename
     if (c == '\n') {
     doCmd();                     // execute it if possible
     state = X0;                  // loop back for more.     } 
     }
     else if (iswhite(c)) {       // terminator, look for more
      state = X3;
     } else {
     pack(Name1, c);              // add character to name if there's room
     nargs = 1;
     }
     break;

   case X3 :                      // handle second name
    if ((c == '\n') || (iswhite(c))) {
      doCmd();                      // execute it if possible
      state = X0;
    } else {                         // loop back for more.    
      pack(Name2, c);               // add character to name if there's room
      nargs = 2;
    }
    break;

  default :
    Serial << "State fucked up!" << endl;
    state = X0;
    break;
    
  }

}
void setup() {
  Serial.begin(115200);
  Serial << endl << endl << "Reset... SPIFFS cli" << endl;

    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
    while (1) { yield(); }  // await next reset...
    }
  
  Serial << "SPIFFS mounted!" << endl;
  
  usage();
}

// fetch characters from serial and process them
void loop() {
  char c;
  
  if (Serial.available() != 0) {  // if there is a character in the buffer,
    c = Serial.read();            // get it.
    
    if (c == '\r') c = '\n';

    nextChar(c);                  // feed it to the parser.
    
  } // end if (Serial.available())
  // **
  // if you have something else to do, do it here.
  // **
}
