extern unsigned long currentMillis;
extern unsigned int currentMillisLow;
extern byte ioRowIndex;
extern byte ioColumnIndex;

const byte inputRowSize = (inputColumns + 7 / 8);
const byte inputColumnsRounded = (inputColumns + 7 / 8) * 8;
const byte inputByteSize = (inputRows * inputColumns + 7) / 8;

const byte addressBroadcast = 0xff;

#if defined(__AVR_ATmega328P__)  // Arduino UNO, NANO
/**
 * Write all 4 address lines at once
 */
inline void selectDemuxLine(byte line) {
  byte pb = PORTB & 0xf0;
  pb |= (line & 0x0f);
  PORTB = pb;
}
#else
#   error "Unsupported board version"
#endif

void recordStartTime(unsigned int& store) {
  store = currentMillis & 0xffff;
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
  byte  lenOrMatrix : 6;    // 8
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
    lenOrMatrix = (w - 1 + ax) | ((h - 1 + ay) << 4);
    target = t;
    commandBase = b;
  }

  void printDef();
};


byte analogTTLRead(byte pin) {
  return (analogRead(pin) > ((1024 / 50) * 25)) ? 1 : 0;
}

struct RemoteCommand {
  byte    operation;
  byte    commandId;
  boolean pressed;

  RemoteCommand(byte aop, byte acommand, boolean apress) : operation(aop), commandId(acommand), pressed(apress) {}
};


