//#include "itoa.h"
#include "SdFat.h"
#include "FreeStack.h"

// **************** SD CARD
const uint8_t SD_CS_PIN = 4;
const char MP3DirPath[] = "/"; // MP3 path
SdFat audio_SD;
SdFile file;
SdFile dirFile;

void setup() {
  Serial.begin(9600);
  delay(100);
  while (!Serial);
  Serial.println("SD Card directory sorting...");
  if (!audio_SD.begin(SD_CS_PIN, SD_SCK_MHZ(50))) {
    Serial.println("SD card error!");
  }
  Serial.print(F("FreeStack: "));
  Serial.println(FreeStack());
  Serial.println();
}
int count = 0;
void loop() {
  getMP3dir() ;
  Serial.println(count);
  count++;
  Serial.print(F("FreeStack: "));
  Serial.println(FreeStack());
  Serial.println();
  delay(1000);
}

void getMP3dir() {
  int MAXFILES = 255; // MAX files in directory allowed
  int numberElementsInArray = 0; // helper
  int dirfilecounter = 0; // counter of directory files
  int dirIDlist[MAXFILES];
  char xstring[20]; // temp string
  char tempString[21]; // temp string
  memset (xstring, 0, sizeof(xstring));
  memset (tempString, 0, sizeof(tempString));
  memset (dirIDlist, 0, sizeof(dirIDlist));
  if (!dirFile.open(MP3DirPath, O_READ)) {
    Serial.print("open MP3-dir Path folder failed");
  }
  while ( file.openNext(&dirFile, O_READ) && dirfilecounter < MAXFILES) { // count  MP3 subfolders
    if (  !file.isHidden()) { // avoid hidden files
      dirfilecounter++;
    }
    file.close();
  }
  if (file.isOpen())
    file.close();
  dirFile.close();
  char *fileName[dirfilecounter + 1];
  dirFile.open(MP3DirPath, O_READ); // reopen dir (rewind doesn't work?)
  while ( file.openNext(&dirFile, O_READ) && numberElementsInArray < dirfilecounter) { // getting number of MP3 subfolders
    if (  !file.isHidden()) { // avoid hidden files
      file.getName(xstring, 20); // get only 20 chars from each file name
      // file.getSFN(xstring ); // better alternative get 8.3 filename? saves RAM
      dirIDlist[numberElementsInArray] = file.dirIndex();
      xstring[0] = toupper(xstring[0]); // upper case the first char
      sprintf(tempString, "%s", xstring);
      numberElementsInArray++;
      fileName[numberElementsInArray - 1] = (char *)malloc(21);
      sprintf(fileName[numberElementsInArray - 1], "%s", tempString);
    }
    file.close();
  }
  if (file.isOpen())
    file.close();
  dirFile.close();
  // sorting the file list (only first 20 chars)
  char *  temp;
  int temp2 = 0;
  for (int j = 0; j < numberElementsInArray - 1; j++)
  {
    for (int i = 0; i < numberElementsInArray - 1; i++)
    {
      if (strncmp(fileName[i], fileName[i + 1], 20) > 0)
      {
        temp = fileName[i];
        temp2 = dirIDlist[i];
        fileName[i] = fileName[i + 1];
        dirIDlist[i] = dirIDlist[i + 1];
        fileName[i + 1] = temp;
        dirIDlist[i + 1] = temp2;
      }
    }
  }
  // output file list
  if (!dirFile.open(MP3DirPath, O_READ)) {
    Serial.print("open MP3-dir Path folder failed");
  }
  for (int sortnumber = 0; sortnumber < numberElementsInArray ; sortnumber++) {
    file.open(&dirFile, dirIDlist[sortnumber], O_READ);
    if (sortnumber < 10)
      Serial.print("0");
    Serial.print(sortnumber);
    Serial.print(": ");
    file.printName(&Serial);
    Serial.println();
    file.close();
  }
  if (file.isOpen())
    file.close();
  dirFile.close();
  Serial.println();
  Serial.print  ("MP3 sub dir in Dir: ");
  Serial.println(dirfilecounter);
  for (int x = 0; x < numberElementsInArray; x++) {
    free(fileName[x]);
  }
}

