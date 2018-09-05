const boolean debugKeyInput = true;
const boolean debugKeyFn = true;

static_assert(inputColumns % 8 == 0, "Number of output columns must be a multiply of 8");
static_assert((sizeof(unsigned int) * 8) >= inputColumns, "Max 16 columns is supported");
static_assert(maxTarget <= 16, "KeySpec structure can record just 16 targets, sorry");

KeySpec   (&keyTranslations)[maxKeyTranslations] = eeData.keyTranslations;

// the acknowledged, debounced state
byte inputDebounced[inputByteSize];

class KeyDebouncer : public Debouncer {
  public:
  KeyDebouncer(byte modCount, byte* debouncedState);
  virtual void stableChange(byte number, boolean nState);
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
  delayMicroseconds(10);
  digitalWrite(TcInputClock, HIGH);
  delayMicroseconds(10);
  digitalWrite(TcInputLatch, LOW);
  byte input1 = TcInputData > 13 ? analogShiftIn(TcInputData, TcInputClock, LOW) : shiftIn(TcInputData, TcInputClock, LOW);
  input1 = ~input1;
  inputKeyDebouncer.debounce(ioRowIndex, &input1, 1);
  if (elapsedTime(lastDebounceTick, keyboardDebounceTime)) {
    inputKeyDebouncer.tick();
  }
}

int findKeyTranslation(byte nx, byte ny, int& target) {
  for (const KeySpec* spec = keyTranslations; !spec->isEmpty(); spec++) {
    if (spec->matrix) {
      if ((spec->x > nx) || (spec->y > ny)) {
        continue;
      }
    }
    byte w = (spec->lenOrMatrix & 0xf);
    byte h = (spec->lenOrMatrix >> 4);
    if ((w < nx) || (h < ny)) {
      continue;
    }
    byte fx = w - spec->x + 1;
    byte fy = h + spec->y + 1;

    byte r = (ny - spec->y) * fx;
    r += (nx - spec->x);
    target = spec->target;
    return r + spec->commandBase;
  }
  return -1;
}

void KeyDebouncer::stableChange(byte number, boolean nState) {
  Debouncer::stableChange(number, nState);
  byte ny = number / inputColumnsRounded;
  byte nx = number - (ny * inputColumnsRounded);

  if (debugKeyInput) {
    Serial.print("Stable change: "); Serial.print(inputColumnsRounded); Serial.print(';'); Serial.print(number); Serial.print('='); Serial.println(nState);
  }

  pressKey(nx, ny, nState);
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
    if (index < 1 || index > slotCnt) {
      Serial.println(F("Bad index"));
      return;
    }
    c = *inputPos;
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
  }
  inputPos++;
  int x = nextNumber();
  int y = nextNumber();

  if ((x < 1) || (x > inputColumns)) {
    Serial.println(F("Bad column"));
    return;
  }
  if ((y < 1) || (y > inputRows)) {
    Serial.println(F("Bad row"));
    return;
  }
  int lengthOrMatrix;

  x--;
  y--;
  if (matrix) {
    char *colon = strchr(inputPos, ':');
    if (colon == NULL) {
      Serial.println(F("Bad definition"));
      return;
    }
    *colon = 0;
    char *dot = strchr(inputPos, ',');
    if (dot == NULL) {
      Serial.print(F("Bad rect format"));
      return;
    }
    *dot = 0;
    int w = nextNumber();
    inputPos = dot + 1;
    int h = nextNumber();

    if (w < 1 || h < 1) {
      Serial.print(F("Invalid dim"));
      return;
    }
    w += x;
    h += y;
    if ((w > inputColumns) || (h > inputRows)) {
      Serial.println(F("Large dim"));
      return;
    }
    h--;
    w--;
    lengthOrMatrix = (h & 0x0f) << 4 | w;
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
    pos = keyTranslations + (index - 1);
    memmove(pos + 1, pos, (freeSlot - pos) * sizeof(KeySpec));
  }
  *pos = spec;    

  Serial.print(F("Defined keymap: ")); Serial.print(slotCnt + 1); Serial.print(':');
  pos->printDef();
  Serial.println();
  saveAll();
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
  saveAll();
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
  Serial.print(x + 1); Serial.print(':'); Serial.print(y + 1); Serial.print(':');
  if (matrix) {
    int w = ((lenOrMatrix & 0x0f) + 1) - x;
    int h = ((lenOrMatrix >> 4) + 1) - y;
    Serial.print(w); Serial.print(','); Serial.print(h);
  } else {
    Serial.print(lenOrMatrix);
  }
  Serial.print(':');
  Serial.print(target); Serial.print(':'); Serial.print(commandBase);
}

void commandPress() {
  int r = nextNumber();
  if (r < 1 || r > inputRows) {
    Serial.println(F("Bad row"));
    return;
  }
  int c = nextNumber();
  if (c < 1 || c > inputRows) {
    Serial.println(F("Bad col"));
    return;
  }
  byte on = true;
  switch (*inputPos) {
    case '0': case '-': case 'n': case 'u': case 'o':
    on = false;
  }
  pressKey(r - 1, c - 1, on);
}

