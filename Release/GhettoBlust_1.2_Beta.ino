#include <EEPROM.h>
#include <Arduino.h>  // for type definitions
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <OnewireKeypadAlt.h>
#include <SD.h>
//#include <avr/pgmspace.h> //brauche ich nicht??
// Quelle: https://github.com/golesny/12mp3/blob/master/mp3player/mp3player.ino
// OneWireKeypad Setup

char KEYS[] = {1, 2, 3,
               4, 5, 6,
               7, 8, 9,
               11, 10, 12
              };

OnewireKeypad <Print, 12> KP2(Serial, KEYS, 4, 3, A0, 4700, 1000, 4700, ExtremePrec);

// uncomment to turn off DEBUG
#define DEBUG 0
// while coding you can force reindexing the SD card
#define FORCE_REINDEX false

#define MAX_ALBUMS 50 // was 50 - 50 will consume 2 bytes * MAX_ALBUMS of dynamic memory (Idea, if we need more mem: save offsets in a second fixed length file _OFFSETS on SD card)

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
const char MSG_KEYLEVEL[] PROGMEM           = "Keylevel is:";



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

byte currentAlbum; //war int. Wenn byte nicht funkt zurück
byte currentTrack;
//bool forceCoverPaint = true; // on start-up cover must be painted

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
byte keylevel = 0;  //how often key is repeated.

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
// The time the last Button was pressed
long lastButtonTime;
long times = 0;

// create mp3 maker shield object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

//Function Prototypes setting prototypes for voids because got error emssages:
void mp3player_dbg(const int lineno, const char msg[]);
void mp3player_dbg(const int lineno, const char msg[], const char *param);
void mp3player_dbgi(const int lineno, const char msg[], long param);
void mp3player_fatal(const int numb, const char msg[], long param);
void mp3player_fatal(const int numb, const char msg[], const char *param);
void mp3player_fatal(const int numb, const char msg[]);
void updateIndex();
void handlePause();
File getIndexFile(uint8_t mode);
const char* getCurrentTrackpath();
// Templates
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

//Setup

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
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
}

