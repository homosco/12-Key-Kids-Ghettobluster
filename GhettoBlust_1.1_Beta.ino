/ Quelle: https://github.com/golesny/12mp3/blob/master/mp3player/mp3player.ino

#include <EEPROM.h>
#include <Arduino.h>  // for type definitions
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <OnewireKeypadAlt.h>
#include <SD.h>
#include <avr/pgmspace.h> //brauche ich nicht??

// OneWireKeypad Setup

byte KEYS[]={1,2,3,
             4,5,6,
             7,8,9,
             11,10,12};

OnewireKeypad <Print, 12 > KP2(Serial, KEYS, 4, 3, A0, 4700, 1000, 4700, ExtremePrec);

// uncomment to turn off DEBUG
#define DEBUG 1
// while coding you can force reindexing the SD card
#define FORCE_REINDEX false

#define MAX_ALBUMS 50 // will consume 2 bytes * MAX_ALBUMS of dynamic memory (Idea, if we need more mem: save offsets in a second fixed length file _OFFSETS on SD card)

// struct to persist the config in the EEPROM
struct state_t
{
  int track;
  int album;
  int maxAlbum;
  int idxLen;
  int albumOffsets[MAX_ALBUMS];
} state;

// strings
const char ERR_NO_VS1053[] PROGMEM          = " No VS1053";
const char ERR_BMP_NOT_RECOGNIZED[] PROGMEM = " BMP format not recognized.";
const char ERR_FILE_NOT_FOUND[] PROGMEM     = " File not found:";
const char ERR_NO_SD_CARD[] PROGMEM         = " SD not found!";
const char ERR_DREQ_PIN[] PROGMEM           = " DREQ not an interrupt!";
const char ERR_NO_FILES_FOUND[] PROGMEM     = " No files found ";
const char ERR_SEEK_FAILED[] PROGMEM        = " seek in file failed to pos ";
const char ERR_INDEX_FAILED[] PROGMEM       = " failed to create the index";

const char MSG_SETUP[] PROGMEM              = " setup";
const char MSG_SKIP_INDEX_CREATE[] PROGMEM  = " skipping index creation. /_IDX already exists.";
const char MSG_LOADING[] PROGMEM            = " Loading:";
const char MSG_DEBUG[] PROGMEM              = "DEBUG:";
const char MSG_FATAL[] PROGMEM              = "FATAL:";
const char MSG_FORCE_REINDEX[] PROGMEM      = " Force reindexing ";

const char MSG_LOOP_START[] PROGMEM         = "======";
const char MSG_PATH[] PROGMEM               = " path: ";
const char MSG_CLOSING[] PROGMEM            = " closing ";
const char MSG_IGNORING[] PROGMEM           = " Ignoring ";
const char ERR_OPEN_FILE[] PROGMEM          = " Could not open file ";
const char MSG_START_PLAY[] PROGMEM         = " Start playing ";
const char MSG_STOP_PLAY[] PROGMEM          = " Stop playing ";
const char MSG_BUTTON[] PROGMEM             = " Button: ";
const char MSG_NEW_INDEX[] PROGMEM          = " Creating new index ";
const char MSG_CLOSING_INDEX[] PROGMEM      = " Closing index ";
const char MSG_OPEN_FILE[] PROGMEM          = " :::: Open file :::: ";
const char MSG_CLOSE_FILE[] PROGMEM         = " Closing file ";
const char MSG_POS[] PROGMEM                = " Pos ";
const char MSG_AVAIL[] PROGMEM              = " Available ";
const char MSG_TRACK[] PROGMEM              = " Track ";
const char MSG_ALBUM[] PROGMEM              = " Album ";
const char MSG_OFFSET[] PROGMEM             = " Offset ";
const char MSG_MAXALBUM[] PROGMEM           = " Max Album ";
const char MSG_READ_TRACK[] PROGMEM         = " Reading Track ";
const char MSG_VOLUME[] PROGMEM             = " volume ";
const char MSG_SAVE_STATE[] PROGMEM         = " save state";
const char MSG_SHUTDOWN[] PROGMEM           = " shutdown";
const char MSG_ACTION[] PROGMEM             = " action ";


// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7       // VS1053 chip select pin (output)
#define SHIELD_DCS    6       // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin


// constants for actions

#define IDX_LINE_LENGTH 24
#define IDX_LINE_LENGTH_W_LF 26
#define VOLUME_MAX 25
#define VOLUME_INIT 50
#define VOLUME_MIN 60

#define DIG_IO_POWER_ON 5
#define GPIO_SHUTDOWN 5

int currentAlbum;
int currentTrack;
bool forceCoverPaint = true; // on start-up cover must be painted

char trackpath[IDX_LINE_LENGTH_W_LF];
char albumpath[IDX_LINE_LENGTH_W_LF];
File idxFile;
byte userAction = 0;

// the number of the pin that is used for the pushbuttons
const int buttonsPin = A0;

// the pin of the potentiometer that is used to control the volume
const int volumePin = A5;

// wait before next click is recognized
const int buttonPressedDelay = 5000;

// the current volume level, set to min at start
byte volumeState = 254;

byte buttonState = 0;   // not pressed(0), pressed(1), released(2) or held(3).

//pressedButton
byte pressedButton = 0;
// last button that was pressed
byte lastPressedButton = 0;
// is the last pressed button released
boolean released = true;
// remember if the back button was pressed last time
byte lastReleasedButton = 0;
// the time at the back button was pressed last time
long lastBackButtonTime = 0;

// create mp3 maker shield object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);




void setup() {
   digitalWrite(DIG_IO_POWER_ON, HIGH);
   pinMode(DIG_IO_POWER_ON, OUTPUT);

  #if defined(DEBUG)
    Serial.begin(9600);
    delay(100);
  #endif
  	 mp3player_dbg(__LINE__, MSG_SETUP);
	 pinMode(13, OUTPUT);
 	 digitalWrite(13, LOW);
  if (!SD.begin(CARDCS)) {
    mp3player_fatal(__LINE__, ERR_NO_SD_CARD);
  }

  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
    mp3player_fatal(__LINE__, ERR_NO_VS1053);
  }

  //musicPlayer.sineTest(0x44, 250);    // Make a tone to indicate VS1053 is working

  if (EEPROM_readAnything(0, state) > 0) {
    currentTrack = state.track;
    currentAlbum = state.album;
  } else {
    currentTrack = 0;
    currentAlbum = 0;
  }
  updateIndex();

     // setup pins

  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT)) {
    mp3player_fatal(__LINE__, ERR_DREQ_PIN);
  }
	musicPlayer.GPIO_pinMode(GPIO_SHUTDOWN, OUTPUT);
    musicPlayer.GPIO_digitalWrite(GPIO_SHUTDOWN, LOW);


  // open Index File
  idxFile = getIndexFile(FILE_READ);
  mp3player_dbgi(__LINE__, MSG_AVAIL, idxFile.available());
}

/**
   Main prog loop
*/
void loop() {
  mp3player_dbg(__LINE__, MSG_LOOP_START);

  // stop
  if (musicPlayer.playingMusic) {
    mp3player_dbg(__LINE__, MSG_STOP_PLAY, musicPlayer.currentTrack.name());
    musicPlayer.stopPlaying();
  }

  // play
  const char* trackpath = getCurrentTrackpath();
  mp3player_dbg(__LINE__, MSG_START_PLAY, trackpath);
  if (! musicPlayer.startPlayingFile(trackpath)) {
    // mp3player_fatal(__LINE__, ERR_OPEN_FILE, trackpath);
  }
  // Wenn das hier drin ist funktioniert pause und Play nicht. Folglich muss hier eine WEnnschleife drum, mit if Pause = false do…… if = true  skip
  // file is now playing in the 'background' so now's a good time
  // to do something else like handling LEDs or buttons :)
  waitForButtonOrTrackEnd();
  delay(250);
  saveState();
}

void saveState() {
  mp3player_dbg(__LINE__, MSG_SAVE_STATE);
  state.album = currentAlbum;
  state.track = currentTrack;
  EEPROM_writeAnything(0, state);
}

