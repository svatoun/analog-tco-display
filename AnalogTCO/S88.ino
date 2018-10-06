
///////////////////////// Configuration details for S88 //////////////////////////////////////

const boolean debugS88 = true;
const boolean debugS88Pins = false;
const boolean testS88 = false;

volatile int input = 7;

/**
 * Minimum 20ms between debounce tick.
 */
const int delayBetweenDebounceTick = 20;

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

volatile int mux;


volatile long intCount = 0;
volatile long trackAboveLimit = 0;

void resetIntCounters() {
  noInterrupts();
  if (intCount > 100) {
    intCount = 0;
    trackAboveLimit = 0;  
  }
  interrupts();
}

void startAnalogRead(int channel) {
  noInterrupts();
  ADMUX = (mux & ~0x07) | channel;
  ADCSRA |= (1 << ADSC);  // start ADC measurements 
  interrupts();
}

volatile int lastVoltage = 0;

// Interrupt po zpracovani ADC vstupu
ISR(ADC_vect)
{
  /*
  int readLo = ADCL; // ADCL must be read first; reading ADCH will trigger next coversion.
  int readHi = ADCH; 
  int v = (readHi << 8) | readLo;
  noInterrupts();
  lastVoltage = v;
  intCount++;
  if (v >= eeData.minTrackVoltage) {
    trackAboveLimit++;
  }
  ADMUX = (mux & ~0x07) | 7;
  ADCSRA |= (1 << ADSC);  // start ADC measurements.
  interrupts();
  */
  int readLo = ADCL; // ADCL must be read first; reading ADCH will trigger next coversion.
  int readHi = ADCH; 
  intCount++;
  int v = (readHi << 8) | readLo;
//  accum += v;
  startAnalogRead(7);
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

  S88Phase(&s88DataInit, 0),
  // during setup, clock is already low at this point
  S88Phase(&s88ClockLow, 30),
  // now the normal read cycle may happen
  S88Phase(&s88ClockHigh, 30),
  S88Phase(&s88ReadData, 0),

  S88Phase(&s88ClockLow, 30),
  S88Phase(&s88Finish, 5000)   // wait ~5k usec = 5msec.
};

void setupTrackInput() {
#if 0
  // --------------- Shamelessly copied from http://yaab-arduino.blogspot.cz/2015/02/fast-sampling-from-analog-input.html
  // clear ADLAR in ADMUX (0x7C) to right-adjust the result
  // ADCL will contain lower 8 bits, ADCH upper 2 (in last two bits)
  ADMUX &= B11011111;
  
  // Set REFS1..0 in ADMUX (0x7C) to change reference voltage to the
  // proper source (01)
  ADMUX |= B01000000;
  
  // Clear MUX3..0 in ADMUX (0x7C) in preparation for setting the analog
  // input
  ADMUX &= B11110000;
  
  // Set ADEN in ADCSRA (0x7A) to enable the ADC.
  // Note, this instruction takes 12 ADC clocks to execute
  ADCSRA |= B10000000;
  
  // Set ADATE in ADCSRA (0x7A) to enable auto-triggering.
  ADCSRA |= B00100000;
  
  // Clear ADTS2..0 in ADCSRB (0x7B) to set trigger mode to free running.
  // This means that as soon as an ADC has finished, the next will be
  // immediately started.
  ADCSRB &= B11111000;

  // Set the Prescaler to 32 (16000KHz/32 = 500KHz)
  ADCSRA = (ADCSRA & B11111000) | B00000101;
  
  // Set ADIE in ADCSRA (0x7A) to enable the ADC interrupt.
  // Without this, the internal interrupt will not trigger.
  ADCSRA |= B00001000;
  
  // Enable global interrupts
  // AVR macro included in <avr/interrupts.h>, which the Arduino IDE
  // supplies by default.

  // save the ADMUX value, will be combined with desired analog input later.
  mux = ADMUX;

  // start from input #0
  input = 7;
  sei();
  startAnalogRead(input);
  delay(10);
#endif
}

void setupS88Ports() {
  pinMode(S88Input, INPUT);
  pinMode(S88Clock, OUTPUT);
  pinMode(S88Latch, OUTPUT);
  pinMode(S88ResetAll, OUTPUT);
  pinMode(S88ResetTrack, OUTPUT);

  pinMode(S88TrackInput, INPUT);

  setupTrackInput();

  registerLineCommand("SENS", &commandTrackSensitivity);
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
 * State of the reading automaton
 */
byte s88CurrentState = 0;
/**
 * Micros after the last S88 processing
 */
unsigned long s88LastMicros = 0;
/**
 * How many micros to wait before next processing step
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
  return 0xff;
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
    Serial.print(F("S88: got ")); Serial.println(s88Received, BIN);
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
  if (s88WaitMicros != 0 && ((usec - s88LastMicros) < s88WaitMicros)) {
    return false;
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


