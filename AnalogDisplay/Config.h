
/**
 * Zpozdeni pred zpracovanim dalsiho radku - behem te doby LEDky radku
 * sviti. 0 znamena, ze se ihned bude zpracovavat dalsi radek. Doba se udava
 * v ms
 */
const int ioRowSwitchDelay = 0;

const int keyboardDebounceTime = 20;
const int maxContinuousTransfer = 30;

const byte s88ModuleCount = 1;

/**
 * Velikost prijmoveho bufferu v byte. Musi byt delsi nez nejdelsi zpracovavany packet.
 * Delsi packety se ani nezaznamenavaji a zahazuji rovnou.
 */
const int recvBufferSize = 20;

////////////////////// S88 input pin assignments ///////////////////////
/**
 * Vstup z S88 sbernice
 */
const int S88Input = A6;

/**
 * Hodiny pro S88 
 */
const int S88Clock = A5;

/**
 * LOAD signal. Kdyz se zmeni na HIGH, S88 moduly nactou data ze stradacu
 */
const int S88Latch = A4;

/**
 * RESET signal, na std. vodici #6
 */
const int S88ResetAll = A3;

/**
 * RESET signal, ale jen je-li v kolejich napeti
 */
const int S88ResetTrack = A2;

/**
 * Vstup pro mereni napeti v kolejich
 */
const int S88TrackInput = A7;

/**
 * Minimum 20ms between S88 debounce tick.
 */
const int delayBetweenDebounceTick = 20;

/**
 * Kolik tiku musi byt stabilni zmena na 0 pred prijetim stavu
 */
const int S88OffDebounce = 5;


////////////////////// Feedback POWER+ SHIFT REGISTER ///////////////////////
const int SHIFTREG_CLOCK_COMMON = A1;

/**
 * Clock for shift register which controls power+ for feedback LEDs
 */
const int FbPowerClock = SHIFTREG_CLOCK_COMMON;
/**
 * Data sent to the feedback power+ shift register
 */
const int FbPowerData = 7;
/**
 * Enables output on the data
 */
const int FbPowerLatch = 6;

////////////////////// TCO pullup latches SHIFT REGISTER ///////////////////////
const int TcInputClock = SHIFTREG_CLOCK_COMMON;
const int TcInputData = 2;

//const int TcInputClock = 13;
//const int TcInputData = 12;

const int TcInputLatch = A0;

////////////////////// RS485 communication pins //////////////////////
const int rs485Receive = 3;
const int rs485Send = 4;
const int rs485Direction = 5;

////////////////////// Feedback POWER- DEMUX, keyboard GND demux  ///////////////////////
const int DemuxAddr3 = 8;
const int DemuxAddr2 = 9;
const int DemuxAddr1 = 10;
const int DemuxAddr0 = 11;


const byte inputRows = 8;   // max 16
const byte inputColumns = 8;

static_assert(inputRows == 8 || inputRows == 16, "Either 8 or 16 rows must be selected");

const byte outputRows = 8;
const byte outputColumns = 8;

const byte maxRowCount = max(inputRows, outputRows);
const byte maxColumnCount = max(inputColumns, outputColumns);


const byte maxKeyTranslations = 32;
const byte outputByteSize = (outputRows * outputColumns + 7) / 8;

const byte maxTarget = 16;

const int majorVersion = 0;
const int minorVersion = 1;

