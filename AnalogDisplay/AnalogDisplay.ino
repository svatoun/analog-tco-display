#undef NEO
#ifdef NEO
#include <NeoSWSerial.h>
#else
#include <SoftwareSerial.h>
#endif

#include <EEPROM.h>

#include "Config.h"
#include "Utils.h"
#include "Debounce.h"
#include "RS485Frame.h"
#include "EEData.h"
#include "Common.h"

const boolean debugControl = false;

EEData eeData;

char printBuffer[50];

/**
 * Aktualni cas, milisekundy. Nastaveno v kazdem loop-u. Rychlejsi
 * nez neustale cteni millis()
 */
unsigned long currentMillis = 0;
/**
 * Spodnich 16 bitu casu. Vyuziva se pro rychle timery (0-64k = cca 60 sec.)
 */
unsigned int currentMillisLow = 0;

unsigned long start = micros();

/**
 * Cislo radku pro adresu demultiplexeru 4514 a 4515
 */
byte ioRowIndex = 0;

/**
 * Nastavi provoz pro porty demultiplexeru
 */
void setupDemuxPorts() {
  pinMode(DemuxAddr0, OUTPUT);
  pinMode(DemuxAddr1, OUTPUT);
  pinMode(DemuxAddr2, OUTPUT);
  pinMode(DemuxAddr3, OUTPUT);
  // a zaroven nastavi pocatecni adresu
  selectDemuxLine(ioRowIndex);
}

/**
 * Nastavi postupne porty pro vsechny podsystemy
 */
void setupPorts() {
  setupRS485Ports();
  setupS88Ports();
  setupInputPorts();
  setupOutputPorts();

  setupDemuxPorts();
}


/**
 * Prejde na dalsi radek klavesnice, a zobrazovaci
 * matice
 */
void advanceRowIndex() {
  selectDemuxLine(ioRowIndex);
}

