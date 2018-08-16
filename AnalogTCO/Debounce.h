/**
 * Pokus o "lepsi debounce" na bitovych datech. Pro vstupy typu bitove pole, kde mohou jednotlive
 * vstupy kmitat. Spotreba pameti je n/2 + n/4 (n pocet vstupu). V pripade nutnosti by sla vymenit
 * rychlost a jednoduchost (citace v pulbajtech) za velikost pameti, ale pro mene nez 128 vstupu
 * to asi nestoji za to.
 * 
 * Princip: syrovy odecteny stav se uchovava v bitovem poli `rawState`. Bity, ktere zmenily stav
 * a jsou "v podmince" nez budou prijaty jako novy stav maji 1 na sve pozici v bitovem poli `changes`.
 * Jakmile bit zacne menit stav, ziska pocitadlo (0-15) na "svem" pulbajtu v poli `counterNibbles`. 
 * Pokud se hodnota bitu opet zmeni (zakmit), pocitadlo se resetuje na pocatek. Jakmile dopocita do nuly
 * (= pozadovany pocet cteni stejna hodnota), je hodnota prijata jako stabilni.
 * 
 * Zapsano jako trida, ponevadz debounceru je obcas treba vice.
 * 
 * Kmitajici vstupy se zapisuji metodou `debounce`. Stabilni vystup se hlasi virtualni metodou `stableChange`
 * - nutno pretizit. Doba, po ktere se vstup uzna za stabilni jde menit jednak pocatecni hodnotou pocitadla
 * (`setCounterOn`, `setCounterOff`) a jednak frekvenci volani `tick()`, ktere odtikne casovou jednotku. 
 * Neni nutne volat tick() po kazdem ctecim cyklu.
 * 
 */
class Debouncer {
  private:
  byte onCounter = 4, offCounter = 4;
  const byte  stateBytes;
  byte* const stableState;
  byte* changes;
  byte* rawState;
  byte* counterNibbles;

  /**
   * Aktualni pozice v nibble poli; pro zrychleni 
   */
  byte* curNibble;

  void reportChange(byte number, boolean nState);

  protected:
  virtual void reportByteChange(byte n8, byte state, byte mask);
  /**
   * Vola se pokud je zmena stabilni.
   * @param number cislo vstupu
   * @param nState novy stav
   */
  virtual void stableChange(byte number, boolean nState);

  public:
  /**
   * Inicializace Debounceru pro `inputCount` vstupu. Alokuje si
   * vlastni pole.
   */
  Debouncer(byte inputCount);

  /**
   * Initializace, stabilni stav propisuje do `stableState`; to musi byt 
   * dlouhe alespon inputCount / 8.
   */
  Debouncer(byte inputCount, byte *stableState);

  /**
   * Pocatecni hodnota pocitadla pro stav 'on'
   */
  void setOnCounter(byte x) { onCounter = x; }

  /**
   * Pocatecni hodnota pocitadla pro stav 'off'
   */
  void setOffCounter(byte x) { offCounter = x; }
  
  /**  
   * Vstup kmitajicich dat. Cislo prvniho zapisovaneho vstupu se predava
   * jako poradove cislo osmice (bajtu v bitovem poli). Zapisuji se cele
   * osmice vstupu.
   * 
   * @param inputStart8 poradove cislo zapisovane osmice vstupu.
   * @param raw bitova data on/off v delce rawByteSize
   * @param rawByteSize delka bitovych dat. Pocet zapsanych vstupu je rawByteSize * 8
   */
  void debounce(byte inputStart8, const byte* raw, byte rawByteSize);

  /**
   * Posune pocitadla, hlasi expirovane (stabilizovane) vstupy.
   */
  void tick();

  /**
   * Diagnostika
   */
  void print();
};

