extern unsigned long currentMillis;
extern unsigned int currentMillisLow;
extern byte ioRowIndex;
extern byte ioColumnIndex;

const byte inputRowSize = ((inputColumns + 7) / 8);
const byte inputColumnsRounded = ((inputColumns + 7) / 8) * 8;
const byte inputByteSize = (inputRows * inputColumns + 7) / 8;

const byte addressBroadcast = 0xff;

#ifdef FAST_ADDREESS
#if defined(__AVR_ATmega328P__)  // Arduino UNO, NANO
/**
 * Write all 4 address lines at once
 */
inline void selectDemuxLine(byte line) {
  /*
  byte rev = 0;
  // because of design error, address lines are reversed compared to bit order
  if (line & 0x01) rev |= 8;
  if (line & 0x02) rev |= 4;
  if (line & 0x04) rev |= 2;
  if (line & 0x08) rev |= 1;
  byte pb = PORTB & 0xf0;
  pb |= (rev & 0x0f);
  PORTB = pb;
  */
}
#else
#   error "Unsupported board version"
#endif
#else 
inline void selectDemuxLine(byte line) {
  if (line < 8) {
    digitalWrite(DemuxAddr0, line & 0x01);
    digitalWrite(DemuxAddr1, line & 0x02);
    digitalWrite(DemuxAddr2, line & 0x04);
    digitalWrite(DemuxAddr3, 1);
  }
}
#endif

void recordStartTime(unsigned int& store) {
  store = currentMillis & 0xffff;
}

void updateTime() {
  currentMillis = millis();
  currentMillisLow = currentMillis & 0xffff;
}

/**
 * Quick check of the elapsed time, uses integer-based time to record delays in mills,
 * NO LONGER THAN 0xffff millis = 65 seconds.
 */
boolean elapsedTime(unsigned int &msecStart, unsigned int d) {
  unsigned int x;
  if (msecStart > currentMillisLow) {
    // currentMillisLow overflowed, so the delay
    // is from msecStart to 0x10000 (complement of msecStart) + currentMillisLow (which is
    // above 0x10000.
    x = currentMillisLow + (~msecStart);
  } else {
    x = currentMillisLow - msecStart;
  }
  boolean ret = x >= d;
  if (ret) {
    recordStartTime(msecStart);
  }
  return ret;
}

boolean readBit(const byte* storage, int index) {
  byte i = index >> 3;
  byte m = 1 << (index & 0x07);
  return (*(storage + i) & m) > 0;
}

boolean writeBit(const byte* storage, int index, boolean state) {
  byte i = index >> 3;
  byte m = 1 << (index & 0x07);
  byte *p = storage + i;

  if (state) {
    *p = (*p & ~m) | m;
  } else {
    *p = (*p & ~m); 
  }
}

/** 
 *  Translation of physical keys into target:command
 */
struct KeySpec {
  byte  x : 4;
  byte  y : 4;     
  boolean matrix : 1 ;      // 1
  byte  lenOrMatrix : 8;    // 8
  byte  commandBase : 6;    
  byte  target : 5;         // 5 ... 2 bit remain

  boolean isEmpty() {
    return target == 0;
  }

  void range(byte s, byte len, byte t, byte b) {
    y = (s / inputColumnsRounded);
    x = s - (y * inputColumnsRounded);
    matrix = 0;
    lenOrMatrix = len;
    target = t;
    commandBase = b;
  }

  void rectangle(byte ax, byte ay, byte w, byte h, byte t, byte b) {
    x = ax; y = ay;
    matrix = 1;
    lenOrMatrix = w | (h << 4);
    target = t;
    commandBase = b;
    Serial.print("Rectangle: "); Serial.println(lenOrMatrix, HEX);
  }

  void printDef();
};

static_assert(sizeof(KeySpec) > 3, "Large keyspec");


byte analogTTLRead(byte pin) {
  return (analogRead(pin) > ((1024 / 50) * 25)) ? 1 : 0;
}

struct RemoteCommand {
  byte    operation;
  byte    commandId;
  boolean pressed;

  RemoteCommand(byte aop, byte acommand, boolean apress) : operation(aop), commandId(acommand), pressed(apress) {}
};


