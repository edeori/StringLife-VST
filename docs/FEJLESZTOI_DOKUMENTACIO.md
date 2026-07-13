# StringLife – fejlesztői dokumentáció

## Projektállapot

- Keretrendszer: JUCE 8.0.4 (CMake `FetchContent`, `GIT_TAG 8.0.4`)
- Nyelv: C++17
- Buildrendszer: CMake 3.22+
- Formátumok: VST3, AU, Standalone
- Aktuális verzió: 1.6.2 (`CMakeLists.txt` `project(StringLife VERSION 1.6.2)`)
- Licencelés: valós, szerveralapú próba-/aktiváció modul (`Source/Licensing/`),
  `STRINGLIFE_ENABLE_LICENSING` CMake-opcióval (alapból `ON`) ki-/bekapcsolható,
  `licensed`/`dev` build-profil szerint
- DSP-fázis: prototípus - mind a négy DSP-osztály (`TransientEnhancer`,
  `HarmonicExciter`, `PresenceEQ`, `UpwardExpander`) lefordul és stabil
  interfészt ad, de az algoritmusok hangolása valós gitár-/basszusfelvételeken
  még folyamatban van (lásd a forráskód `TODO`-kommentjeit és a gyökér
  `README.md` "Status" szakaszát)

## Architektúra

```text
PluginProcessor / APVTS
        │
        ├── String Age makró (offset-alapú leképezés)
        │       └── Spark / Bite / Shimmer / Bloom
        │
        ├── A/B Compare (SoundSnapshot A és B, teljes paraméter-pillanatkép)
        │
        ├── TransientEnhancer   ("Bite" / attackRestore)
        ├── HarmonicExciter     ("Shimmer" / harmonicExcite)
        ├── PresenceEQ          ("Spark" / presenceAmount)
        ├── UpwardExpander      ("Bloom" / sustainShape)
        │
        ├── Monitor mix (Normal dry/wet crossfade | Delta wet-mínusz-dry)
        │
        └── licensing::LicenseManager (processBlock() elején gate-eli a hangot)
```

A `PluginProcessor` az APVTS-en keresztül olvassa a nyers paraméterértékeket,
majd a négy DSP-osztálynak `setAmount()`/`setBassMode()` hívásokkal adja
tovább őket - nincs közbülső, értéktípusú `Parameters` struktúra, mint pl. a
DeathWings widenerénél; minden DSP-osztály közvetlenül a saját belső
`juce::SmoothedValue`-ján keresztül simítja a kapott `amount`-ot.

## Jelút

```text
Input L/R
  → TransientEnhancer (Bite)
  → HarmonicExciter (Shimmer)
  → PresenceEQ (Spark)
  → UpwardExpander (Bloom)
  → Monitor mix (Normal: dry/wet crossfade | Delta: (wet - dry) × 2)
  → Output
```

A pontos sorrendet a `PluginProcessor::processBlock()` határozza meg:

```cpp
transientEnhancer.process (block);
harmonicExciter.process (block);
presenceEQ.process (block);
upwardExpander.process (block);
```

A `dryBuffer` a négy DSP-szakasz *előtt* másolódik le a bemenetről, ezért a
Monitor mix (Normal/Delta) mindig a teljes, mind a négy szakaszon átment
jelet állítja szembe az eredeti, teljesen érintetlen bemenettel - nem az
egyes szakaszok közötti köztes állapotokat.

**Bypass**: a `bypass` APVTS-paraméter true bypass - `processBlock()` a
dry buffer másolása és a négy DSP-hívás előtt tér vissza, ha
`bypassParam->load() > 0.5f`, tehát ilyenkor egyik szakasz sem fut, és a
buffer mintapontosan érintetlen marad. A processor felülírja
`getBypassParameter()`-t, hogy a host saját bypass-sávjaként ismerje fel
ugyanezt a paramétert.

## DSP-szakaszok

### TransientEnhancer (Bite / `attackRestore`)

Burkológörbe-követő alapú tranziens enhancer: gyors/lassú burkológörbét
követ, és a felfutó tranzienseket (pengetés/pluck attack) erősíti fel, hogy
pótolja a fáradt húrok által elveszített "pattanást". Az `amount` egy
~20 ms-os `juce::SmoothedValue`-n keresztül jut a DSP-be. A fejléc TODO-ja
szerint jelenleg egy naiv burkológörbe-derivált detektor van használatban,
lookahead nélkül - ez tudatosan dokumentált, nem javított állapot ebben a
fázisban.

