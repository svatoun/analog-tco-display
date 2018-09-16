const boolean debugMatrixOutput = false;
const boolean debugFlashes = false;

const int flashMillis = 500;
const int maxFlashCount = 16;
const int onFlashCount = 8;

/**
 * Holds physical output to be set to the LEDs
 */
static_assert(outputColumns % 8 == 0, "Number of output columns must be a multiply of 8");
static_assert((sizeof(unsigned int) * 8) >= outputColumns, "Max 16 columns is supported");

const byte outputRowSize = ((outputColumns + 7) / 8);
const int maxOutputs = outputRows * outputColumns;

/**
 * Maps S88 sensors to output signals. Initializes to identity function.
 */
byte (&sensorToOutputMap)[maxOutputs] = eeData.sensorToOutputMap;

/**
 * Bitfield where 1 represnts an output, which will flash when going ON.
 */
byte (&outputsToFlashOn)[outputByteSize] = eeData.outputsToFlashOn;

/** 
 *  Bitfield of the output matrix periodically shifted into the 
 *  shift register row by row. 
 */
byte physicalOutput[outputByteSize];

/**
 * Value of the current (selected on demux-es) output row, 8 or 16 bits
 */
unsigned int outRowValue;

/**
 * The number of flashing outputs / the top index of the buffer
 */
byte flashTop = 0;

/**
 * Specifies flashing item
 */
struct FlashItem {
  byte id : 8;        // id of the output
  byte count : 4;     // counter; when reaches zero, the output stops flashing
  boolean finalState : 1; // the final output's state
  
  FlashItem() : id(0xff), count(0), finalState(false) {}
  FlashItem(byte aid, byte acount, boolean aState) : id(aid), count(acount), finalState(aState) {}
};

static_assert(sizeof(FlashItem) <= 2, "FlashItem is too large");

/////////////////////////////////////////////////////////////////////////////

FlashItem flashes[maxFlashCount];

/**
 * Time of last flash flip, shortened millis.
 */
unsigned int lastFlip = 0;

/**
 * Last flash phase. Alters each flipPhase timeout
 */
boolean flashPhase;

/**
 * Initializes output-related variables
 */
void resetOutput() {
  for (byte i = 0; i < outputByteSize; i++) {
    physicalOutput[i] = 0;
    outputsToFlashOn[i] = 0;
    sensorToOutputMap[i] = i;
  }
}

void setupOutputPorts() {
  // setup 
  pinMode(FbPowerClock, OUTPUT);
  pinMode(FbPowerData, OUTPUT);
  pinMode(FbPowerLatch, OUTPUT);
  resetOutput();

  registerLineCommand("FLSH", &commandFlash);
  registerLineCommand("NOFL", &commandNoFlash);
  registerLineCommand("FDEF", &commandFlashAll);
  registerLineCommand("FDMP", &commandFlashDump);
  registerLineCommand("OUT", &commandOut);
}

void printFlashTable() {
  if (!debugFlashes) {
    return;
  }
  Serial.print(F("Flash dump:")); Serial.println(flashTop);
  const FlashItem* item;
  byte pos;
  for (item = &flashes[0], pos = 0; pos < flashTop; pos++, item++) {
    Serial.print('#'); Serial.print(pos); Serial.print(' ');
    Serial.print(item->id); Serial.print('\t'); Serial.print(item->count); Serial.print(':'); Serial.println(item->finalState);
  }
}

/**
 * Starts flashing at a certain output. If the output is already flashing,
 * it will continue, with the new timeout
 */
void addFlashOutput(byte index, byte count, boolean state) {
  if (count > 16) {
    return;
  }
  if (count == 0) {
    count++;
  }
  if (debugFlashes) {
    Serial.print(F("AddFlash ")); Serial.print(index); Serial.print(F(" c:")); Serial.print(count); Serial.print('-'); Serial.println(state);
    printFlashTable();
  }
  if (flashTop >= maxFlashCount) {
    Serial.print(F("Flashes exhausted"));
    return;
  }
  int pos;
  FlashItem *item;
  for (item = flashes, pos = 0; pos < flashTop; pos++, item++) {
    if (item->id >= index) {
      break;
    }
  }
  if ((item < (flashes + flashTop)) && (item->id > index)) {
    int l = (flashTop - pos) * sizeof(FlashItem);
    if (debugFlashes) {
      Serial.print(F("Room @")); Serial.print(pos); Serial.print(':'); Serial.println(l);
    }
    memmove(item + 1, item, l);
  }
  *item = FlashItem(index, count - 1, state);
  if (flashTop++ == 0) {
    if (debugFlashes) {
      Serial.println(F("Start flashing"));
    }
    // initialize flash phase and timer, first flashing signal
    flashPhase = state;
    recordStartTime(lastFlip);
  }
  printFlashTable();
}

