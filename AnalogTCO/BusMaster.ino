/**
 * Implementace "Bus mastera" ridiciho sbernici. 
 * Bus master je JEDINY kdo smi zahajit datovou vymenu. Ostatni stanice na RS485 smi vysilat jen, kdyz jim "master" posle vyzvu, a museji zacit
 * vysilat do urciteho casu (`ackTimeout` [ms]). V tomto jednoduchem provedeni smeji POUZE potvrdit prijem dat. Jde rozsirit na system, kde slave
 * muze oslovit dalsiho - tzn. komunikace kazdeho s kazdym, administrovana "masterem". 
 * 
 * Pokud dostane "master" pozadavek na vysilani, a muze, zahaji vysilani packetu. Ihned po dokonceni vysilani prejde do rezimu cteni a ceka na potvrzeni.
 * Nedojde-li potvrzeni do `ackTimeout` ms, zprava se povazuje za nedorucenou, a vysilani se bude opakovat. Opakovani se zahaji nejdrive `minRepeatDelay` ms
 * po posledni neprijate zprave, nejsou-li zadne zpravy k odeslani.
 * Jakmile se nepodari dorucit zpravu zarizeni X, jsou dalsi zpravy ve fronte pro totez zarizeni blokovane - poslou se az po uspesnem zaslani nedorucene zpravy.
 * Blokovani se uchovava v poli `blockedSlaves` a resetuje se na zacatku kazdeho cyklu pruchodu fronty.
 * 
 * POZNAMKA: muze nastat zahlceni, pokud vetsi pocet zarizeni neodpovida, a posila se mnoho zprav. Mozne reseni je resetovat frontu po urcitem poctu ms / zprav
 * a vynutit opakovani drive, ackoliv jsou ve fronte stale nikdy neodeslane zpravy (ty odejdou naposledy).
 * 
 * Pokud dojde k definitivnimu vyrazeni packetu, budou se (docasne, do zpracovani cele fronty) ignorovat vsechny packety pro dane zarizeni - vsechny se vyradi.
 * Selhane stanice se uchovavaji v poli `failedSlaves` a vymazou se na zacatku cyklu zpracovani fronty.
 * 
 * Pri vysilani se vysila vice byte najednou; kvuli omezeni pouzite knihovny mohou byt behem te doby vypnuta preruseni. Po uplynuti `maxTransmitTime` ms
 * se vysilani prerusi, at jiz byl packet odeslan nebo ne - dalsi volani transmitFrames() bude pokracovat.
 * 
 * Pokud dojde k neocekavanemu prijmu dat, blokuje se vysilani; prijimac se snazi "chytit" pomoci znacky na zasilana data. Budto nalezne znacku a konec ramce, nebo
 * se nechyti, a ceka se timeout po poslednim byte.
 * POZOR: muze dojit k uvaznuti, pokud nekdo nevychovany na sbernici vysila. Ale pokud to dela, stejne by se nedala data poslat, portoze by se jen poskodila.
 * 
 * Pouziti: zpravy k odeslani se zaradi do fronty pomoci `addMessage`. Pote (casem) se vyslou podle moznosti. Stornovat zpravu z fronty nejde (jde doplnit). Stav 
 * (uspesne odeslano, cekani, opakovani) se volajicimu neoznamuje; je nutne vycist z fronty. Je NUTNE periodicky volat 
 *    void transmitFrames();
 * ktera si ridi prijem i vysilani, a pocita timeouty. Po dosazeni konce fronty odesilanych zprav a uplynuti timeoutu 
 */

const boolean debugBusMaster = true;

const boolean printErrors = true;

/**
 * Doba cekani na ACK. Pote je packet povazovany za nedoruceny, a bude se opakovat.
 */
const int ackTimeout = 50;

/**
 * Max pocet opakovani. Po dosazeni se packet VYRADI.
 */
const int maxPacketRepeats = 3;

/**
 * Velikost bufferu pro prijem dat
 */
const int msgBufferSize = 160;


/**
 * Max pocet obsluhovanych zarizeni - pro vypocet velikosti poli
 */
const int maxSlaves = 16;
const int maxSlaves8 = (maxSlaves + 7) / 8;

/**
 * Doba po kterou se smi nepretrzite vysilat. Po uplynuti se vysilani prerusi,
 * aby mohlo Arduino delat i jine veci.
 */
const int maxTransmitTime = 30;

/**
 * Minimum delay from the last "normal" transmission
 */
const int minRepeatDelay = 20;

/**
   Maximum size of stalled data. If more data is not delivered
   to the slaves, the least recent stalled packet should be trashed.
*/
const int msgStalledSize = 40;

