# StringLife – felhasználói kézikönyv

## Áttekintés

A StringLife egy JUCE 8 alapú VST3/AU/Standalone audio effekt már felvett
elektromos gitár- és basszussávok "húrfelújításához". Célja, hogy fáradt,
tompa húrú felvételeket frissebbnek, élénkebbnek hangzóvá tegyen - anélkül,
hogy húrzajt, reverbet vagy delay-t adna hozzá - négy egymásra épülő DSP-
szakasz kombinációjával: **Bite** (tranziens enhancer, visszaadja a pengetés
"pattanását"), **Shimmer** (harmonikus exciter, felső harmonikus csillogást
ad), **Spark** (presence EQ, magas frekvenciás emelés) és **Bloom** (upward
expander, a lecsengő sustaint húzza feljebb).

A négy szakaszt egyetlen **String Age** makró vezérli (Dead-Balanced-Fresh),
amely arányos eltolással köti össze a négy részparamétert - kézi
finomhangolás után is követik a makró mozgását, csak az eltolás marad meg.

A StringLife egy valódi, szerveralapú próbaidőszak/aktiváció modult
(Licensing) tartalmaz - lásd a "Licencelés" szakaszt.

## Gyors használat

1. Helyezd a StringLife plug-int a gitár- vagy basszussáv insert effektjei
   közé - lásd az "Elhelyezés az effektláncban" szakaszt.
2. Válaszd ki a hangszert a **GUITAR**/**BASS** választóval - ez állítja be
   mind a négy DSP-szakasz belső frekvenciafókuszát.
3. Indulj a **String Age** csúszka középállásáról (**50%, Balanced**).
4. A teljes mixben hallgatva húzd a csúszkát **Fresh** felé, amíg a húrok
   élénkebbnek/frissebbnek hatnak, de a hangzás még nem válik
   természetellenessé vagy zajossá.
5. Ha egy adott jelleg (pengetés, csillogás, sustain, magas prezencia) külön
   finomításra szorul, állítsd egyenként a **Spark**/**Bite**/**Shimmer**/
   **Bloom** gombokat - ezek a String Age-hez képesti arányos eltolással
   követik tovább a makrót, tehát a kézi finomhangolás nem vész el, ha
   utólag visszahúzod a String Age-t.
6. **DELTA** monitor módban ellenőrizd, mit ad ténylegesen hozzá a plug-in a
   jelhez, mielőtt visszaváltasz **NORMAL**-ra.
7. Az **A / B** gombbal két teljes, egymástól független beállítást
   hasonlíthatsz össze.
8. Ha kétséged van, **BYPASS**-szal ellenőrizd az eredeti jelet.

## A kezelőfelület

### Fejléc

- **MOTH PRODUCTION** / logó - márkajelzés, nem interaktív.
- **v1.6.2** - build-verziószám, nem interaktív.
- **A / B** - gyors összehasonlító kapcsoló *két teljes, egymástól
  független beállítás-pillanatkép* között, nem egy dry/bypass váltó: a
  gombra kattintva a plug-in elmenti az éppen elhagyott oldal minden
  "hangzási" paraméterét (String Age, Instrument, Spark, Bite, Shimmer,
  Bloom, Mix), majd visszahívja a másik oldal korábban mentett értékeit.
  Mindkét oldal azonosan indul, és attól kezdve válik el egymástól, hogy
  melyiken mit hangolsz. A gomb felirata mindig azt mutatja ("A" vagy "B"),
  melyik oldalt hallod éppen.
- **BYPASS** - valódi, host-automatizálható kikapcsolás: bekapcsolt
  állapotban a bemenet feldolgozás nélkül megy át, minden más vezérlő
  (Monitor, A/B, Mix) inaktívvá válik a felületen, mert semmi nincs, amit
  befolyásolnának. A host saját bypass-paramétereként is regisztrálva van.

### Cím sor

- **StringLife** / **RESTORE TIRED STRINGS** - terméknév és szlogen, nem
  interaktív.
- **GUITAR / BASS** (Instrument) - a négy DSP-szakasz frekvenciafókuszát
  állítja át egyszerre:

  | | Guitar | Bass |
  |---|---|---|
  | Shimmer (harmonikus exciter) felülvágó | 2 kHz | 800 Hz |
  | Spark (presence shelf) | 5 kHz, 0-tól +6 dB-ig | 2,5 kHz, 0-tól +5 dB-ig |

