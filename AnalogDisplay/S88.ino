
/**
 * S88 je implementovano jako stavovy automat. Kazda iterace loop() provede jeden krok (stav). 
 * Kazdy krok se sestava z volani funkce a prodlevy pred dalsim krokem. Volana funkce vrati cislo nasledujiciho
 * krok nebo 0xff, pokud se ma provest dalsi krok. Pokud jiz dalsi stav/krok neni, zacne se opet od zacatku.
 * 
 * Kdyz jeste neuplynula prodleva pred dalsim krokem, automat se ihned vraci.
 */

///////////////////////// Configuration details for S88 //////////////////////////////////////

const boolean debugS88 = false;
const boolean debugS88Pins = false;
const boolean testS88 = false;

volatile int input = 7;


/**
 * Pocet mereni napeti v kolejich, ktere presahuje nastaveny limit
 */
volatile long intCount = 0;

/**
 * Minimalni pozadovane napeti v kolejich. U pulzniho regulatoru 
 * to muze byt i docela hodne
 */
volatile long trackAboveLimit = 0;

/**
 * Posledni zaznamenane napeti; diagnostika
 */
volatile int lastVoltage = 0;


/**
 * Debounced state of the sensors, one bit per sensor. Use bitRead/bitWrite to change data
 */
byte s88DebouncedState[s88ModuleCount];

/**
 * Specific implemementation of the debouncer: stableChange will propagate the data to output.
 */
class S88toOutputDebouncer : public Debouncer {
  public:
  S88toOutputDebouncer(byte modCount, byte* debouncedState) : Debouncer(modCount, debouncedState) {}
  virtual boolean stableChange(byte number, boolean nState);
};

S88toOutputDebouncer s88Debounce(s88ModuleCount, s88DebouncedState);


/**
 * State of a S88 automaton. But "state" is greatly overloaded term, so let's have Phase.
 */
struct S88Phase {
  /**
   * The actula handler for the phase
   */
  const byte (*handler)();

  /**
   * microseconds delay after the phase
   */
  const unsigned int delay;
  
  S88Phase(byte (*aHandler)(), unsigned int aDel) : handler(aHandler), delay(aDel) /*, id(none)*/ {}
};

void resetIntCounters() {
  noInterrupts();
  if (intCount > 100) {
    intCount = 0;
    trackAboveLimit = 0;  
  }
  interrupts();
}


void processVoltage() {
  int v = analogRead(S88TrackInput);
  intCount++;
  lastVoltage = v;
  intCount++;
  if (v >= eeData.minTrackVoltage) {
    trackAboveLimit++;
  }
}

boolean checkTrackPowered() {
  if (eeData.enableTrack == 0) {
    return true;
  }
  if (debugS88) {
    Serial.print("Voltage: ");
    Serial.println(lastVoltage);
  }
  int total = intCount;
  int okCount = trackAboveLimit;
  int percent = (okCount * (long)100) / total;

  if (debugS88) {
    Serial.print("Voltage %: ");
    Serial.println(percent);
  }
  resetIntCounters();
  return percent >= eeData.minTrackPercent;
}

/**
 * Definition of the S88 automaton. The machine will wrap back to state 0 after the end of the array.
 */
const S88Phase  automatonStates[] = {
  // make clock low
  S88Phase(&s88ClockLow, 50),
  // initiate LOAD
  S88Phase(&s88LoadHigh, 20),
  // clock goes high to load shift register
  S88Phase(&s88ClockHigh, 50),
  // clock low
  S88Phase(&s88ClockLow, 50),
  
  S88Phase(&s88LoadLow, 0),
  // now reset goes high
  S88Phase(&s88ResetHigh, 40),
  S88Phase(&s88ResetLow, 50),

  S88Phase(&s88DataInit, 20),
  // during setup, clock is already low at this point
  S88Phase(&s88ClockLow, 30),
  // now the normal read cycle may happen
  S88Phase(&s88ClockHigh, 30),
  S88Phase(&s88ReadData, 0),

  S88Phase(&s88ClockLow, 30),
  S88Phase(&s88Finish, 5000)   // wait ~5k usec = 5msec.
};