/**
   The actual msg buffer
*/
byte msgBuffer[msgBufferSize + 1];
const byte* msgBufferLimit = msgBuffer + msgBufferSize;

/**
 * Pointer to the 1st free position
 */
byte *msgBufferTop = msgBuffer;

/**
 * Head of the buffer to broadcast.
 */
byte *msgBufferHead = msgBuffer;

CommFrame*  sendPacket = (CommFrame*)msgBuffer;

byte  blockedSlaves[maxSlaves8];
byte  failedSlaves[maxSlaves8];

byte &busMasterId = eeData.busId;

/**
 *  Slave, kteremu se odeslala posledni data, 
 *  pro kontrolu ACK
 */
address_t xmitTarget;

/**
 * V modulu RS485; XOR/CRC checksum zaslany v packetu
 * do slave, mel by prijit v ACK.
 */
extern checksum_t xmitXor;

unsigned int lastTransmit = 0;
unsigned int startBufferTime = 0;

void clearBlockedSlaves() {
  for (int i = 0; i < sizeof(blockedSlaves); blockedSlaves[i] = failedSlaves[i]= 0, i++) ;
}

void resetBusMaster() {
  clearBlockedSlaves();
  sendPacket = NULL;
  busMasterId = 1;
}

void skipPacket() {
  CommFrame *f = sendPacket->next();
  byte l = sendPacket->bufferSize();
  if (debugBusMaster) {
    Serial.println(F("Queue packet"));
  }
  printBufStat();
  if (msgBufferHead < sendPacket) {
    if (debugBusMaster) {
      sendPacket->printStat();
      Serial.print(F("Moving blocked ")); Serial.print((int)sendPacket, HEX); Serial.print(':'); Serial.print((int)msgBufferHead, HEX); 
      Serial.print('-'); Serial.println(l); 
    }
    memmove(msgBufferHead, sendPacket, l);
    if (debugBusMaster) {
      ((CommFrame*)msgBufferHead)->printStat();
    }
  }
  msgBufferHead += l;
  sendPacket = f;
  recordStartTime(startBufferTime);
  if (debugBusMaster) {
    printBufStat();
  }
}

void printBufStat() {
  if (!debugBusMaster) {
    return;
  }
  Serial.print(F("Bufstat, start= ")); Serial.print((int)msgBuffer, HEX); 
  Serial.print(F(" end=")); Serial.print((int)msgBufferTop, HEX);
  Serial.print(F(" head=")); Serial.print((int)msgBufferHead, HEX);
  Serial.print(F(" send=")); Serial.print((int)sendPacket, HEX); Serial.print(F(" next=")); Serial.println((sendPacket != NULL) ? (int)sendPacket->next() : 0, HEX);
}

void checkAndRepeatFailed() {
  compactBuffer();
  if (!elapsedTime(startBufferTime, minRepeatDelay)) {
    return;
  }
  msgBufferHead = msgBuffer;
  if (msgBufferTop <= msgBuffer) {
    return;
  }
  clearBlockedSlaves();
  if (msgBufferTop > msgBuffer) {
    sendPacket = (CommFrame*)msgBuffer;
  } else {
    sendPacket = NULL;
  }
  if (debugBusMaster) {
    Serial.println(F("Resending")); 
    printBufStat();
  }
}

void printPacket(const CommFrame& f) {
  Serial.print(F("f:")); Serial.print(f.from); Serial.print(F("t:")); Serial.print(f.to);
  Serial.print(F(" l:")); Serial.print(f.len); 
  Serial.print(F(" r:")); Serial.print(f.retryCount); 
}

unsigned int startReceiverTime;
boolean masterReceiving = false;

/**
 * Errors propagated from isr
 */
volatile byte recvError = 0;

/**
 * The message received from ISR
 */
volatile const CommFrame* receivedMessage = NULL;


/**
 * Starts receiving bytes.
 */
void masterStartReceiver() {
  masterReceiving = true;
  recvError = 0;
  if (!isReceiving()) {
    startReceiver();  
  }
  recordStartTime(startReceiverTime);
  Serial.print("Master receiving: "); Serial.println(startReceiverTime);
}

void masterStopReceiver() {
  masterReceiving = false;
  recvError = 0;
  stopReceiver();
  startReceiverTime = 0;
}