### HarmonicExciter (Shimmer / `harmonicExcite`)

Párhuzamos harmonikus exciter: a jel egy felülvágott (`setBassMode()`
szerint hangszerfüggő vágási pontú) másolatát egy tanh-alapú lágy
szaturátoron vezeti át, 2. és 3. felharmonikusokat generálva, majd
visszakeveri a száraz jelbe. A gyökér `README.md` szerint a drive teljes
`amount`-nál legfeljebb ×3, kb. 0,3% THD-re kalibrálva, a visszakevert
tartalom legfeljebb a hajtott jel 30%-a. A fejléc TODO-ja szerint a drive-
görbe és a harmonikus-egyensúly hangolása valós felvételeken még hátra van;
egy jövőbeli finomítás felmerülhet a páros/páratlan harmonikus-tartalom
szétválasztásáról is.

### PresenceEQ (Spark / `presenceAmount`)

High-shelf "presence" emelés `juce::dsp::IIR` szűrővel
(`ProcessorDuplicator`). A shelf frekvenciája és a maximális erősítés
`setBassMode()` szerint hangszerfüggő (lásd lentebb), a jósági tényező fix
0,707 (nincs rezonáns csúcs). Az erősítés egy ~50 ms-os `SmoothedValue`-n
keresztül fut; az együtthatók blokkonként, allokáció nélkül frissülnek,
amíg az érték még mozgásban van (`coefficientsDirty` flag).

### UpwardExpander (Bloom / `sustainShape`)

Kímélő upward expander a lecsengő hangfarkon: a fix -30 dBFS küszöb alatti
jelet a küszöb felé húzza, legfeljebb 1,2:1 arányban - ez a downward
expander/gate ellentéte, nem csillapít, hanem felemeli a halk, lecsengő
tartalmat. 5 ms attack / 50 ms release burkológörbe. A fejléc TODO-ja
szerint jelenleg egyszerű csúcs- (peak-) burkológörbét használ RMS-detektor
helyett.

### Instrument-függő frekvenciafókusz (`setBassMode()`)

| | Guitar | Bass |
|---|---|---|
| HarmonicExciter felülvágás | 2 kHz | 800 Hz |
| PresenceEQ shelf | 5 kHz, 0-tól +6 dB-ig | 2,5 kHz, 0-tól +5 dB-ig |

Az `instrument` APVTS-paraméter (`AudioParameterChoice`, "Guitar"/"Bass")
`processBlock()` minden blokkjában újra kiértékelődik, és mindkét érintett
DSP-osztályra (`presenceEQ.setBassMode()`, `harmonicExciter.setBassMode()`)
alkalmazásra kerül.

## String Age makró és a paraméter-eltolás mechanizmus

A `stringAge` (0-1) egyetlen makró, amely a `presenceAmount`,
`attackRestore`, `harmonicExcite` és `sustainShape` négy paramétert vezérli
- de nem egyszeri érték-másolással, hanem **állandó, előjeles eltolással**
(`macroOffset[param] = paramValue - stringAgeValue`, a paraméter utolsó
kézi állításának pillanatában rögzítve). A String Age mozgatásakor minden
paraméter ugyanazzal az eltolással mozdul el, megőrizve a kézzel beállított
különbségeket - hasonlóan egy szintetizátor relatív/offset makró-
leképezéséhez. A cél `stringAgeValue + macroOffset[param]` [0,1]-re van
korlátozva, de maga az eltolás nincs - így ha a makró egy paramétert a
szélére szorítja, az később, amint a makró visszamozdul, azonnal újra
követni kezdi ugyanazzal az eltolással.

```cpp
auto target = juce::jlimit (0.0f, 1.0f, stringAgeValue + macroOffset[idx].load());
apvts.getParameter (paramID)->setValueNotifyingHost (target);
```

A `parameterChanged()` (APVTS-listener) különbözteti meg a makró-eredetű
push-t (`updatingFromMacro` flag) a valódi felhasználói/automatizált
szerkesztéstől: utóbbi esetben újraszámolja és eltárolja az adott paraméter
`macroOffset`-jét a String Age aktuális értékéhez képest.