boolean removeFlashOutput(byte index) {
  if (debugFlashes) {
    Serial.print(F("RemFlash")); Serial.println(index);
    printFlashTable();
  }
  for (FlashItem* item = flashes; item < flashes + flashTop; item++) {
    if (item->id > index) {
      return false;
    } else if (item->id == index) {
      byte l = (flashTop - (item + 1 - flashes)) * sizeof(FlashItem);
      if (debugFlashes) {
        Serial.print(F("Del @")); Serial.print((item - flashes)); Serial.print(':'); Serial.println(l);
      }
      memmove(item, item + 1, l);
      flashTop--;
      return true;
    }
  }
  return false;
}

void flipFlashes() {
  if (!elapsedTime(lastFlip, flashMillis)) {
    return;
  }
  if (flashTop == 0) {
    return;
  }
  if (debugFlashes) {
    printFlashTable();
  }
  flashPhase = !flashPhase;
  FlashItem *p = flashes;
  FlashItem *d = flashes;
  if (debugFlashes) {
    Serial.print(F("FlashFlip @")); Serial.print((int)p, HEX); Serial.print('-'); Serial.print(flashPhase);  Serial.print(F(" top:")); Serial.println(flashTop);
  }
  for (byte i = 0; i < flashTop; i++, p++) {
    if (p->count) {
      p->count--;
      if (debugFlashes) {
        Serial.print(F("Toggle #")); Serial.print(p->id); Serial.print(F(" cnt:")); Serial.print(p->count); Serial.print('\t'); Serial.println((int)p, HEX);
      }
      byte id = p->id;
      writeBit(physicalOutput, id, flashPhase);
      if (p > d) {
        if (debugFlashes) {
          Serial.print(F("Moving @")); Serial.print((int)p, HEX); Serial.print('-'); Serial.println((int)d, HEX);
        }
        *(d) = *p;
      }
      d++;
    } else {
      if (debugFlashes) {
        Serial.print(F("Stop #")); Serial.print(p->id); Serial.print(':'); Serial.println(p->finalState);
      }
      byte id = p->id;
      writeBit(physicalOutput, id, p->finalState);
      // do not increment d
    }
  }
  flashTop = (d - flashes);
  if (debugFlashes) {
    Serial.print(F("Flash count")); Serial.println(flashTop);
  }
}

void prepareOutputRow() {
  if (outputRowSize == 1) {
    outRowValue = *(physicalOutput + ioRowIndex);
  } else if (outputRowSize == 2) {
    outRowValue = *(((unsigned int*)(physicalOutput)) + ioRowIndex);
  } else {
    // yet unsupported
    outRowValue = (ioRowIndex & 0x01) ? 0xaaaa : 0x5555;
  }
}

void displayOutputRow() {
  digitalWrite(FbPowerLatch, LOW);
  if (outputColumns == 8) {
    shiftOut(FbPowerData, FbPowerClock, MSBFIRST, outRowValue);
  } else if (outputColumns == 16) {
    shiftOut(FbPowerData, FbPowerClock, MSBFIRST, outRowValue & 0xff);
    shiftOut(FbPowerData, FbPowerClock, MSBFIRST, outRowValue >> 8);
  } else {
    // unsupported
    return;
  }
  digitalWrite(FbPowerLatch, HIGH);
  digitalWrite(FbPowerLatch, LOW);
}

void setOutputFromSensor(byte sensorNumber, boolean state) {
  if (sensorNumber >= sizeof(sensorToOutputMap)) {
    return;
  }
  byte outId = sensorToOutputMap[sensorNumber];
  if (debugMatrixOutput) {
    Serial.print(F("OutputSet: sens=")); Serial.print(sensorNumber); Serial.print(F(" oid:")); Serial.print(outId);
  }
  setOutput(outId, state);
}

void setOutput(int outId, boolean state) {
  boolean shouldFlash = readBit(outputsToFlashOn, outId);
  
  if (debugMatrixOutput) {
    Serial.print(F(" fl:")); Serial.print(shouldFlash); Serial.print(F(" st:")); Serial.println(state);
  }
  if (shouldFlash) {
    if (state) {
      addFlashOutput(outId, onFlashCount, state);
      writeBit(physicalOutput, outId, flashPhase);
      return;
    } else {
      removeFlashOutput(outId);
      // fall through
    }
  }
  writeBit(physicalOutput, outId, state);
}