void transmitFrames() {
  periodicReceiveCheck();
  if (isReceiving()) {
    if (startReceiverTime > 0 && elapsedTime(startReceiverTime, ackTimeout)) {
      if (debugBusMaster) {
        Serial.print(F("Timeout @start "));  Serial.println(currentMillisLow);
      }
      boolean r = masterReceiving;
      masterStopReceiver();
      if (r) {
        scheduleRepeat();
      }
    }
    // we are listening now.
    return;
  }
  readReceivedMessage();
  boolean cont = false;
  do {
    cont = false;
    if (isTransmitting()) {
      long lastTransmit = millis();
      // when transmitSingle returns true, the receiver is already activated
      // but bus master need to set up the variables ASAP, before this function ends.
      while (!transmitSingle(true)) {
        // transmit at most 5ms
        long l = millis();
        int d = l - lastTransmit;
        if (d >= maxContinuousTransfer) {
          if (debugBusMaster) {
            Serial.print(F("Transmission long: ")); Serial.println(d);
          }
          return;        
        }
      }
      if (debugBusMaster) {
        Serial.println(F("Complete -> listen"));
      }
      printBufStat();
      // switch to listening
      masterStartReceiver();
      return;
    }
    
    if (sendPacket == NULL || sendPacket >= msgBufferTop) {
      checkAndRepeatFailed();
      if (sendPacket == NULL) {
        return;
      }
      // fall through to send
    }
    while (sendPacket < msgBufferTop) {
      // normal data to transmit; check if the data targets a blocked client
      if (debugBusMaster) {
        Serial.println(F("Got packet: "));
        printPacket(*sendPacket);
        Serial.println();
      }
      byte t = sendPacket->to;
      if (readBit(blockedSlaves, t)) {
        if (debugBusMaster) {
          Serial.println(F("Slave blocked"));
        }
        // if a slave is blocked, some previous packet was not ACKed.
        // this will postpone all subsequent packets for that slave.
        if (readBit(failedSlaves, t)) {
          discardPacket();
        } else {
          skipPacket();
        }
      } else {
        if (debugBusMaster) {
          Serial.println(F("Starting transmission"));
        }
        printBufStat();
        // we have something to transmit
        xmitTarget = t;
        transmitFrame(sendPacket);
        cont = true;
        break;
      }
    }
  } while (cont);
  if (debugBusMaster) {
    Serial.println(F("Buffer exhausted"));
  }
  // reached end, buffer is compacted
  msgBufferTop = msgBufferHead;
  sendPacket = NULL;
}

void discardPacket() {
  if (debugBusMaster) {
    Serial.print(F("Drop packet: ")); Serial.println((int)sendPacket, HEX);
  }
  if (sendPacket == NULL) {
    return;
  }
  sendPacket = sendPacket->next();
  if (debugBusMaster) {
    printBufStat();
  }
}

void scheduleRepeat() {
  byte t = sendPacket->to;
  if (t != addressBroadcast && t < maxSlaves) {
    if (sendPacket->retryCount++ >= maxPacketRepeats) {
      if (printErrors) {
        Serial.print(F("Retry failed: ")); 
        sendPacket->printStat();
        Serial.println();
      }
      writeBit(failedSlaves, t, 1);
      discardPacket();
      return;
    }
    skipPacket();
    if (debugBusMaster) {
      Serial.print(F("Blocking slave ")); Serial.println(t);
    }
    writeBit(blockedSlaves, t, 1);
  }
}

/**
 * Reception of the message
 */
void onReceivedMessage(const CommFrame& frame) {
  receivedMessage = &frame;
}

void readReceivedMessage() {
  boolean r = masterReceiving;
  if (!r) {
    // neni zajimave, low-level prijimac vyhlasil chybu
    // kdyz se po nem nic nechtelo. Mozna by to ale chtelo
    // pockat s vysilanim, kdyz je "na drate" binec ?
    return;
  }
  if (recvError > 0) {
    if (debugBusMaster) {
      Serial.println(F("Error flag is set")); 
    }
    masterStopReceiver();
    scheduleRepeat();
    return;
  }
  if (receivedMessage == NULL) {
    // jeste zadna zprava nedorazila, jedeme dal...
    return;
  }
  if (debugBusMaster) {
    Serial.println(F("Received message")); 
  }
  masterStopReceiver();
  if (!r) {
    Serial.println(F("Unexpected: discard")); 
    return;
  }
  const volatile CommFrame& frame = *receivedMessage;
  receivedMessage = NULL;
  startReceiverTime = 0;
  if ((frame.len != sizeof(checksum_t)) || (frame.from != xmitTarget) || (frame.dataStart != xmitXor)) {
    // checksum OK, but bad data
    if (debugBusMaster) {
      frame.printStat();
      Serial.print(F("Bad ACK: ")); Serial.print(xmitXor, HEX); Serial.print(':'); Serial.print(frame.dataStart, HEX); 
      Serial.print(" f:"); Serial.print(frame.from); Serial.print(':'); Serial.println(xmitTarget);
    }
    scheduleRepeat();
  } else {
    discardPacket();
    if (debugBusMaster) {
      Serial.println(F("Got ACK"));
      printBufStat();
    }
  }
}