- **MONITOR: NORMAL / DELTA** - a kimenet forrása:
  - `Normal`: a feldolgozott (wet) és az eredeti (dry) jel keveréke, a
    **MIX** gomb aránya szerint.
  - `Delta`: kizárólag azt hallod, amit a plug-in ténylegesen hozzáad a
    jelhez (`(wet - dry) × 2`, azaz +6 dB kiegészítő erősítéssel, hogy a
    finom változások is jól hallhatók legyenek). Ilyenkor a MIX gomb
    hatástalan.

### String Age panel

- **STRING AGE** cím / **MACRO CONTROL** felirat - nem interaktív.
- Hullámzó "húr" háttérrajz - dekoráció, nem vezérlőelem.
- **REVIVAL** kijelző - a String Age aktuális értéke százalékban (pl. 64%).
- **DEAD - BALANCED - FRESH** csúszka (String Age, 0-100%) - a fő makró.
  Elmozdítása egyszerre, arányosan tolja el a Spark/Bite/Shimmer/Bloom
  értékét: minden részparaméter a String Age-hez viszonyított, utoljára
  kézzel beállított eltolását őrzi meg, nem egy rögzített arányt. Ha a
  makró egy paramétert a 0%/100% szélére tolna, az ott befagy, de amint a
  makró visszamozdul, a paraméter azonnal újra követni kezdi.

### Input/Output panel

- **INPUT / OUTPUT** cím - nem interaktív.
- **IN** / **OUT** függőleges csúcstartó szintmérők, -60 dB és 0 dB között,
  30 Hz-en frissülnek. A fehér vonal a csúcstartás: kb. 1 másodpercig tartja
  az utolsó csúcsot, utána 0,5 dB/frissítés (kb. 15 dB/s) ütemben süllyed,
  amíg egy új, magasabb csúcs nem érkezik.
- **MIX** forgatógomb (Wet/Dry Mix, 0-100%) - csak **NORMAL** monitor
  módban aktív; **DELTA** módban és **BYPASS** alatt inaktívra (halványabbra)
  vált, mert egyik esetben sem befolyásolja a kimenetet.

### Haladó sor (Spark / Bite / Shimmer / Bloom)

Mind a négy gomb saját, bekeretezett panelben, névvel és alatta egy
százalékos értékkijelzővel. A String Age makró mozgatja őket automatikusan,
de bármelyik egyenként is kézzel állítható (lásd fent).

- **SPARK** (Presence Amount) - magas-shelf ("presence") emelés. A shelf
  frekvenciája és a maximális erősítés a hangszerbeállítástól függ (lásd a
  fenti táblázatot); a szűrő jósági tényezője fix 0,707, tehát nincs
  rezonáns csúcs, csak sima emelés.
- **BITE** (Attack Restore) - tranziens enhancer: a pengetési/pluck
  tranzienseket erősíti fel, visszaadva a fáradt húrok által elveszített
  "pattanást". 0,5 ms attack / 20 ms release burkológörbe-követés; a
  felfutó tranzienseken alkalmazott erősítés-szorzó legfeljebb az `amount`
  nyolcszorosa.
- **SHIMMER** (Harmonic Excite) - harmonikus exciter: a jel egy felülvágott
  másolatát egy lágy, tanh-alapú szaturátoron vezeti át (2. és 3.
  felharmonikusokat generálva), majd visszakeveri a jelbe. A hajtás
  legfeljebb ×3-szoros teljes Shimmer-nél (kb. 0,3% THD-re kalibrálva), a
  visszakevert felharmonikus tartalom legfeljebb 30%-a a hajtott jelnek.
- **BLOOM** (Sustain Shape) - kímélő upward expander a lecsengő hangfarkon:
  a -30 dBFS küszöb alatti tartalmat a küszöb felé húzza, legfeljebb 1,2:1
  arányban - vagyis a halk, lecsengő rész nem hal el olyan gyorsan, mint egy
  fáradt húron. Ez a downward expander/gate ellentéte: nem csillapít,
  hanem felemel. 5 ms attack / 50 ms release.

Minden folyamatosan változtatható mennyiség simítva van (mintavétel-
függően, allokáció és zárolás nélkül az audio szálon), ezért a gyors
csúszka-mozgás és a host-automatizálás - beleértve a String Age makró teljes
Dead↔Fresh söprését is - nem okoz zippelést: a Bite/Shimmer/Bloom szakaszok
~20 ms-on, a Mix keverés ~30 ms-on, a Spark shelf-erősítése pedig ~50 ms-on
fut be.