void shutdownNow() {
  	mp3player_dbg(__LINE__, MSG_SHUTDOWN);
	digitalWrite(DIG_IO_POWER_ON, LOW);
	musicPlayer.GPIO_digitalWrite(GPIO_SHUTDOWN, HIGH);
}

// checks the value of the potentiometer
// if it has changed by 2 then set the new volume
void checkVolume()
{
    // read the state of the volume potentiometer
    int read = analogRead(volumePin);

    // set the range of the volume from max=0 to min=254
    // (limit max volume to 20 and min to 80)
    byte state = map(read, 0, 1023, 10, 80);

    // recognize state (volume) changes in steps of two
    if (state < volumeState - 2 || state > volumeState + 2) //empfindlichkeit
    {
        // remember the new volume state
        volumeState = state;

        // set volume max=0, min=254
        musicPlayer.setVolume(volumeState, volumeState); //muss hier nicht auch nach dem Komma volumeState?

        // print out the state of the volume
       // Serial.print(volumePin); //comment for final
       // Serial.print(" volume "); //comment for final
       // Serial.println(volumeState); //comment for final
    }
}



void waitForButtonOrTrackEnd() {
 bool exitLoop = false;
  //unsigned long lastMultiButtonTS = 0;
  while ( ! exitLoop ) {
    //btCount = 0;
    checkVolume();
    byte pressedButton = KP2.Getkey();// checkButtons()
    // Serial  << __LINE__ << ": " << "Button: " << pressedButton;
    byte buttonState = KP2.Key_State();
    // check that button has been released
    if (pressedButton != 0) {
      bool exLoop = false;
        while (!exLoop) {
          if (buttonState != 2){
          buttonState = KP2.Key_State();
        } else {
        Serial << __LINE__ << ": " << "Taste: " << pressedButton << " !  ";
        lastPressedButton = pressedButton;
        // buttons released --> leave loop only if user action shall be executed
        handleUserAction(pressedButton);
        exitLoop = true;
        exLoop = true;
      }
      }
     }
     else if(!musicPlayer.playingMusic)
        {
          // play the next track
         currentTrack++;
         // if the album is at the end of an album we shutdown the player
         // next startup the player will start at the next album (that why we handleUserAction first)
         if (hasLastTrackReached(currentAlbum, currentTrack))
         {
           currentTrack = 0;
           saveState();
           //shutdownNow();
         }
        exitLoop = true;
        }
    else {
        delay(10);
    } // end special actions
  }
  mp3player_dbg(__LINE__, MSG_BUTTON, "loop end");
}

int getOffset(int albumNo, int lineNo) {
  return state.albumOffsets[albumNo] + IDX_LINE_LENGTH_W_LF * lineNo;
}

boolean hasLastTrackReached(int albumNo, int trackNo) {
  int offsetOfNextAlbum;
  if (albumNo == state.maxAlbum) {
    offsetOfNextAlbum = idxFile.size();
  } else {
    offsetOfNextAlbum = getOffset(albumNo + 1, 0);
  }
  int offset = getOffset(albumNo, trackNo + 1) + IDX_LINE_LENGTH_W_LF;
  mp3player_dbgi(__LINE__, MSG_OFFSET, offset);
  mp3player_dbgi(__LINE__, MSG_OFFSET, offsetOfNextAlbum);
  if (offset >= offsetOfNextAlbum) {
    return true;
  }
  return false;
}