/* ****************************************************************
   Main prog loop
*/
void loop() {
  mp3player_dbg(__LINE__, MSG_LOOP_START);
  // if Schleife um stop and play zu verhindern, wenn pause gedrückt ist. Wenn derzeitigige Lösung funktioniert (Verweis direkt zurück zu waitforaction.., dann kann diese Schleife weg.
  // if (! musicPlayer.paused()) {
  // stop
  if (musicPlayer.playingMusic) {
    mp3player_dbg(__LINE__, MSG_STOP_PLAY, musicPlayer.currentTrack.name());
    musicPlayer.stopPlaying();
  }
  // play
  const char* trackpath = getCurrentTrackpath();
  mp3player_dbg(__LINE__, MSG_START_PLAY, trackpath);
  if (! musicPlayer.startPlayingFile(trackpath)) {
    mp3player_fatal(__LINE__, ERR_OPEN_FILE, trackpath);
  }
  musicPlayer.pausePlaying(false);
  //}
  // Wenn das hier drin ist funktioniert pause und Play nicht. Folglich muss hier eine WEnnschleife drum, mit if Pause = false do…… if = true  skip
  // file is now playing in the 'background' so now's a good time
  // to do something else like handling LEDs or buttons :)
  // Wait till button is released:

  waitForReleaseButton();
  delay(200);
  saveState();
  waitForButtonOrTrackEnd();
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

void waitForReleaseButton() {
  while (true) {
    if (buttonState == 0 || buttonState == 2) {
      break;
    }
    else {
      buttonState = KP2.Key_State();
    }
  }
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
  while (true) {
    //btCount = 0;
    checkVolume();
    pressedButton = KP2.Getkey();// checkButtons()
    // Serial  << __LINE__ << ": " << "Button: " << pressedButton;
    buttonState = KP2.Key_State();
    if (pressedButton != 0 && pressedButton != 10) {
      //Serial << __LINE__ << ": " << "Taste: " << pressedButton << " !  " << "State: " << buttonState << '\n';
      handleUserAction(pressedButton);
      return;
    }
    else if (pressedButton == 10) {
      handlePause();
    }
    else if (!musicPlayer.playingMusic && !musicPlayer.paused()) {
      // if track ended, play the next track
      currentTrack++;
      // if the album is at the end of an album. We start at the first track again.
      if (hasLastTrackReached(currentAlbum, currentTrack))
      {
        currentTrack = 0;
        //shutdownNow();
      }
      return;
    }
    else {
      delay(8);
    } // end special actions
  }
  mp3player_dbg(__LINE__, MSG_BUTTON, "Wait for Action loop end");
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
  int offset = getOffset(albumNo, trackNo) + IDX_LINE_LENGTH_W_LF;
  mp3player_dbgi(__LINE__, MSG_OFFSET, offset);
  mp3player_dbgi(__LINE__, MSG_OFFSET, offsetOfNextAlbum);
  if (offset >= offsetOfNextAlbum) {
    return true;
  }
  return false;
}

void playLastTrackIfReachedFirst() {
  while (!hasLastTrackReached(currentAlbum, currentTrack)) {
    currentTrack++;
  }
  currentTrack--;
}

void handleUserAction(byte action) {
  // Set keylevel one up. If bunton was pressed again within buttonPressedDelay
  long times = millis();
  Serial.println("lastButton, pressedbutton, times, lasttime"); 
  Serial.println(lastPressedButton);
  Serial.println(pressedButton);
  Serial.println(times);
  Serial.println(lastButtonTime);
  if (lastPressedButton == pressedButton && ((times - lastButtonTime) < buttonPressedDelay))
    {
      keylevel++;
    }
    else
    {
      keylevel = 0;
      Serial.println("set keylevel to 0");
    }
    mp3player_dbgi(__LINE__, MSG_KEYLEVEL, keylevel);
    lastPressedButton = pressedButton;
    lastButtonTime = times;
  if (action < 10) //Changed from 11 to 10. Because Key 10 will become Play and Pause Button.
  {
    currentAlbum = (action -1 + keylevel * 9); // to make number of key fit numbers of Albums wich is 0 - 17
    currentTrack = 0;
    while (currentAlbum > state.maxAlbum)
    {
      Serial.println("AlbumNummer zu hoch.Setze AlbumNummer tiefer");
      keylevel--;
      currentAlbum = (action -1 + keylevel * 9);
    }
    Serial.println("Switched to Album Number:");
    Serial.println(currentAlbum);
    //playCurrent();
    return;
  }
  // if a function button is pressed:
  else if (action == 11) {
    // We can check if it was pressed again and within given time by checking keylevel
    if (keylevel > 0)
    {
      currentTrack--;
      if (currentTrack < 0) {
        playLastTrackIfReachedFirst(); //numberOfTracks[currentAlbum]
      }
      //playPrevious();
      Serial.println("Zurück Pressed--- fast again -> play previous");
    }
    else {
      //playCurrent(); Passiert sowieso. else schleife überflüssig. hier nur für Debugging
      Serial.println("Zurück Pressed--- once -> play current from beginning");
    }
    return;
  }
  else if (action == 12) {
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
    return;
  }
  // mp3player_dbgi(__LINE__, MSG_ALBUM, currentAlbum);
  // mp3player_dbgi(__LINE__, MSG_TRACK, currentTrack);
  // saveState();
}

void handlePause() { // if Button is 10 pause Song. if already paused. start playing again:
  if (! musicPlayer.paused()) {
    Serial.println("Paused");  // mp3player_dbg(__LINE__, MSG_BUTTON, "Wait for Action loop end");
    musicPlayer.pausePlaying(true);
    waitForReleaseButton();
    return;
  }
  else {
    Serial.println("Resumed");
    musicPlayer.pausePlaying(false);
    waitForReleaseButton();
    return;
  }
  //go back to loop waiting for key or end of Track.
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
  int albumNo = -1; //warum -1 ???? Probably to make first album number 0. (+1)
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

/*
//Function for Alphabetical sorting
// von: https://forum.arduino.cc/index.php?topic=331162.0


//----------------------------------------------------------------------------------------------

void SDCardListWebPage(const String &p_start_folder) { //Minimise Stack size
  const byte c_proc_num = 30;
  Push(c_proc_num);

  CheckRAM();
  ProcessCache(false); //Check cache size before SD file operations

  //See the SD card files /PUBLIC/INITWBPG.TXT and /PUBLIC/ACTNMENU.TXT
  //for the program code for these two common procedures.
  InitialiseWebPage(EPSR(E_SD_CARD_78)); //will install some javascript
  if (HTMLParmRead(E_HEAD_315) == "T") {
    CheckRAM();
    Pop(c_proc_num);
    return;
  }
  InsertActionMenu(600, EPSR(E_SD_CARD_78), St_SD_Card_List_WPE);

  G_EthernetClient.println(F("<tr style=\"background-color:Khaki;\">"));
  G_EthernetClient.println(F("<td><b>PAGE ACTIONS</b></td>"));
  G_EthernetClient.println(F("</tr>"));

  G_EthernetClient.print(F("<tr>"));
  G_EthernetClient.print(F("<td>"));

  InsertLogFileLinks(0); //Hack (public) or all five links

  if ((G_SDCardOK) &&
      (UserLoggedIn())) {
    G_EthernetClient.print(F("<br><input type='checkbox' name='FileDelete' value='Delete'>File Delete<br><br>"));

    G_EthernetClient.print(F("<input type='submit' formaction='/"));
    G_EthernetClient.print(G_PageNames[43]);
    G_EthernetClient.println(F("/' style='width:150px;height:30px;font-size:20px' value='File Upload'>"));
    G_EthernetClient.print(F("<br>"));
  }

  G_EthernetClient.print(F("</td>"));
  G_EthernetClient.print(F("</tr>"));

  CheckRAM();

  //END OF MASTER TABLE, ROW 1, CELL 1 on LHS
  //Time, Page Hits, Page Menu, Action Menu all on LHS
  G_EthernetClient.print(F("</table>"));  //LHS
  G_EthernetClient.print(F("</td>"));

  //START OF RHS PAGE DATA
  //A NEW CELL ON ROW 1 OF THE MASTER TABLE
  G_EthernetClient.println(F("<td style=\"vertical-align:top;\">"));
  G_EthernetClient.println(F("<table id='SDCardListTable' border=1>"));
  G_EthernetClient.print(F("<tr style=\"font-family:Courier;background-color:White;\">"));

  String l_parent_dir = p_start_folder;
  if (!G_SDCardOK) {
    ActivityWrite(EPSR(E_SD_Card_Failure_16));
    G_EthernetClient.print(F("<td>SD Card Failure</td>"));
  }
  else {
    if (l_parent_dir == "")
      l_parent_dir = "/";
    //
    //display the file if a local LAN user or if user logged in or is one of the public directories
    boolean l_SD_file_links = CheckFileAccessSecurity(l_parent_dir);
    String l_last_directory = "";
    int l_dir_count; //reset to zero within the sub procedure
    int l_file_count; //reset to zero within the sub procedure
    int l_panel_count = 0;
    while (true) {
      G_EthernetClient.println(F("<td style=\"vertical-align:top;\" align=\"left\">"));
      SDCardDirectoryBrowse(&l_parent_dir[0], l_SD_file_links, l_last_directory, l_dir_count, l_file_count);
      G_EthernetClient.println(F("</td>"));
      l_panel_count++;
      //Exit now if this is the root folder or there are no sub directories or files or if we already have four panels
      if ((p_start_folder == "/") || (l_dir_count == 0) || (l_file_count != 0) || (l_panel_count == 4)) {
        break;
      }
      //Otherwise we iterate to the last sub ddirectory/folder within this sub directory or folder.
      l_parent_dir = l_parent_dir + l_last_directory + '/';
    }
  } //SD Card OK - extract the file listing

  G_EthernetClient.println(F("</tr>"));

  G_EthernetClient.println(F("<tr><td style='background-color:sandybrown;'><b>CURRENT FOLDER:</b></td></tr>"));
  G_EthernetClient.print(F("<tr><td colspan=\"0\"><input type='text' name='FOLDER' style='font-family:Courier;font-size:20px;background-color:White;text-align:left;width:100%' "));
  G_EthernetClient.print(F("readonly value='"));
  G_EthernetClient.print(l_parent_dir);
  G_EthernetClient.print(F("'></td></tr>"));

  G_EthernetClient.println(F("<tr><td style='background-color:sandybrown;'><b>FILENAME:</b></td></tr>"));
  G_EthernetClient.print(F("<tr><td colspan=\"0\"><input type='text' id='FILENAME' name='FNAME' style='font-family:Courier;font-size:20px;background-color:White;text-align:left;width:100%' "));
  G_EthernetClient.print(F("value=''"));
  //G_EthernetClient.print(F("NULL"));
  G_EthernetClient.println(F("></td></tr>"));

  G_EthernetClient.println(F("</table>")); //RHS SDCardListTable
  G_EthernetClient.println(F("</td>"));

  G_EthernetClient.println(F("</tr>"));
  G_EthernetClient.print(F("</table>")); //form
  G_EthernetClient.println(F("</form>"));
  G_EthernetClient.println(F("</body></html>"));
  CheckRAM();
  Pop(c_proc_num);
} //SDCardListWebPage

void SDCardDirectoryBrowse(const String &p_parent_dir, boolean p_file_links, String &p_last_directory, int &p_dir_count, int &p_file_count) {
  //This procedure is called iteratively to auto traverse to the latest log files.
  const byte c_proc_num = 31;
  Push(c_proc_num);
  CheckRAM();
  SPIDeviceSelect(DC_SDCardSSPin);

  const char C_FileType_Dir  = '1';
  const char C_FileType_File = '2';

  File l_SD_root = SD.open(p_parent_dir.c_str(), FILE_READ);
  if (!l_SD_root) {
    ActivityWrite(Message(M_Unable_to_Open_SD_Card_Root_19));
    SPIDeviceSelect(DC_EthernetSSPin);
    Pop(c_proc_num);
    return;
  }

  //Using arrays we output a sorted directory listing
  //We only use 13 char strings so hopefully we get no String memory fragmentation
  const String C_StringMax = EPSR(E_13_tildes_109);
  const int C_SortMax = 5;
  String l_SortList[C_SortMax];  //The sort array can be as small as one array element
  unsigned long l_FileSize[C_SortMax];
  String l_PrevMax = "";
  int l_entry_count = 2; //Add one for the heading

  //Initialise the sort arrays
  for (byte l_idx = 0; l_idx < C_SortMax; l_idx++) {
    l_SortList[l_idx] = C_StringMax; //Reserves the filename space
    l_FileSize[l_idx] = 0;
  }
  CheckRAM();

  SPIDeviceSelect(DC_EthernetSSPin);
  G_EthernetClient.print(F("<table id='FileListTable' style=\"font-family:Courier;background-color:White;text-align:left;\">"));
  G_EthernetClient.println(F("<tr><td colspan=\"2\">"));
  G_EthernetClient.println(F("<b>DIR: "));
  G_EthernetClient.print(p_parent_dir);
  G_EthernetClient.print(F("</b></td>"));
  G_EthernetClient.print(F("</tr>"));
  G_EthernetClient.println(F("<tr><td><b>ENTRY NAME</b></td><td style=\"text-align:right;\"><b>SIZE</b></td></tr>"));
  SPIDeviceSelect(DC_SDCardSSPin);

  //We extract the SD card file list multiple times outputting a small sorted subset each time
  //SPIDeviceSelect(DC_SDCardSSPin);
  String l_SD_filename = "";
  boolean l_finished = false;
  boolean l_first    = true;
  File l_SD_file;
  p_last_directory = "";
  p_dir_count = 0;
  p_file_count = 0;
  CheckRAM();
  while (true) { //iterate until complete

    //In this first section we parse all the files in the directory and extract
    //the next C_SortMax set that is immediately greater that l_PrevMax.
    l_SD_root.rewindDirectory();
    while (true) {
      l_SD_file = l_SD_root.openNextFile();
      if (!l_SD_file) {
        break;
      }
      if (l_SD_file.isDirectory())
        l_SD_filename = String(C_FileType_Dir);
      else
        l_SD_filename = String(C_FileType_File);
      //
      l_SD_filename += l_SD_file.name();
      //Serial.println(l_SD_filename);
      if ((l_SD_filename > l_PrevMax) && (l_SD_filename < l_SortList[C_SortMax - 1])) {
        boolean l_inserted = false;
        //We are in range so we have to insert
        for (byte l_idx = C_SortMax - 1; l_idx > 0; l_idx--)  {
          //discard the final record (we can get it again on the next parse)
          l_SortList[l_idx] = l_SortList[l_idx - 1];
          l_FileSize[l_idx] = l_FileSize[l_idx - 1];
          //Now see if down one record is where to insert
          if (l_SD_filename > l_SortList[l_idx - 1]) {
            l_SortList[l_idx] = l_SD_filename;
            if (l_SD_file.isDirectory())
              l_FileSize[l_idx] = 0;
            else
              l_FileSize[l_idx] = l_SD_file.size();
            //
            l_inserted = true;
            break;
          } //if
        } //for
        if (!l_inserted) {
          //insert as first item - prev 1st iten was copied to be 2nd item
          l_SortList[0] = l_SD_filename;
          if (l_SD_file.isDirectory())
            l_FileSize[0] = 0;
          else
            l_FileSize[0] = l_SD_file.size();
          //
        } //if l_inserted
      } //if skip files already processed or beyond range
      l_SD_file.close();
    } //for each SD card file
    //We now have our sorted subset of SD card files

    CheckRAM();
    //Now print the array and reset its value
    //If the last cell of the array is C_SortMax we have finished
    SPIDeviceSelect(DC_EthernetSSPin);
    for (byte l_idx = 0; l_idx < C_SortMax; l_idx++) {
      if (l_SortList[l_idx] == C_StringMax) {
        l_finished = true;
        break; //we are done
      }
      if ((l_entry_count % 20) == 0) { //rollover to new column every 20 entries
        G_EthernetClient.print(F("</table>"));
        G_EthernetClient.print(F("</td>"));
        G_EthernetClient.println(F("<td style=\"vertical-align:top;\" align=\"left\">"));
        G_EthernetClient.print(F("<table style=\"font-family:Courier;background-color:White;text-align:left;\">"));
        G_EthernetClient.println(F("<tr><td><b>ENTRY NAME</b></td><td><b>SIZE</b></td>"));
        l_entry_count++; //for this heading
      }
      l_entry_count++; //for the file entry below

      G_EthernetClient.print(F("<tr><td>"));
      if ((p_file_links) || (l_SortList[l_idx][0] == C_FileType_Dir)) { //always show links for sub directories
        //Part I - Set up the html link
        G_EthernetClient.print(F("<a href=\""));
        G_EthernetClient.print(p_parent_dir);
        G_EthernetClient.print(l_SortList[l_idx].substring(1)); //file link pagename
        if (l_SortList[l_idx][0] == C_FileType_Dir) { //Directory
          //G_EthernetClient.print(F(".DIR/\">"));
          G_EthernetClient.print(F("/\">"));
        } //submit() never applies to directories
        else { //File
          //G_EthernetClient.print(F("/\" onclick='fnamesubmit(\""));
          G_EthernetClient.print(F("\" onclick='fnamesubmit(\""));
          G_EthernetClient.print(l_SortList[l_idx].substring(1));
          G_EthernetClient.print(F("\"); return false;'>"));
        }
      }

      //Part II - output the dir/file name
      G_EthernetClient.print(l_SortList[l_idx].substring(1)); //displayed file/dir name

      if ((p_file_links) || (l_SortList[l_idx][0] == C_FileType_Dir))  {//always show links for sub directories
        //Part III - terminate the html link
        G_EthernetClient.print(F("</a>"));
      }
      G_EthernetClient.print(F("</td>"));

      // files have sizes, directories do not
      G_EthernetClient.print(F("<td style=\"text-align:right;\">"));
      if (l_SortList[l_idx][0] == C_FileType_Dir) { //DIR
        p_dir_count++;
        p_last_directory = l_SortList[l_idx].substring(1);
        G_EthernetClient.print(F("[DIR]"));
      }
      else {
        p_file_count++;
        G_EthernetClient.print(l_FileSize[l_idx], DEC);
      }
      //
      G_EthernetClient.println(F("</td></tr>"));
      l_PrevMax = l_SortList[l_idx]; //record the updated prevMax
      l_SortList[l_idx] = C_StringMax; //Reset the array
      l_FileSize[l_idx] = 0; //Reset the array
    } //Print and reset each sort array record

    l_first = false;

    //We are finished if we encountered a C_StringMax record anywhere
    if (l_finished)
      break; //DC_EthernetSSPin already selected
    //
    SPIDeviceSelect(DC_SDCardSSPin);
  } //iterate until complete

  SPIDeviceSelect(DC_SDCardSSPin); //Just in case
  l_SD_root.close();

  SPIDeviceSelect(DC_EthernetSSPin);
  G_EthernetClient.println(F("<tr><td>"));
  G_EthernetClient.println(F("Subdirectories"));
  G_EthernetClient.println(F("</td><td style=\"text-align:right;\">"));
  G_EthernetClient.println(p_dir_count);
  G_EthernetClient.println(F("</td></tr>"));
  G_EthernetClient.println(F("<tr><td>"));
  G_EthernetClient.println(F("Files"));
  G_EthernetClient.println(F("</td><td style=\"text-align:right;\">"));
  G_EthernetClient.println(p_file_count);
  G_EthernetClient.println(F("</td></tr>"));
  G_EthernetClient.print(F("</table>")); //SDFileListTable
  CheckRAM();

  Pop(c_proc_num);
} //SDCardDirectoryBrowse


//Over Option for sorting from: 
//  https://stm32duinoforum.com/forum/viewtopic_f_18_t_4149.html

#include "itoa.h"
#include "SdFat.h"
#include "FreeStack.h"
// **************** SD CARD
const uint8_t SD_CS_PIN = SS;
const char MP3DirPath[] = "MP3/"; // MP3 path
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
  for (int x=0;x 

  
*/
