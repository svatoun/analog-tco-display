const boolean debugKeyInput = false;
const boolean debugKeyFn = true;
const boolean debugKeySearch = false;

static_assert(inputColumns % 8 == 0, "Number of output columns must be a multiply of 8");
static_assert((sizeof(unsigned int) * 8) >= inputColumns, "Max 16 columns is supported");
static_assert(maxTarget <= 16, "KeySpec structure can record just 16 targets, sorry");

KeySpec   (&keyTranslations)[maxKeyTranslations] = eeData.keyTranslations;

// the acknowledged, debounced state
byte inputDebounced[inputByteSize];

class KeyDebouncer : public Debouncer {
  public:
  KeyDebouncer(byte modCount, byte* debouncedState);
  virtual boolean stableChange(byte number, boolean nState);
};

KeyDebouncer::KeyDebouncer(byte modCount, byte* debouncedState) : Debouncer(modCount, debouncedState) {
}

KeyDebouncer inputKeyDebouncer(sizeof(inputDebounced), inputDebounced);

unsigned int inputRowValue;
unsigned int inputRowMask;

void setupInputPorts() {
  pinMode(TcInputClock, OUTPUT);
  pinMode(TcInputData, INPUT);
  pinMode(TcInputLatch, OUTPUT);
  resetInput();

  registerLineCommand("KMAP", &commandMapKeys);
  registerLineCommand("KEYS", &commandShowKeys);
  registerLineCommand("DMAP", &commandDelMap);
  registerLineCommand("PRES", &commandPress);
}

void resetInput() {
  for (byte i = 0; i < inputByteSize; i++) {
    inputDebounced[i] = 0;
  }
  memset(keyTranslations, 0, sizeof(keyTranslations));
  // zadefinujeme pocatecni nastaveni - vsechna tlacitka se posilaji na
  // jedno zarizeni.
  KeySpec &sp = keyTranslations[0];
  sp.rectangle(0, 0, 7, 15, 2, 0);
}

uint8_t analogShiftIn(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder) {
  uint8_t value = 0;
  uint8_t i;

  for (i = 0; i < 8; ++i) {
    digitalWrite(clockPin, HIGH);
    if (bitOrder == LSBFIRST)
      value |= analogTTLRead(dataPin) << i;
    else
      value |= analogTTLRead(dataPin) << (7 - i);
    digitalWrite(clockPin, LOW);
  }
  return value;
}

unsigned int lastDebounceTick = 0;

void processInputRow() {
  digitalWrite(TcInputClock, LOW);
  digitalWrite(TcInputLatch, HIGH);
//  delayMicroseconds(10);
  digitalWrite(TcInputClock, HIGH);
//  delayMicroseconds(10);
  digitalWrite(TcInputLatch, LOW);
  byte input1 = TcInputData > 13 ? analogShiftIn(TcInputData, TcInputClock, LOW) : shiftIn(TcInputData, TcInputClock, LOW);
  input1 = ~input1;
  inputKeyDebouncer.debounce(ioRowIndex, &input1, 1);
  if (elapsedTime(lastDebounceTick, keyboardDebounceTime)) {
    inputKeyDebouncer.tick();
  }
}

int findKeyTranslation(byte nx, byte ny, int& target) {
  if (debugKeySearch) {
    Serial.print(F("Search: ")); Serial.print(ny); Serial.print(','); Serial.println(nx);
  }
  for (const KeySpec* spec = keyTranslations; !spec->isEmpty(); spec++) {
    if (debugKeySearch) {
      Serial.print(F("Trying: ")); spec->printDef(); Serial.println();
    }
    if (spec->matrix) {
      if ((spec->x > nx) || (spec->y > ny)) {
        if (debugKeySearch) {
          Serial.println(F("Start too high"));
        }
        continue;
      }
    }
    byte w = (spec->lenOrMatrix & 0xf) + 1;
    byte h = (spec->lenOrMatrix >> 4) + 1;

    byte mx = spec->x + w;
    byte my = spec->y + h;
    if (debugKeySearch) {
      Serial.print(F("h:")); Serial.print(h); Serial.print('\t'); Serial.print(my); 
      Serial.print(F("\tw: ")); Serial.print(w); Serial.print('\t'); Serial.println(mx); 
    }
    if ((mx <= nx) || (my <= ny)) {
      if (debugKeySearch) {
        Serial.println(F("End too low"));
      }
      continue;
    }

    byte fx = w;
    byte dx = (nx - spec->x);
    byte dy = (ny - spec->y);
    
    byte r = fx * dy + dx;
    target = spec->target;
    byte cmd = r + spec->commandBase;
    if (debugKeySearch) {
      Serial.print(F("dx: ")); Serial.print(dx); Serial.print(F("\tdy: ")); Serial.print(dy); 
      Serial.print(F("\tline size: ")); Serial.print(fx); Serial.print(F("\toffset: ")); Serial.println(r); 
      Serial.print(F("target: ")); Serial.print(target); Serial.print(F("\tcmd: ")); Serial.println(cmd); 
    }
    return cmd;
  }
  return -1;
}