void setupTrackInput() {
  pinMode(S88TrackInput, INPUT);
}

void setupS88Ports() {
  pinMode(S88Input, INPUT);
  pinMode(S88Clock, OUTPUT);
  pinMode(S88Latch, OUTPUT);
  pinMode(S88ResetAll, OUTPUT);
  pinMode(S88ResetTrack, OUTPUT);

  digitalWrite(S88Latch, LOW);
  digitalWrite(S88Clock, LOW);
  digitalWrite(S88ResetAll, LOW);
  digitalWrite(S88ResetTrack, LOW);

  setupTrackInput();

  registerLineCommand("SENS", &commandTrackSensitivity);

  s88Debounce.setOffCounter(S88OffDebounce);
}

void resetS88() {
  for (int i = 0; i < s88ModuleCount; i++) {
    s88DebouncedState[i] = 0;
  }
}

boolean S88toOutputDebouncer::stableChange(byte number, boolean nState) {
  if (!Debouncer::stableChange(number, nState)) {
    return false;
  }
  setOutputFromSensor(number, nState);
  return true;
}

/**
 * Stav automatu S88
 */
byte s88CurrentState = 0;
/**
 * Microsekundy od posledniho zpracovani S88
 */
unsigned long s88LastMicros = 0;
/**
 * Kolik mikrosekund se musi pockat do nasledujiciho kroku
 */
byte s88WaitMicros = 0;

/**
 * Accumulates received S88 bits
 */
byte s88Received;

/** 
 *  Counts 0-7 before bits are transferred to change bufer
 */
byte s88BitCount;

/**
 * For testing: one bit of S88 input
 */
boolean testS88Input;

/**
 * Module number of the current S88.
 */
byte s88ModuleNumber = 0;

/**
 * Millis time for debouncing S88
 */
unsigned int s88DebounceTime;

byte s88DataInit() {
  if (debugS88) {
    Serial.println(F("S88: init data"));
  }
  s88Received = 0;
  s88BitCount = 0;
  // hodiny presly do stavu HIGH, zatimco PE signal uz byl LOW,
  // takze nejvyssi bit narotoval na Q7 a da se precist.
  return s88ReadData();
}

byte s88ReadData() {
  s88Received <<= 1;
  boolean b;
  if (testS88) {
    b = testS88Input;
  } else {
    if (S88Input > 13) {
      /**
       * Interpret approx 2.5V from the analog pin as true
       */
      b = analogTTLRead(S88Input);
    } else {
      b = digitalRead(S88Input);
    }
    
  }
  if (b) {
    s88Received |= 0x01;
  }
  if (debugS88Pins) {
    Serial.print(F("S88: Read #")); Serial.print(s88ModuleNumber); Serial.print('/'); Serial.print(s88BitCount); Serial.print('='); Serial.println(b);
    Serial.print(F("S88: recvd:")); Serial.println(s88Received, HEX);
  }
  if (++s88BitCount < 8) {
    return 0x08;
  }
  if (debugS88Pins) {
    Serial.println(F("S88: received full byte"));
  }
  if (debugS88) {
    Serial.println(s88Received, BIN); 
  }
  // process the byte as a change
  s88Debounce.debounce(s88ModuleNumber, &s88Received, sizeof(s88Received));

  s88ModuleNumber++;
  if (s88ModuleNumber >= s88ModuleCount) {
    s88ModuleNumber = 0;
    // advance, wait a little then repeat.
    return 0xff;
  }
  return 0x07;
}

byte s88Finish() {
  return 0xff;
}

/**
 * Entry to the state automaton
 */