void printOutputBuffer() {
  if (!debugBusMaster) {
    return;
  }
  const CommFrame* pF = (CommFrame*)msgBuffer;
  Serial.println(F("Dumping buffer"));
  while (pF < msgBufferTop) {
    byte l = pF->len;
    if ((void*)pF == msgBufferHead) {
      Serial.print('*');
    }
    Serial.print('@'); Serial.print((int)pF, HEX); Serial.print('\t');
    Serial.print(F("From:")); Serial.print(pF->from); 
    Serial.print(F(" To:")); Serial.print(pF->to);
    Serial.print(F(" Len:")); Serial.print(l);
    Serial.print(F(" Retr:")); Serial.print(pF->retryCount); 
    Serial.print(' ');
    byte *d = &(pF->dataStart);
    Serial.print(*d++, HEX);
    if (l-- > 1) {
      Serial.print(' '); Serial.print(*d++, HEX);
    }
    if (l-- > 1) {
      Serial.print(' '); Serial.print(*d++, HEX);
    }
    if (l-- > 1) {
      Serial.print(' '); Serial.print(*d++, HEX);
    }
    Serial.println();
  }
}

void compactBuffer() {
  if (sendPacket <= msgBufferHead) {
    return;
  }
  int shiftBytes = ((byte*)sendPacket) - ((byte*)msgBufferHead);
  int len = msgBufferTop - ((byte*)sendPacket);
  if (len > 0) {
    if (debugBusMaster) {
      Serial.print("Compacting: "); Serial.print((int)sendPacket, HEX); Serial.print(':'); Serial.print(len); Serial.print('-'); Serial.print((int)msgBufferHead, HEX);
      Serial.print(" start:"); Serial.println((int)msgBuffer, HEX);
    }
    memmove(msgBufferHead, sendPacket, len);
  }
  sendPacket = (CommFrame*)msgBufferHead;
  msgBufferTop -= shiftBytes;
  if (msgBufferTop <= sendPacket) {
    sendPacket = NULL;
  }
  if (debugBusMaster) {
    Serial.print("Buf: "); Serial.print((int)msgBuffer, HEX); Serial.print(" head:"); Serial.print((int)msgBufferHead, HEX); 
    Serial.print(" send:"); Serial.print((int)sendPacket, HEX); Serial.print(" top:"); Serial.print((int)msgBufferTop, HEX);
    Serial.print(" lim:"); Serial.println((int)msgBufferLimit, HEX);
  }
}

/**
 *  Queues the message for sending
 */
void addMessage(const byte target, const byte sender, const byte* msg, byte len) {
  byte tlen = CommFrame::skipSize(len) + 1;
  if (debugBusMaster) {
    Serial.print("Adding msg: "); Serial.print("t :"); Serial.print(target); Serial.print(" s:"); Serial.print(sender);
    Serial.print(" data @"); Serial.print((int)msg, HEX); Serial.print(" l:"); Serial.println(len);
    Serial.print("Framelen: "); Serial.println(tlen);
  }
  if (!isBroadcast(sender) && target >= maxSlaves) {
    return;
  }
  if (sender>= maxSlaves) {
    return;
  }
  if ((msgBufferTop + tlen) >= msgBufferLimit) {
    if (debugBusMaster) {
      Serial.println("Buffer full, compacting");
    }
    compactBuffer();
    if ((msgBufferTop + tlen) >= msgBufferLimit) {
      // FIXME: should probably somehow alert
      Serial.println("Transmit buffer full");
      return;
    }
  }

  CommFrame& frame = *((CommFrame*)msgBufferTop);
  frame.retryCount = 0;
  frame.from = sender;
  frame.to = target;
  frame.len = len;
  memmove(&frame.dataStart, msg, len);
  if (debugBusMaster) {
    Serial.print(F("485-addMsg ")); frame.printStat();
  }
  msgBufferTop = (byte*)frame.next();
  if (sendPacket == NULL) {
    if (debugBusMaster) {
      Serial.println("Initiated transmission");
    }
    sendPacket = &frame;
  }
  if (debugBusMaster) {
    Serial.print("New frame @"); Serial.println((int)&frame, HEX);
    frame.printStat();
    Serial.print(" next @"); Serial.print((int)msgBufferTop, HEX); Serial.println();
    printBufStat();
  }
}

void onReceiveError(int reason) {
  recvError = reason;
}