long sensTime = millis() + 500;

boolean KeyDebouncer::stableChange(byte number, boolean nState) {
  if (!Debouncer::stableChange(number, nState)) {
    return false;
  }
  byte ny = number / inputColumnsRounded;
  byte nx = number - (ny * inputColumnsRounded);

  if (debugKeyInput) {
    Serial.print("Stable change: "); Serial.print(inputColumnsRounded); Serial.print(';'); Serial.print(number); Serial.print('='); Serial.println(nState);
  }

  if (currentMillis < sensTime) {
    return;
  }

  pressKey(nx, ny, nState);
  return true;
}

void pressKey(byte nx, byte ny, boolean nState) {
  int target = 0;
  int command = findKeyTranslation(nx, ny, target);

  if (command == -1) {
    Serial.print(F("Unhandled key: ")); Serial.print(nx); Serial.print(','); Serial.println(ny);
    return;
  }
  if (debugKeyFn) {
    Serial.print(F("Key ")); Serial.print(nx); Serial.print(','); Serial.print(ny); Serial.print(F(" => "));
    Serial.print(target); Serial.print(':'); Serial.println(command);
  }
  RemoteCommand cmd(1, command, nState);

  addMessage(target, eeData.busId, (const byte*)&cmd, sizeof(cmd));
}

void commandMapKeys() {
  KeySpec *freeSlot = NULL;
  byte slotCnt = 0;
  int index = -1;
  for (KeySpec *slot = keyTranslations; slotCnt < maxKeyTranslations; slotCnt++, slot++) {
    if (slot->isEmpty()) {
      freeSlot = slot;
      break;
    }
  }
  if (freeSlot == NULL) {
    Serial.println(F("No free slots"));
    return;
  }
  char c = *inputPos;
  boolean matrix;

  if (c >= '1' && c <= '9') {
    index = nextNumber();
    if (index < 1 || index > slotCnt + 1) {
      Serial.println(F("Bad index"));
      return;
    }
    c = *inputPos;
    index--;
  }
  
  switch (c) {
    case 'm':
      matrix = 1;
      break;
    case 's':
      matrix = 0;
      break;
    default:
      Serial.println(F("No type"));
      return;
  }
  inputPos++;
  if (*inputPos != ':') {
      Serial.println(F("Syntax err"));
      return;
  }
  inputPos++;
  char *colon = strchr(inputPos, ':');
  if (colon == NULL) {
    Serial.println(F("Bad definition"));
    return;
  }
  *colon = 0;
  char *dot = strchr(inputPos, ',');
  if (dot == NULL) {
    Serial.print(F("Bad point"));
    return;
  }
  *dot = 0;
  int y = nextNumber();
  inputPos = dot + 1;
  int x = nextNumber();

  if ((x < 1) || (x > inputColumns)) {
    Serial.println(F("Bad column"));
    return;
  }
  if ((y < 1) || (y > inputRows)) {
    Serial.println(F("Bad row"));
    return;
  }
  inputPos = colon + 1;
  int lengthOrMatrix;
  
  x--;
  y--;
  if (matrix) {
    colon = strchr(inputPos, ':');
    if (colon == NULL) {
      Serial.println(F("Bad definition"));
      return;
    }
    *colon = 0;
    dot = strchr(inputPos, ',');
    if (dot == NULL) {
      Serial.print(F("Bad dim format"));
      return;
    }
    *dot = 0;
    int h = nextNumber();
    inputPos = dot + 1;
    int w = nextNumber();

    if (w < 1 || h < 1) {
      Serial.print(F("Bad dim"));
      return;
    }
    byte ex = x + w;
    byte ey = y + h;
    if ((ex > inputColumns) || (ey > inputRows)) {
      Serial.println(F("Large dim"));
      return;
    }
    h--;
    w--;
    lengthOrMatrix = ((h & 0x0f) << 4) | w;
    inputPos = colon + 1;
  } else {
    lengthOrMatrix = nextNumber();
    if (lengthOrMatrix < 1) {
      Serial.println(F("Bad length"));
      return;
    }
  }

  int target = nextNumber();
  if ((target < 1) || (target == eeData.busId) || (target >= maxTarget)) {
    Serial.println(F("Invalid target"));
    return;
  }

  int cmdBase = nextNumber();
  if (cmdBase < 0) {
    Serial.println(F("Invalid command base"));
    Serial.print(target); Serial.print(' '); Serial.print(cmdBase);
    return;
  }

  KeySpec *pos = freeSlot;
  KeySpec spec;
  spec.x = x; spec.y = y;
  spec.matrix = matrix;
  spec.lenOrMatrix = lengthOrMatrix;
  spec.target = target;
  spec.commandBase = cmdBase;
  if ((index > -1) && (index < slotCnt)) {
    pos = keyTranslations + index;
    memmove(pos + 1, pos, (freeSlot - pos) * sizeof(KeySpec));
  }
  *pos = spec;    

  Serial.print(F("Defined keymap: ")); Serial.print(slotCnt + 1); Serial.print(':');
  pos->printDef();
  Serial.println();
}