void commandFlashAll() {
  char c = *inputPos;
  boolean turnOn = true;
  switch (c) {
    case '-': case '0': case 'n':
      turnOn = false;
  }
  eeData.flashDefault = turnOn;
  for (byte n = 0; n < outputByteSize; n++) {
    outputsToFlashOn[n] = turnOn ? 0xff : 0x00;
  }
  Serial.println(turnOn ? F("All flash") : F("All stable"));
}

void commandNoFlash() {
  commandFlashOnOff(false);
}

void commandFlash() {
  commandFlashOnOff(true);
}

void commandFlashOnOff(boolean turnOn) {
  int start;
  int end;
  while (*inputPos > 0) {
    char* colon = strchr(inputPos, ':');
    if (colon == NULL) {
      colon = inputPos + strlen(inputPos);
    } else {
      *colon = 0;
      colon++;
    }
    char* dash = strchr(inputPos, '-');
    if (dash == NULL) {
      start = end = nextNumber();
    } else {
      *dash = 0;
      start = nextNumber();
      inputPos = dash + 1;
      end = nextNumber();
    }
    if (start < 1 || (start > (maxOutputs))) {
      Serial.println(F("Bad start"));
      return;
    }
    if (end < 1 || end > (maxOutputs)) {
      Serial.println(F("Bad end"));
      return;
    }
    if (end < start) {
      Serial.println(F("Bad range"));
    }
    start--;
    end--;
    for (byte n = start; n <= end; n++) {
      writeBit(outputsToFlashOn, n, turnOn);
    }
    Serial.print(F("Set ")); 
    Serial.print(start + 1); 
    if (end > start) {
      Serial.print('-'); Serial.print(end + 1); 
    }
    Serial.println(turnOn ? F(" Flash") : F(" Stable"));
  }
  saveAll();
}

int itemCount = 0;
void printStartEnd(int start, int end) {
  if (start == -1) {
    return;
  }
  if ((itemCount & 0x07) == 0) {
    if (itemCount > 0) {
      Serial.println(); 
    }
    Serial.print(eeData.flashDefault ? F("NOFL:") : F("FLSH:"));
  } else {
    Serial.print(':');
  }
  itemCount++;
  switch (end - start) {
    case 0:
      Serial.print(start + 1);
      break;
    case 1:
      Serial.print(start + 1); Serial.print(':'); Serial.print(end + 1);
      break;
    default:
      Serial.print(start + 1); Serial.print('-'); Serial.print(end + 1);
  }
}

void commandFlashDump() {
  boolean defState = eeData.flashDefault;
  boolean curState = defState;
  
  byte outNum = 0;
  int startNum = -1;
  itemCount = 0;
  for (byte i = 0; i <  outputByteSize; i++) {
    byte empty = curState ? 0xff : 0x00;
    byte out = outputsToFlashOn[i];
    if (debugFlashes) {
      Serial.print(F("fli:")); Serial.print(i); Serial.print('='); Serial.print(out, BIN);
      Serial.print(' '); Serial.print(curState); Serial.print('/'); Serial.println(defState);
    }
    if (out == empty) {
      outNum += 8;
      continue;
    }
    byte mask = 0x01;
    for (int b = 0; b < 8; b++, mask <<= 1) {
      boolean st = (out & mask);
      if (st == curState) {
        outNum++;
        continue;
      }

      if (startNum > -1 && curState != defState) {
        printStartEnd(startNum, outNum - 1);
      }
      curState = st;
      startNum = (curState == defState) ? -1 : outNum;
      outNum++;
    }
  }
  if (startNum > 0 && curState != defState) {
    printStartEnd(startNum, outNum);
  }
  if ((itemCount & 0x07) > 0) {
    Serial.println();
  }
}

void commandOut() {
  int n = nextNumber();
  boolean on = true;
  switch (*inputPos) {
    case '0': case '-': case 'n':
      on = false;
  }
  if (n < 1 || n > maxOutputs) {
    Serial.println(F("Bad output"));
    return;
  }
  setOutput(n - 1, on);
}

void print2Digits(int a) {
  if (a < 10) Serial.print('0');
  Serial.print(a);
}

void print3Digits(int a) {
  if (a < 10) Serial.print(' ');
  if (a < 100) Serial.print(' ');
  Serial.print(a);
}

void print8Bits(int a) {
  byte m = 0x80;
  while (m > 0) {
    Serial.print((a & m) > 0 ? '1': '0');
    m >>= 1;
  }
}

void printMatrixOutput() {
  for (byte r = 0; r < outputRows; r++) {
    Serial.print('#'); print2Digits(r + 1); Serial.print('['); print3Digits(r * 8 + 1); Serial.print('-'); print3Digits(r * 8 + 1 + 7); Serial.print(F("]:\t")); 
    print8Bits(physicalOutput[r]);
    Serial.println();
  }
}