void handleUserAction(byte action) {
    if (action < 10) ; //Changed from 11 to 10. Because Key 10 shall become Play and Pause Button.
    {
      currentAlbum = (action-1); /// Warum -1 Stimmt das????? ============ >Todo< stimmt wenn Alben von 0 nach 8 durchgezählt werden Ich glaube ja.
      currentTrack = 0;
      //playCurrent();
    }
    // if a function button is pressed:
    else if (action == 10) // if Button is 10 pause Song. if already paused. start playing again:
    {
        if (! musicPlayer.paused()) {
          Serial.println("Paused");
          musicPlayer.pausePlaying(true);
        }
        else {
          Serial.println("Resumed");
          musicPlayer.pausePlaying(false);
        }
      waitForButtonOrTrackEnd() //go back to loop waiting for key or end of Track.
    }
    else if (action == 11)
    {
      //musicPlayer.stopPlaying(); kommt eh.
      long times = millis();
      // this is the second press within 1 sec., so we
      // got to the previous track:
      Serial.print((times - lastBackButtonTime));
      if (lastPressedButton == 11 && ((times - lastBackButtonTime) < buttonPressedDelay))
      {
        currentTrack--;
        if (currentTrack < 0) {
          currentTrack = 0; //numberOfTracks[currentAlbum]
        }
        //playPrevious();
        Serial.println("Zurück Pressed--- fast again -> play previous");
        lastBackButtonTime = times;
      }
      else
      {
        //playCurrent();
        Serial.println("Zurück Pressed--- once -> play current from beginning");
        lastBackButtonTime = times;
      }

    }
    else if (action == 12)
    {
      currentTrack++;
      // fehtl noch korrektur wenn es Datei nicht gibt !!!!
      if (hasLastTrackReached(currentAlbum, currentTrack))
      {
        currentTrack = 0;
        Serial  << __LINE__ << ": " << "last track reached beginn from beginningn" << "\n";
      }
      else {
      Serial << __LINE__ << ": " << "Play Next" << "\n";
      }
    }
        mp3player_dbgi(__LINE__, MSG_ALBUM, currentAlbum);
        mp3player_dbgi(__LINE__, MSG_TRACK, currentTrack);
        saveState();
}

File getIndexFile(uint8_t mode) {
  return SD.open("/_IDX", mode);
}

// read path from index
void getIndexEntry(int albumNo, int trackNo, char* returnVal) {
  char s[IDX_LINE_LENGTH];
  int pos = getOffset(albumNo, trackNo);
  idxFile.seek(pos);
  if (idxFile.position() == -1) {
    mp3player_fatal(__LINE__, ERR_SEEK_FAILED, pos);
  } else {
    mp3player_dbgi(__LINE__, MSG_POS, idxFile.position());
  }
  idxFile.read(&s[0], IDX_LINE_LENGTH - 1);
  s[IDX_LINE_LENGTH - 1] = 0;
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  trim(s);
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  strcpy(returnVal, &s[0]);
}

const char* getCurrentTrackpath() {
  bool last = false;
  // first entry is the album path
  getIndexEntry(currentAlbum, 0, &trackpath[0]);
  // read track path
  getIndexEntry(currentAlbum, currentTrack + 1, &trackpath[0]);
  mp3player_dbgi(__LINE__, MSG_READ_TRACK, currentTrack);
  return &trackpath[0];
}

