# analog-tco-display

Projekt se skládá z 2 částí: Analogové TCO a Zobrazovač obsazení

## Analogové TCO
**TCO je ovladač ANALOGOVÉHO kolejiště do ovládacího pultu**, pracuje s tlačítky (spínači) nebo přepínači. Čte stav tlačítek, sleduje změny a pomocí sběrnice RS485 odesílá jednotlivým výkonným zařízením pod kolejištěm. Sběrnice nahrazuje samostatné spínací / povelové vodiče vedoucí z pultu k jednotlivým servům, přestavníkům apod. TCO si vystačí se 3 vodiči (U+, U- a GND) a dokáže po nich přenést velké libovolné množství povelů.

Bežné "analogově řízené" kolejiště využívá jednoduchého spínání, tedy na každé zařízení (přestavník, lampa, světlo návěstidla atd) padne jeden vodič. Jejich množství může lehce vyčerpat vodiče v kabelu propojující ovládací panel s kolejištěm.

**POZOR** jedná se o ovládání ANALOGOVÉHO kolejiště, u digitálního kolejiště pomocí _XPressNet_ je pro komunikaci s centrálou použitá sicestejná kabeláž ale **jiný, standardizovaný, protokol** ! Komunikace a možnosti tohoto "analogového TCO" v žádném případě nedosahuje robustnosti a šíře protokolu XpressNet nebo LocoNet.

TCO se **konfiguruje** pomocí integrovaného USB portu v Arduinu, zapojeného do PC. 

Více viz [Analogové TCO](http://cs.ttodbocna.wikia.com/wiki/Analog_TCO)

## Zobrazení obsazení
Skládá se z několika částí:
- Proudové detektory obsazení uzpůsobené pro analogový provoz
- Enkodér detektorů pro sběrnici S88 (stejná jako se používá u digitalních kolejišť)
- Zobrazovací jednotka v ovládacím pultu
Umožňuje přenášet až 128 detekovaných úseků a zobrazovat jejich stav na ovládacím pultu. Umožňuje také do přenášených dat zapojit jiný tzp detektorů, 
např. jazýčková relé, optické senzory atd. 

Podrobnější popis bude doplněn.

## Celková "architektura"
Jak spolu spolupracují TCO, Display, proudove detektory atd atd je [vysvětleno ve Wiki](http://cs.ttodbocna.wikia.com/wiki/Architektura_Analog)