## Elhelyezés az effektláncban

A StringLife elsősorban már rögzített, viszonylag tiszta gitár-/basszussávon
dolgozik a legjobban - a tranziens- és harmonikus-alapú feldolgozás a
lehető legtöbb valós információt egy még nem erősen torzított/tömörített
jelből tudja kinyerni. Jellemzően a lánc elején, erősítő-szimuláció vagy
erős torzítás/kompresszió előtt érdemes elhelyezni:

```text
Gitár/basszus felvétel → StringLife → Amp/cab szimuláció → EQ/kompresszió
```

## Licencelés

A StringLife egy valódi, szerveralapú próbaidőszak/aktiváció modult
tartalmaz (`Source/Licensing/`), ugyanaz a megosztott modul, mint a
PhaseLock és a többi Moth Production plug-in esetén.

- **Próbaidőszak**: minden új telepítés **15 napos**, teljes funkcionalitású
  ingyenes próbaidőszakkal indul.
- **Amíg a próbaidő tart vagy a licenc aktív**, a plug-in normálisan
  feldolgozza a hangot.
- **Ha a próbaidő lejár** (vagy aktiváció szükséges/érvénytelen a licenc),
  a kezelőfelület továbbra is megnyílik, de a hangfeldolgozás leáll: a
  kimenet néma marad, amíg nem aktiválod a terméket.
- **Offline türelmi idő**: ha korábban már aktiváltad a licencet, de a
  licencszerver átmenetileg nem elérhető, a plug-in az utolsó sikeres
  szerverellenőrzéstől számítva legfeljebb **14 napig** továbbra is
  működik, mintha aktiválva lenne.
- **Aktiválás**: nyisd meg a plug-int - próbaidőszak alatt vagy lejárat
  után egy felugró panel jelenik meg ("Unlock StringLife" cím, alatta a
  próbaidőből hátralévő napok száma vagy a pontos állapotüzenet). Az
  **ACTIVATE** gombra kattintva egy párbeszédablak kéri a licenckulcsot;
  megerősítés után a plug-in a szerverrel ellenőrzi és aktiválja azt.
- **TRIAL** gomb - bezárja a felugró panelt az adott szerkesztő-ablak
  munkamenetére, hogy tovább tudj dolgozni próbaidőszak alatt anélkül,
  hogy azonnal aktiválnod kellene. Ha újra elő szeretnéd hívni (pl. hogy
  aktiválj), zárd be, majd nyisd meg újra a plug-in szerkesztő ablakát - ez
  új szerkesztő-példányt hoz létre, amely a panelt ismét megmutatja, ha a
  licenc állapota ezt indokolja. Nincs külön, mindig látható "hátralévő
  napok" jelző a fejlécben ezen a paneleken kívül.
- A jelenlegi felület nem tartalmaz külön **Deactivate** gombot - a
  licencmodul programozási felülete támogatja a leaktiválást, de az editor
  ezt egyelőre nem köti kezelőelemre.

## Ismert korlátok

- A DSP-lánc négy szakasza (Bite, Shimmer, Spark, Bloom) egyike sem ad
  hozzá feldolgozási latency-t a kimenethez - nincs bennük FFT vagy
  lookahead-alapú elemzés, mindegyik minta-alapú burkológörbe-követést és
  IIR-szűrést használ.
- A tranziens-detektor (Bite) és az upward expander (Bloom) jelenleg
  lookahead nélküli, egyszerű burkológörbe-alapú detektálást használ (a
  Bloom pedig csúcs-, nem RMS-alapú szintkövetést) - ez a forráskódban is
  dokumentált, tudatosan vállalt fejlesztési állapot, nem hiba, de valós
  anyagon időnként kevésbé sima viselkedést eredményezhet, mint egy
  lookahead-es/RMS-alapú detektor.
- A négy DSP-szakasz jelenleg valós gitár-/basszusfelvételeken való
  hangolás alatt áll; az alapértékek és görbék a fejlesztés előrehaladtával
  még változhatnak.

## Telepítési helyek macOS-en

- Audio Unit: `~/Library/Audio/Plug-Ins/Components/StringLife.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/StringLife.vst3`
- Standalone app: `/Applications/Moth Production/StringLife.app`
  (telepítőcsomagból) vagy `build/Release/<profil>/Standalone/StringLife.app`
  (közvetlen buildből)

Ha a DAW nem látja az új buildet, indítsd újra a DAW-t, vagy futtasd újra a
plug-in ellenőrzését.