**Ismert, dokumentált korlát** (forráskódi TODO): `parameterChanged()`
audio szálon is meghívódhat host-automatizálás alatt, és jelenleg
szinkron módon hívja a `setValueNotifyingHost()`-ot a makró-push során -
a teljes valós idejű biztonsághoz ezt egy `AsyncUpdater`-en keresztül
halasztani kellene, ez még nem történt meg.

## A/B Compare

Az **A/B** gomb (`abCompare` bool paraméter) nem dry/bypass váltó, hanem két
teljes, egymástól független `SoundSnapshot` (String Age, Instrument, Spark,
Bite, Shimmer, Bloom, Mix) közötti váltás. A `parameterChanged()` az
`abCompare` váltásakor előbb elmenti az éppen elhagyott oldal aktuális
állapotát (`captureCurrentSnapshot()`), majd betölti a másik oldal korábban
mentett értékeit (`applySnapshot()`). Az `applySnapshot()` először a
`stringAge`-t állítja be (ez a makró-push-t is elindítja a pillanatnyi
eltolásokkal), majd a négy haladó paramétert felülírja a pillanatkép saját
értékeivel - ez egyúttal újra levezeti a helyes `macroOffset`-eket is a
fenti `parameterChanged()` ágon keresztül, tehát a pillanatkép arányos
"érzete" is visszaáll.

Mindkét oldal a paraméterek alapértékével azonosan indul (a konstruktorban),
és attól kezdve válik szét, hogy melyiken mit hangol a felhasználó.

A pillanatképek és a `macroOffset` értékek nem APVTS-paraméterek (nincs
host-automatizáció mögöttük), ezért a `getStateInformation()`/
`setStateInformation()` kézzel, külön XML-attribútumokként menti/tölti be
őket (`presenceOffset`, `attackOffset`, `harmonicOffset`, `sustainOffset`,
`snapA*`/`snapB*`) az APVTS állapota mellé - enélkül egy preset
visszatöltése némán elvesztené a String Age-hez képesti kézi eltolásokat és
a másik A/B oldal tartalmát.

## APVTS-paraméterek

| ID | Típus | Tartomány / opciók | Alapérték | UI-név |
|---|---|---|---|---|
| `stringAge` | float | 0,0-1,0 | 0,5 | String Age (makró, "meta" jelölésű) |
| `instrument` | choice | Guitar / Bass | Guitar | Guitar/Bass választó |
| `presenceAmount` | float | 0,0-1,0 | 0,5 | Spark |
| `attackRestore` | float | 0,0-1,0 | 0,5 | Bite |
| `harmonicExcite` | float | 0,0-1,0 | 0,5 | Shimmer |
| `sustainShape` | float | 0,0-1,0 | 0,5 | Bloom |
| `wetDryMix` | float | 0-100 (%) | 100 | Mix |
| `monitorMode` | choice | Normal / Delta | Normal | Monitor Normal/Delta |
| `abCompare` | bool | - | false | A/B ("meta" jelölésű) |
| `bypass` | bool | - | false | Bypass; `getBypassParameter()` erre mutat |

A "meta" jelöléssel (`withMeta (true)`) ellátott paraméterek (`stringAge`,
`abCompare`) a host felé is jelzik, hogy nem egyszerű hangzásparaméterek,
hanem UI-szintű vezérlők/makrók.

A `presenceAmount`/`attackRestore`/`harmonicExcite`/`sustainShape`
paraméterek megjelenített neve (Spark/Bite/Shimmer/Bloom) eltér a belső
azonosítójuktól és a `SoundSnapshot`/`macroOffset` mezőnevektől
(presence/attack/harmonic/sustain) - ez szándékos: csak a felhasználó felé
mutatott elnevezés változott a reskin során, az állapotmentés és a makró-
logika kulcsai nem.

## Forrásfájlok

- `Source/PluginProcessor.*` - lifecycle, APVTS, String Age makró-eltolás,
  A/B pillanatkép-kezelés, jelút, licenc-gate, state mentés/betöltés.
- `Source/PluginEditor.*` - saját kezelőfelület; a négy haladó csúszka
  sima `SliderAttachment`-tel kapcsolódik (a makró-push a processoron
  keresztül automatikusan frissíti az értéküket).
- `Source/StringLifeLookAndFeel.*` - egyedi sötét/fémes + cián-glow
  megjelenés (rotary knobok, lineáris String Age csúszka, panel/gomb
  keretezés), a mockup CSS-éből (`Mockup/stringlife-ui.html`) átvéve.