boolean processS88Bus() {
  unsigned long usec = micros();
  if (s88WaitMicros != 0) {
      if (usec < s88LastMicros) {
          // Preteceni. Uplynuly cas spocitame jako pocet usec do max hodnoty plus
          // hodnotu casovace po preteceni.
          unsigned long diff = (-s88LastMicros) + usec;
          if (diff < s88WaitMicros) {
            return false;
          }
      } else if (((usec - s88LastMicros) < s88WaitMicros)) {
          return false;
      }
  }
  
  s88WaitMicros = 0;
  const S88Phase& s = automatonStates[s88CurrentState];
  if (s.handler == NULL) {
    return false;
  }
  s88CurrentState++;
  if (s88CurrentState >= (sizeof(automatonStates) / sizeof(automatonStates[0]))) {
    if (debugS88) {
      Serial.println(F("S88 loop start"));
    }
    s88CurrentState = 0;
  }
  s88WaitMicros = s.delay;
  byte res = (*(s.handler))();
  if (debugS88Pins) {
    Serial.print(F("S88 delay: ")); Serial.println(s.delay);
  }
  if (res < (sizeof(automatonStates) / sizeof(automatonStates[0]))) {
    s88CurrentState = res;
  }
  if (debugS88Pins) {
    Serial.print(F("curState: ")); Serial.print(s88CurrentState); Serial.print(F(" res: ")); Serial.println(res);
  }
  s88LastMicros = usec;
  if (s88CurrentState == 0) {
    if (elapsedTime(s88DebounceTime, delayBetweenDebounceTick)) {
      s88Debounce.tick();
    }
  }
  return s88CurrentState == 0;
}


void dumpTrackSensitivity() {
  Serial.print(F("SENS:")); Serial.print(eeData.minTrackVoltage); Serial.print(':'); Serial.println(eeData.minTrackPercent);
}

void commandTrackSensitivity() {
  int limit = nextNumber();
  if (limit == -2) {
    dumpTrackSensitivity();
    return;
  }
  int percent = nextNumber();

  if ((limit < 10) || (limit >= 800)) {
    Serial.println(F("Bad limit"));
    return;
  }
  if ((percent < 0) || (percent > 100)) {
    Serial.println(F("Bad percentage"));
    return;
  }
  eeData.minTrackPercent = percent;
  eeData.minTrackVoltage = limit;
}

// ------------------------ Indivudual port manipulations ---------------

byte s88LoadHigh() {
  if (debugS88Pins) {
    Serial.println(F("S88: Load -> HIGH"));
  }
  digitalWrite(S88Latch, HIGH);
  return 0xff;
}

byte s88LoadLow() {
  if (debugS88Pins) {
    Serial.println(F("S88: Load -> LOW"));
  }
  digitalWrite(S88Latch, LOW);
  return 0xff;
}

byte s88ClockHigh() {
  if (debugS88Pins) {
    Serial.println(F("S88: Clock -> HIGH"));
  }
  digitalWrite(S88Clock, HIGH);

  if (debugS88Pins) {
    boolean b;
    if (S88Input > 13) {
      /**
       * Interpret approx 2.5V from the analog pin as true
       */
      b = analogTTLRead(S88Input);
    } else {
      b = digitalRead(S88Input);
    }
    Serial.print(F("** Read @clock: ")); Serial.println(b);
  }
  return 0xff;
}

byte s88ClockLow() {
  if (debugS88Pins) {
    Serial.println(F("S88: Clock -> LOW"));
  }
  digitalWrite(S88Clock, LOW);
  return 0xff;
}

byte s88ResetHigh() {
  if (checkTrackPowered()) {
    if (debugS88Pins) {
      Serial.println(F("S88: Reset -> HIGH"));
    }
    digitalWrite(S88ResetAll, HIGH);
  } else if (debugS88) {
    Serial.println(F("No track power"));
  }
  return 0xff;
}

byte s88ResetLow() {
  if (debugS88Pins) {
    Serial.println(F("S88: Reset -> LOW"));
  }
  digitalWrite(S88ResetAll, LOW);
  return 0xff;
}