void commandDelMap() {
  KeySpec *freeSlot = NULL;
  byte slotCnt = 0;
  int index = -1;
  for (KeySpec *slot = keyTranslations; slotCnt < maxKeyTranslations; slotCnt++, slot++) {
    if (slot->isEmpty()) {
      freeSlot = slot;
      break;
    }
  }
  if (freeSlot == keyTranslations) {
    Serial.println(F("No keys"));
    return;
  }
  int n = nextNumber();
  if (n < 1 || n > slotCnt) {
    Serial.println(F("Bad index"));
    return;
  }
  KeySpec *pos = (keyTranslations + n) - 1;
  KeySpec save = *pos;
  if (pos < freeSlot) {
    memmove(pos, pos + 1, ((freeSlot - pos) - 1) * sizeof(KeySpec));
    (freeSlot - 1)->target = 0;
  } else {
    pos->target = 0;
  }
  Serial.print(F("Deleted: "));
  save.printDef();
  Serial.println();
}

void commandShowKeys() {
  byte slotCnt = 0;
  for (const KeySpec *slot = keyTranslations; (slotCnt < maxKeyTranslations) && !slot->isEmpty(); slotCnt++, slot++) {
    Serial.print(F("KMAP:")); 
    Serial.print(slotCnt + 1); Serial.print(':');
    slot->printDef();
    Serial.println();
  }
}

void KeySpec::printDef() {
  Serial.print(matrix ? 'm' : 's'); Serial.print(':');
  Serial.print(y + 1); Serial.print(','); Serial.print(x + 1); Serial.print(':');
  if (matrix) {
    int w = ((lenOrMatrix & 0x0f) + 1);
    int h = ((lenOrMatrix >> 4) + 1);
    Serial.print(h); Serial.print(','); Serial.print(w);
  } else {
    Serial.print(lenOrMatrix);
  }
  Serial.print(':');
  Serial.print(target); Serial.print(':'); Serial.print(commandBase);
}

void commandPress() {
  char *colon = strchr(inputPos, ':');
  if (colon == NULL) {
    Serial.println(F("Bad definition"));
    return;
  }
  *colon = 0;
  char *dot = strchr(inputPos, ',');
  if (dot == NULL) {
    Serial.print(F("Bad point"));
    return;
  }
  *dot = 0;
  int y = nextNumber();
  inputPos = dot + 1;
  int x = nextNumber();

  if ((x < 1) || (x > inputColumns)) {
    Serial.println(F("Bad column"));
    return;
  }
  if ((y < 1) || (y > inputRows)) {
    Serial.println(F("Bad row"));
    return;
  }
  inputPos = colon + 1;
  int lengthOrMatrix;
  
  x--;
  y--;

  byte on = true;
  switch (*inputPos) {
    case '0': case '-': case 'n': case 'u': case 'o':
    on = false;
  }
  if (debugKeyFn) {
    Serial.print(F("Pressed: ")); Serial.print(y); Serial.print(','); Serial.print(x); Serial.print('='); Serial.println(on);  
  }
  
  pressKey(x, y, on);
}

