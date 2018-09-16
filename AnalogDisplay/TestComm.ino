
void setMillis(unsigned long l) {
  currentMillis = l;
  currentMillisLow = l & 0xffff;
}

void testCommunication() {
  transmitFrames();
  transmitFrames();
  RemoteCommand cmd(9, 0x55, 9);
  RemoteCommand cmd2(3, 0x33, 0);
  addMessage(1, 2, (byte*)&cmd, sizeof(cmd));
  addMessage(3, 2, (byte*)&cmd2, sizeof(cmd2));
  
  transmitFrames();
  transmitFrames();

  isrReceiveData(0x7f);
  isrReceiveData(0x01);
  isrReceiveData(2);  
  isrReceiveData(1);
  isrReceiveData(0x22);
  isrReceiveData(0x5f);

  transmitFrames();
  transmitFrames();

  isrReceiveData(0x7f);
  setMillis(1000);
  
  transmitFrames();
  
  setMillis(1200);
  isrReceiveData(0x01);
  
  transmitFrames();

  isrReceiveData(2);  
  isrReceiveData(3);
  isrReceiveData(0x4d);
  isrReceiveData(0x31);

  transmitFrames();
  transmitFrames();
  

  setMillis(2000);

  transmitFrames();
  transmitFrames();

  isrReceiveData(0x7f);
  isrReceiveData(0x01);
  isrReceiveData(2);  
  isrReceiveData(3);
  isrReceiveData(0x4d);
  isrReceiveData(0x32);

  transmitFrames();
  transmitFrames();
/*
  isrReceiveData(0x00);
  isrReceiveData(0x05);
  isrReceiveData(0x7f);
  isrReceiveData(22);

  */
}

void testCommunication2() {
  transmitFrames();
  transmitFrames();
  transmitFrames();

  RemoteCommand cmd(9, 0x55, 9);
  RemoteCommand cmd2(3, 0x33, 0);

  Serial.println(sizeof(cmd));
  addMessage(1, 2, (byte*)&cmd, sizeof(cmd));
  addMessage(3, 2, (byte*)&cmd2, sizeof(cmd2));

  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();


  isrReceiveData(0x00);
  isrReceiveData(0x05);
  isrReceiveData(0x7f);

  isrReceiveData(22);


  for (int i = 0; i < CommFrame::frameSize(22) - 1; i++) {
    isrReceiveData(1);
  }
  
  RemoteCommand cmd3(4, 0x11, 1);
  addMessage(1, 2, (byte*)&cmd3, sizeof(cmd3));

  startReceiverTime = 1;
  setMillis(1000);

  transmitFrames();

  isrReceiveData(0xaa);

  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();

  isrReceiveData(0x7f);
  isrReceiveData(0x01);
  isrReceiveData(2);  
  isrReceiveData(3);
  isrReceiveData(0x4d);
  isrReceiveData(0x32);

  transmitFrames();
  transmitFrames();
  transmitFrames();
  transmitFrames();

  startBufferTime = 0;
  setMillis(1000);

  recordStartTime(lastTransmit);
  transmitFrames();
  transmitFrames();
  transmitFrames();
  
  isrReceiveData(0x7f);
  isrReceiveData(0x01);
  isrReceiveData(2);  
  isrReceiveData(1);
  isrReceiveData(0x22);
  isrReceiveData(0x5f);

  transmitFrames();
  transmitFrames();

  isrReceiveData(0x7f);
  isrReceiveData(0x01);
  isrReceiveData(2);  
  isrReceiveData(1);
  isrReceiveData(0x6b);
  isrReceiveData(0x16);
  
  transmitFrames();
  transmitFrames();
  }

