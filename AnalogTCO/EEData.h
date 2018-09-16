/**
 * Struktura, ktera se zapisuje do EEPROM; zapisuje se jako celek. Protoze jsem nejprve psal
 * jednotlive casti programu, zmenil jsem prislusna globalni pole na reference na prvky teto struktury
 * (vyresi si prekladac).
 */

struct EEData {
  KeySpec   keyTranslations[maxKeyTranslations];
  byte      sensorToOutputMap[outputRows * outputColumns];
  byte      outputsToFlashOn[outputByteSize];
  byte      busId;
  boolean   flashDefault;
  int       minTrackVoltage;
  int       minTrackPercent;
  
  byte      enableKeys : 1;
  byte      enableS88 : 1;
  byte      enableTrack : 1;

  EEData() : flashDefault(false), busId(1), minTrackVoltage(40), minTrackPercent(20), enableKeys(1), enableS88(1), enableTrack(1) {}
};

extern EEData eeData;

const int eeaddr_version = 0;
const int eeaddr_config = 1;
const int eeaddr_top = eeaddr_config + sizeof(EEData);

const int CURRENT_DATA_VERSION = 1;