/**
   Writes a block of data into the EEPROM, followed by a checksum
*/
void eeBlockWrite(byte magic, int eeaddr, const void* address, int size) {
  if (debugControl) {
    Serial.print(F("Writing EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", source: ")); Serial.println((int)address, HEX);
  }
  const byte *ptr = (const byte*) address;
  byte hash = magic;
  EEPROM.write(eeaddr, magic);
  eeaddr++;
  for (; size > 0; size--) {
    byte b = *ptr;
    EEPROM.write(eeaddr, b);
    ptr++;
    eeaddr++;
    hash = hash ^ b;
  }
  EEPROM.write(eeaddr, hash);
}

/**
   Reads block of data from the EEPROM, but only if the checksum of
   that data is correct.
*/
boolean eeBlockRead(byte magic, int eeaddr, void* address, int size) {
  if (debugControl) {
    Serial.print(F("Reading EEPROM ")); Serial.print(eeaddr, HEX); Serial.print(F(":")); Serial.print(size); Serial.print(F(", dest: ")); Serial.println((int)address, HEX);
  }
  int a = eeaddr;
  byte hash = magic;
  byte x;
  boolean allNull = true;
  x = EEPROM.read(a);
  if (x != magic) {
    if (debugControl) {
      Serial.println(F("No magic header found"));
    }
    return false;
  }
  a++;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    if (x != 0) {
      allNull = false;
    }
    hash = hash ^ x;
  }
  x = EEPROM.read(a);
  if (hash != x || allNull) {
    if (debugControl) {
      Serial.println(F("Checksum does not match"));
    }
    return false;
  }

  a = eeaddr + 1;
  byte *ptr = (byte*) address;
  for (int i = 0; i < size; i++, a++) {
    x = EEPROM.read(a);
    *ptr = x;
    ptr++;
  }
}

void checkInitEEPROM() {
  int savedVer = EEPROM.read(eeaddr_version);
  if (savedVer == CURRENT_DATA_VERSION) {
    return;
  }
  Serial.println(F("Obsolete EEPROM, reinitializing"));
  EEPROM.write(eeaddr_version, CURRENT_DATA_VERSION);
  resetAll();
  saveAll();
}


void setup() {
  Serial.begin(115200);
  setupPorts();
  
  resetBusMaster();
  resetInput();
  resetOutput();
  resetS88();
  checkInitEEPROM();
  loadAll();
  initTerminal();

  registerLineCommand("EDMP", &commandDumpEEProm);
  registerLineCommand("DMP", &commandDumpAll);
  registerLineCommand("CLR", &commandClear);
  registerLineCommand("SAV", &commandSave);
  registerLineCommand("FTR", &commandFeature);
  registerLineCommand("INF", &commandInfo);

//  testCommunication();

  Serial.print(F("Analog Control Panel, version ")); Serial.print(majorVersion); Serial.print('.'); Serial.println(minorVersion);
  Serial.println(F("Copyright (c) 2018, belgarat@klfree.net; licensed under Apache License 2.0 "));
  Serial.println();
  printPrompt();
}

void resetAll() {
  eeData = EEData();
  resetInput();
  resetOutput();
}

void saveAll() {
  eeBlockWrite('C', eeaddr_config, &eeData, sizeof(eeData));
}


void loadAll() {
  eeBlockRead('C', eeaddr_config, &eeData, sizeof(eeData));
}

/**
 * Casovac pro zpracovani radek klavesnice.
 */
unsigned int lastIORowStart = 0;

byte rowStep = 0;
/**
 * Zpracuje jeden radek klavesnice a vystupu, ale nejdrive za
 * `ioRowSwitchDelay` milisekund; to zajisti urcitou dobu sviceni
 * LEDek v radku
 */
void shiftIORow() {
  if (!elapsedTime(lastIORowStart, ioRowSwitchDelay)) {
    return;
  }
  
  prepareOutputRow();         
  selectDemuxLine(ioRowIndex);
  
  displayOutputRow();
  if ((rowStep % 8) == 0) {
//    processInputRow();
  }
  ioRowIndex = (ioRowIndex + 1) % 8;
  if (ioRowIndex == 0) {
    rowStep++;
  }
}

const boolean testOnly = true;

extern volatile long  intCount;
void loop() {
  processVoltage();
  updateTime();
    
  shiftIORow();
  updateTime();
//  transmitFrames();

  updateTime();
  processS88Bus();

  updateTime();
  flipFlashes();
  processTerminal();
}

void commandClear() {
  Serial.println(F("Clearing all settings"));
  resetAll();
  saveAll();
  commandReset();
}

void commandDumpEEProm() {
  Serial.println(F("EEPROM Dump:"));
  char buffer[24 + 4 + 2];
  int cnt = 0;
  char *ptr = buffer;
  *ptr = 0;
  for (int eea = 0; eea < eeaddr_top; eea++) {
    int r = EEPROM.read(eea);
    if (r < 0x10) {
      *(ptr++) = '0';
    }
    ptr = printNumber(ptr, r, 16);
    append(ptr, ' ');
    cnt++;
    if (cnt %32 == 0) {
      Serial.println(buffer);
      *(ptr = buffer) = 0;
    } else if (cnt % 8 == 0) {
      Serial.print(buffer);
      Serial.print(F("- "));
      *(ptr = buffer) = 0;
    }
  }
  Serial.println(buffer);
}

void commandSave() {
  saveAll();
  Serial.println(F("All saved to EEPROM"));
}

void commandDumpAll() {
  commandFlashDump();
  commandShowKeys();
  dumpTrackSensitivity();
  printFeatures();
}

void printFeatures() {
  if (eeData.enableTrack && eeData.enableKeys && eeData.enableS88) {
    Serial.println(F("FTR:a:1"));
    return;
  }
  Serial.print(F("FTR:t:")); Serial.println(eeData.enableTrack);
  Serial.print(F("FTR:k:")); Serial.println(eeData.enableKeys);
  Serial.print(F("FTR:s:")); Serial.println(eeData.enableS88);
}

void commandFeature() {
  char f = *inputPos;
  if (f == 0) {
    printFeatures();
    return;
  }
  ++inputPos;
  if (*inputPos != ':') {
    Serial.println(F("Bad syntax"));
    return;
  }
  inputPos++;
  boolean en;
  switch (*inputPos) {
    case '1': case '+': case 'y':
      en = 1; break;
    case '0': case '-': case 'n':
      en = 0; break;
    default:
      Serial.println(F("Bad state"));
      return;
  }
  switch (f) {
    case 'a':
      eeData.enableTrack = eeData.enableKeys = eeData.enableS88 = en;
      break;
    case 't': eeData.enableTrack = en; break; 
    case 'k': eeData.enableKeys = en; break; 
    case 's': eeData.enableS88 = en; break; 
    default:
      Serial.println(F("Bad feature"));
      return;
  }
}

void commandInfo() {
  Serial.println(F("Output state:"));
  printMatrixOutput();
}