void updateIndex() {
  if (FORCE_REINDEX) {
    mp3player_dbg(__LINE__, MSG_FORCE_REINDEX);
    SD.remove("/_IDX");
  }
  // check if file must be recreated
  if ( FORCE_REINDEX != true && SD.exists("/_IDX") && state.idxLen > 0) {
    File idxFile = getIndexFile(FILE_READ);
    int len = idxFile.available();
    idxFile.close();
    if (len == state.idxLen) {
      // ok, saved len is equals (we assume that the file is ok)
      mp3player_dbg(__LINE__, MSG_SKIP_INDEX_CREATE);
      return;
    }
  }
  // reset state
  state.idxLen = 0;
  saveState();
  // start generation
  int len = 0;
  int offset = 0;
  int albumNo = -1;
  // create a new index
  mp3player_dbg(__LINE__, MSG_NEW_INDEX);
  File rootDir = SD.open("/");
  File fIdx = getIndexFile(FILE_WRITE | O_TRUNC);
  while (true) {
    File albumDir = rootDir.openNextFile();
    if (!albumDir) {
      // last file reached
      mp3player_dbg(__LINE__, MSG_CLOSING_INDEX, albumDir.name());
      break;
    }
    mp3player_dbg(__LINE__, MSG_OPEN_FILE, albumDir.name());
    if (albumDir.isDirectory() && strcmp(albumDir.name(), "SYSTEM~1") != 0) {
      // in rootDir each dir is an album
      boolean dirEntryWritten = false;
      while (true) {
        File track = albumDir.openNextFile();
        if (!track) {
          // last file reached
          break;
        }
        mp3player_dbg(__LINE__, MSG_TRACK, track.name());
        if (IsValidFileExtension(track.name())) {
            if (!dirEntryWritten) {
              // a directory must have at least 1 song to be indexed
              state.albumOffsets[++albumNo] = offset;
              mp3player_dbgi(__LINE__, MSG_ALBUM, albumNo);
              mp3player_dbgi(__LINE__, MSG_OFFSET, offset);
              // print album path
              fIdx.print('/');
              fIdx.print(albumDir.name());
              len = 1 + strlen(albumDir.name());
              while (len < IDX_LINE_LENGTH) {
                fIdx.print(' ');
                len++;
              }
              fIdx.println("");
              offset += IDX_LINE_LENGTH_W_LF;
              dirEntryWritten = true;
            }
          // all MP3s in album
          fIdx.print('/');
          fIdx.print(albumDir.name());
          fIdx.print('/');
          fIdx.print(track.name());
          len = 2 + strlen(albumDir.name()) + strlen(track.name());
          while (len < IDX_LINE_LENGTH) {
            fIdx.print(' ');
            len++;
          }
          fIdx.println("");
          offset += IDX_LINE_LENGTH_W_LF;
        } else {
          mp3player_dbg(__LINE__, MSG_IGNORING, track.name());
        }
        track.close();
      }
      mp3player_dbg(__LINE__, MSG_CLOSING, albumDir.name());
      albumDir.close();
    } else {
      mp3player_dbg(__LINE__, MSG_IGNORING, albumDir.name());
    }
  }
  // to check if update index process has been aborted we save the length of the file
  fIdx.seek(0);
  state.idxLen = fIdx.available();
  fIdx.close();
  rootDir.close();
  state.maxAlbum = albumNo;
  mp3player_dbgi(__LINE__, MSG_MAXALBUM, state.maxAlbum);
  if (state.idxLen == 0 || state.maxAlbum == -1) {
    mp3player_fatal(__LINE__, ERR_INDEX_FAILED);
  }
  saveState();
}

void mp3player_fatal(const int numb, const char msg[]) {
  mp3player_dbg(numb, msg);
  while (1);
}

void mp3player_fatal(const int numb, const char msg[], const char *param) {
  mp3player_dbg(numb, msg, param);
  while (1);
}

void mp3player_fatal(const int numb, const char msg[], long param) {
  mp3player_dbgi(numb, msg, param);
  while (1);
}

void mp3player_dbg(const int lineno, const char msg[]) {
#if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while ((c = pgm_read_byte(msg++))) { // alle chars lesen
    Serial.write(c);
  }
  Serial.write('\n');
#endif
}

void mp3player_dbg(const int lineno, const char msg[], const char *param) {
#if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while ((c = pgm_read_byte(msg++))) { // read all chars
    Serial.write(c);
  }
  Serial.println(param);
#endif
}

void mp3player_dbgi(const int lineno, const char msg[], long param) {
#if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while ((c = pgm_read_byte(msg++))) { // read all chars
    Serial.write(c);
  }
  Serial.println(param);
#endif
}

int EndsWith(const char *str, const char *suffix)
{
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix >  lenstr) {
    return 0;
  }
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int IsValidFileExtension(const char *str) {
  return EndsWith(str, ".MP3") || EndsWith(str, ".M4A");
}

/*//====== NEU VON MIR UM files mit _ auszusortieren
int BeginsWith(const String *stress, const String *prefix)
{
  if (stress.startsWith(prefix)) {
    return 1;
  }
  else {
    return 0;
  }
}

int IsUnValidFilePrefix(const String *stress) {
  return BeginsWith(stress, "_") || BeginsWith(stress, ".");
}

*/

size_t trim(char* s) {
  char* p = s;
  size_t len = strlen(p);
  while (isspace(p[len - 1])) p[--len] = 0;
  //while(* p && isspace(* p)) ++p, --len;

  //memmove(s, p, len + 1);
  return len;
}

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
  const byte* p = (const byte*)(const void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
  byte* p = (byte*)(void*)&value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}