- `Source/StringLifeFonts.*` - beágyazott Oxanium/Barlow Condensed/Rajdhani
  betűtípusok kezelése (StringLife a termékcsalád többi tagjától eltérően
  `Oxanium-Variable.ttf`-et használ `Oxanium-Bold.ttf` helyett - ez egy
  elfogadott, egyedi eltérés, nincs hatással a funkcióra).
- `Source/TransientEnhancer.*`, `HarmonicExciter.*`, `PresenceEQ.*`,
  `UpwardExpander.*` - a négy DSP-szakasz.
- `Source/Licensing/` - a megosztott, alkalmazás-független licencmodul
  (`LicenseConfig`, `LicenseClient`, `LicenseManager`, `LicenseStore`,
  `TrialManager`, `MachineFingerprint`) - lásd a "Licencelés" szakaszt.
- `Assets/licensing_config.json` - StringLife saját, beépített
  konfigurációja (lásd lentebb), buildkor bináris adatként ágyazódik be.
- `Mockup/stringlife-ui.html` / `stringlife-ui.png` - a UI-reskin forrás-
  mockupja; a `resized()` minden design-térbeli pixelértéke innen, nem
  méréssel/becsléssel származik.

## Licencelés

A `Source/Licensing/` modul a StringLife és a PhaseLock között megosztott,
alkalmazás-független csomag - a modul semmilyen fájlja nem hivatkozik a
StringLife nevére, minden termékazonosságot a beágyazott konfigurációból
kap.

**Viselkedés** (`LicenseManager` + `TrialManager`):

- Új telepítés 15 napos (`trialLengthDays: 15`), teljes funkcionalitású
  ingyenes próbaidőszakkal indul.
- Amíg a próbaidő aktív vagy a licenc érvényesen aktiválva van,
  `isAudioEnabled()` igazat ad vissza.
- Ha a próbaidő lejár, vagy aktiváció szükséges/érvénytelen, a
  kezelőfelület továbbra is megnyílik, de a `processBlock()` a hang helyett
  némít - lásd a gate-et a "Jelút"/"Bypass" szakaszban.
- Érvényes aktiválás, vagy egy aktív offline türelmi idő (az utolsó sikeres
  szerverellenőrzéstől számítva legfeljebb `offlineGracePeriodDays` = 14
  napig) újra engedélyezi a hangot.
- A licenckulcs megadása és az Activate a plug-in felületén történik (lásd
  lentebb); a modul programozási felülete leaktiválást (`deactivate()`) is
  támogat, de az `StringLifeAudioProcessorEditor` jelenleg nem köti be egy
  Deactivate gombra - csak `activateButton` és `trialButton` létezik.

**Config fájl** (`Assets/licensing_config.json`, StringLife tényleges,
committolt értékei):

```json
{
  "companyName": "Moth Production",
  "applicationName": "StringLife",
  "productId": "stringlife",
  "productVersion": "1.6.2",
  "serverBaseUrl": "https://licensing.mothproduction.hu",
  "trialLengthDays": 15,
  "offlineGracePeriodDays": 14,
  "requestTimeoutMs": 5000,
  "allowUnsignedDevelopmentLicenses": false,
  "endpoints": {
    "activate": "/activate",
    "validate": "/validate",
    "refresh": "/refresh",
    "deactivate": "/deactivate"
  }
}
```

(A `publicKey` mező valós, hosszú RSA-modulust tartalmaz, itt kihagyva.)

**Szerveroldali követelmény**: a szervernek olyan aláírt licenc-payloadot
kell kiadnia, amelynek `productId` mezője pontosan `"stringlife"` -
a `LicenseManager` `Tampered` állapotúnak minősít (és elutasít) egy olyan
aláírt licencet, amelynek payloadja nem a futó termék konfigurációjához
tartozik, még akkor is, ha maga az aláírás egyébként érvényes - ez
akadályozza meg, hogy pl. egy PhaseLock-licenc némán feloldja a StringLife-
ot vagy fordítva.

**Build-időbeli felülírás** (a committolt JSON szerkesztése nélkül, pl.
helyi teszt-szerverhez):

```bash
LICENSE_SERVER_BASE_URL=http://127.0.0.1:8787 \
LICENSE_PRODUCT_ID=stringlife-dev \
./build.sh licensed
```

