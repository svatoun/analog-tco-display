const boolean debugDebouncer = false;

Debouncer::Debouncer(byte aCount) : Debouncer(aCount, NULL) {
}

inline byte readNibble(const byte* a, byte index) {
  byte r = *a;
  return (index & 0x01) ? (r >> 4) : (r & 0x0f);
}

inline byte writeNibble(byte* a, byte index, byte val) {
  byte r = *a;
  byte m;
  val &= 0x0f;
  if ((index & 0x01)) {
    r = (r & 0x0f) | (val << 4);
  } else { 
    r = (r & 0xf0) | val;
  }
  *a = r;
  return r;
}


Debouncer::Debouncer(byte aCount, byte* aState) : stateBytes(aCount), stableState(aState), onCounter(4), offCounter(4) {
  byte s = aCount * 2 + (aCount * 4);
  changes = new byte[s];
  rawState = changes + aCount;
  counterNibbles = rawState + aCount;

  for (byte c = s, *p = changes; c > 0; p++, c--) {
    *p = 0;
  }
  for (byte *nb = counterNibbles; nb < (counterNibbles + ((aCount + 1) / 2)); nb++) {
    *nb = 0;
  }
}

void Debouncer::debounce(byte inputStart8, const byte* raw, byte rawByteSize) {
  if (inputStart8 >= stateBytes) {
    if (debugDebouncer) {
      Serial.print(F("*debounce error: st:")); Serial.print(inputStart8); Serial.print(F(" bytes: ")); Serial.println(stateBytes);
    }
    return; 
  }
  if (debugDebouncer) {
    Serial.print(F("debounce: in8:")); Serial.print(inputStart8); Serial.print(F(" sz:")); Serial.println(rawByteSize);    
    Serial.print(F("Complete state: "));
    for (int i = 0; i < stateBytes; i++) {
      Serial.print(rawState[i], BIN); Serial.print(' ');
    }
    Serial.println();
  }
  
  byte *p = rawState + inputStart8;
  byte *cn = counterNibbles + (inputStart8 << 2);
  for (byte c = 0; c < rawByteSize; c++) {
    byte r = *raw;
    byte x = *p ^ r;
    *p = r;
    curNibble = cn;
    reportByteChange(inputStart8, r, x);
    raw++;
    p++;    
    inputStart8++;
    cn += 4;
  }
}

void Debouncer::reportByteChange(byte n8, byte nstate, byte mask) {
  byte x = mask;
  byte n = n8 << 3;
  if (debugDebouncer) {
    Serial.print(F("s88byte: n8:")); Serial.print(n8); Serial.print(F(" n:")); Serial.print(n); Serial.print(F(" r:")); Serial.print(nstate, BIN); Serial.print(F(" x:")); Serial.println(x, BIN);
  }
  while (x != 0) {
    if (x & 0x01) {
      reportChange(n, nstate & 0x01);
    }
    x >>= 1;
    nstate >>= 1;
    if (n & 0x01) {
      curNibble++;
    }
    n = n + 1;
  }
  changes[n8] |= mask;
}

void Debouncer::reportChange(byte n, boolean state) {
  if (debugDebouncer) {
    Serial.print(F("s88Change: n:")); Serial.print(n); Serial.print(" c:"); Serial.print(state ? onCounter : offCounter); Serial.print(F(" s:")); Serial.println(state);
  }
  writeNibble(curNibble, n, state ? onCounter : offCounter);
}

byte dummyByte;

void Debouncer::tick() {
  byte *pchg = changes;
  byte *cn= counterNibbles;
  for (byte i = 0; i < stateBytes; i++, pchg++) {
    byte m = (*pchg);
    if (debugDebouncer) {
      Serial.print(F("tick: n8: ")); Serial.print(i); Serial.print(F(" m:")); Serial.println(m, BIN);
    }
    if (m == 0) {
      cn += 4;
      continue;
    }
    byte n = i * 8;
    byte rs = rawState[i];
    byte mask = 0x01;
    curNibble = cn;
    while (mask != 0 && mask <= m) {
      byte c = readNibble(counterNibbles + (n / 2), n);
      if (debugDebouncer) {
        Serial.print(F("tick: n:")); Serial.print(n); Serial.print(F(" nib:")); Serial.print(i * 4); Serial.print(F(" c:")); Serial.println(c);
      }
      if (c > 0) {
        if (--c == 0) {
          stableChange(n, rs & 0x01);
        } else {
          m &= ~mask;
        }
        writeNibble(curNibble, n, c); 
      }
      mask = mask << 1;
      rs >>= 1;
      if (n & 0x01) {
        curNibble++;
      }
      n++;
    }
    cn += 4;
    byte *pstable = (stableState == NULL) ? &dummyByte : stableState + i;
    *pchg &= ~m;
    if (debugDebouncer) {
      Serial.print(F("stable: pstate:")); Serial.print(*pstable, BIN); Serial.print(F(" mask:")); Serial.print(m, BIN); Serial.print(F(" r:")); Serial.print(rawState[i], BIN);
      Serial.print(F(" chg:")); Serial.println(*pchg, BIN);
    }
    *pstable = (*pstable & ~m) | (rawState[i] & m);
  }

  print();
}

boolean Debouncer::stableChange(byte input, boolean state) {
    if (stableState != NULL) {
      if (readBit(stableState, input) == state) {
        return false;
      }
      writeBit(stableState, input, state);
    }
    if (debugDebouncer) {
      Serial.print(F("stable change: n:")); Serial.print(input); Serial.print(F(" s:")); Serial.println(state);
    }
    return true;
}

void Debouncer::print() {
  if (!debugDebouncer) {
    return;
  }
  Serial.print(F("Debounced state: "));
  if (stableState != NULL) {
    for (int i = 0; i < stateBytes; i++) {
      Serial.print(stableState[i], BIN); Serial.print(' ');
    }
  }
  Serial.println();
  Serial.print(F("Raw state: "));
  for (int i = 0; i < stateBytes; i++) {
    Serial.print(rawState[i], BIN); Serial.print(' ');
  }
  Serial.println();
  Serial.print(F("Pending changes:"));
  for (int i = 0; i < stateBytes; i++) {
    Serial.print(changes[i], BIN); Serial.print(' ');
  }
  Serial.println();
  Serial.println(F("Nibbles:"));
  byte* nc = counterNibbles;
  int writeCount = 0;
  for (int i = 0; i < stateBytes * 4; i++) {
    Serial.print(counterNibbles[i], HEX); Serial.print('-');
  }
  Serial.println();
  for (int i = 0; i < stateBytes * 8; i++) {
    byte cnt = readNibble(nc, i);
    if (i & 0x01) {
      nc++;
    }

    if (cnt == 0 && writeCount == 0) {
      continue;
    }

    if (writeCount == 0) {
      Serial.print('@'); Serial.print(i); Serial.print('=');
    } 
    if (writeCount > 0) {
      Serial.print('-');
    }
    Serial.print(cnt);
    if (++writeCount == 16) {
      writeCount = 0;
      Serial.println();
    }
  }
}

