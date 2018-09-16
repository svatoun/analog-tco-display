
void prepareS88() {
  while (s88CurrentState != 0x09) {
      processS88Bus();
  }
}

void loadOneBit(boolean state) {
  prepareS88();
  Serial.print("Load: "); Serial.println(state);
  testS88Input = state;
  s88LastMicros = 0;
  processS88Bus();
}

void t88LoadState(unsigned long state) {
  unsigned long m = 0x01;
  while (m > 0) {
    loadOneBit(state & m);
    m <<= 1;
  }
  for (byte i = 32; i < s88ModuleCount * 8; i++) {
    loadOneBit(0);
  }

  while (s88CurrentState != 0) {
      processS88Bus();
  }
}

void inputS88Load1() {
  t88LoadState(0xaa55);
}

int rpt = 0;

void testingS88() {
  pinMode(S88Input, INPUT);
  int a = analogRead(S88Input);
  Serial.println(a);
  if (rpt > 0) {
    return;
  }
  for (byte x = 0; x < 20; x++) {
  do {
    processS88Bus();
  } while (s88CurrentState != 1);
  }
  /*
  inputS88Load1();
  s88Debounce.tick();
  t88LoadState(0xaa55 ^ 0x2044);
  s88Debounce.tick();
  s88Debounce.print();
  s88Debounce.tick();
  t88LoadState(0xaa55 ^ 0x2044 ^ 0x0200);
  s88Debounce.tick();
  s88Debounce.tick();
  */
}