Támogatott CMake cache-változók: `STRINGLIFE_LICENSE_SERVER_BASE_URL`,
`STRINGLIFE_LICENSE_PRODUCT_ID`, `STRINGLIFE_LICENSE_PUBLIC_KEY`.
`-DSTRINGLIFE_ENABLE_LICENSING=OFF` mellett a modul teljes egészében
kimarad a buildből - a hang ekkor mindig fut, nincs felugró panel (ez a
`dev` build-profil, lásd lentebb).

**UI-integráció** (`PluginEditor.h/.cpp`):

- `LicenseOverlayComponent` - eltüntethető felugró panel (`activateButton`,
  `trialButton`, cím- és állapotszöveg), amely mindaddig látszik, amíg a
  licenc állapota nem `Activated` vagy `OfflineGracePeriod`, és a
  felhasználó nem zárta be a **TRIAL** gombbal az adott editor-példányban
  (`licenseOverlayDismissed`).
- Az **ACTIVATE** gomb egy `juce::AlertWindow`-t nyit licenckulcs
  bekéréséhez, majd `LicenseManager::startActivation()`-t hív.
- Nincs mindig látható, kis "TRIAL: Nd" jellegű fejléc-link (ellentétben a
  PhaseLock integrációjával) - a felugró panel bezárása után az újbóli
  megjelenítéshez az editor-példányt újra kell létrehozni (pl. a plug-in
  ablak be- majd újranyitásával), mert `licenseOverlayDismissed` alapból
  `false`-ra inicializálódik minden új szerkesztőpéldányban.

**Szálkezelés**: `LicenseManager::initialise()` a konstruktorban, a fő/
üzenetszálon fut (egyszeri lemez-I/O-t végezhet a mentett állapot
beolvasásához, és elindíthat egy háttér-frissítést, ha már aktiválva van).
Minden aktiválás/leaktiválás/frissítés hívás a manager saját, egyszálú
`juce::ThreadPool`-ján fut, sosem az audio szálon. Az `isAudioEnabled()`,
`getStatus()` és `getDaysRemainingInTrial()` egyszerű atomi olvasások,
biztonságosan hívhatók `processBlock()`-ból.

## Build és telepítés

```bash
./build.sh licensed
./build.sh dev
```

Ez a `licensed`/`dev` profil szerint konfigurálja a CMake buildet
(`STRINGLIFE_ENABLE_LICENSING` ON/OFF), lefordítja a VST3/AU/Standalone
célokat, majd lelapított formában másolja az eredményt:

- `build/Release/<profil>/VST3/StringLife.vst3`
- `build/Release/<profil>/AU/StringLife.component`
- `build/Release/<profil>/Standalone/StringLife.app`

A `COPY_PLUGIN_AFTER_BUILD TRUE` miatt macOS-en a VST3 és az AU emellett
automatikusan a felhasználói plug-in könyvtárakba is települ
(`~/Library/Audio/Plug-Ins/...`).

**macOS telepítőcsomag** (`.pkg`, "next → next → finish"):

```bash
./package.sh licensed
./package.sh dev
```

Kimenet: `build/Release/licensed/StringLife.pkg`, illetve
`build/Release/dev/StringLife-dev.pkg`. A csomag rendszerszinten települ:
`/Library/Audio/Plug-Ins/VST3/StringLife.vst3`,
`/Library/Audio/Plug-Ins/Components/StringLife.component`,
`/Applications/Moth Production/StringLife.app`. A `.pkg` jelenleg
aláíratlan; nyilvános kiadás előtt az app/plugin bundle-öket és magát a
csomagot is alá kell írni és notarizálni kell.

**Windows telepítőcsomag**: Inno Setup alapú folyamat
(`packaging/windows/StringLife.iss.in`, `package-windows.ps1`). macOS-ről
`pwsh` (`brew install powershell`) segítségével az `.iss` script
generálható/ellenőrizhető, de a tényleges fordítás/aláírás Windows +
Inno Setup jelenlétét igényli.

## Tesztek

A repóban jelenleg nincs automatizált teszt- vagy mérőeszköz-könyvtár
(`Tests/`, `Tools/`) a DSP-szakaszokhoz - az egyetlen ellenőrzés a build
sikeressége és a manuális audiálás.
