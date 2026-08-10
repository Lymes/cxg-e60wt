# Firmware per saldatore CXG-E60WT (STM8S103K3)

![Saldatore CXG-E60WT](/images/screen1.jpeg)

Firmware per il saldatore CXG-E60WT con MCU STM8S103K3. Dispone di modalità sleep/wake-up, cicalino, rilevamento errori, calibrazione ADC per punta, compensazione della tensione di rete (110V/220V), protezione da sovratemperatura e un **controllore proporzionale-derivativo (PD) con riduzione di potenza** per una regolazione della temperatura precisa e senza sovraoscillazione.
Con questo firmware e le piccole aggiunte hardware (interruttore a inclinazione + cicalino), questo economico saldatore supera di gran lunga le aspettative — offrendo stabilità di temperatura e funzionalità di sicurezza paragonabili a stazioni di saldatura professionali che costano molte volte di più.

> [!WARNING]
> **Questo firmware utilizza di default l'elemento riscaldante A1326 (220V).**
> Se il tuo saldatore è dotato di un riscaldatore **A1316 (110V)**, **DEVI** accedere al menu di configurazione e impostare **`HT = 1`** prima del primo utilizzo. L'utilizzo del firmware con l'impostazione errata del tipo di riscaldatore su una rete da 220V fornirà circa **4× la potenza nominale** all'elemento riscaldante e lo **distruggerà in pochi secondi**.
>
> Per accedere al menu di configurazione: tieni premuto il tasto **`+`** durante l'accensione. Naviga alla voce **`HT`** (ultima voce). Impostalo su **`0`** per A1326 (220V) oppure **`1`** per A1316 (110V). Il valore viene salvato automaticamente nell'EEPROM.


## Selezione dell'elemento riscaldante (A1316 vs A1326)

Molti modelli CXG-E60WT vengono forniti di fabbrica con un riscaldatore **A1316 (110V, ~55Ω a freddo)** — progettato per reti nordamericane e giapponesi da 100–120V. Questo firmware supporta entrambi gli elementi tramite l'impostazione `HT` del menu, ma **su reti europee da 220–240V l'A1326 (220V) è la scelta corretta e consigliata**.

### Perché A1326 è migliore per reti da 230V / europee

Con un'alimentazione a 240V il bus DC raddrizzato è ~339V. Il firmware limita la potenza massima del riscaldatore limitando il ciclo di lavoro PWM (`minPwm`). L'intervallo di lavoro effettivo determina quanto margine ha il controllore PD per accelerare il riscaldamento e frenare prima della sovraoscillazione:

| | A1326 (220V, `HT=0`) | A1316 (110V, `HT=1`) |
|---|---|---|
| Resistenza a freddo | ~200 Ω | ~55 Ω |
| Resistenza a caldo (400 °C) | ~800 Ω | ~200 Ω |
| Ciclo di lavoro max a 240V | **47 %** | 11 % |
| Potenza max con punta calda | ~60 W ✓ | ~63 W ✓ |
| Intervallo di controllo PD | **±47 %** | ±11 % |
| Risoluzione PWM a regime | **0,28 W/step** | 0,13 W/step |
| Rischio di errata configurazione | Basso | **Molto alto** (riscaldatore distrutto istantaneamente con `HT=0`) |
| Impostazione predefinita firmware | ✅ Sì | No — richiede `HT=1` prima del primo utilizzo |

Entrambi gli elementi forniscono ~60 W alla punta alla temperatura di esercizio. La differenza è nella **controllabilità**: con A1316 il controllore PD dispone solo dell'11% del ciclo di lavoro. Se la temperatura della punta sale più velocemente dell'intervallo di campionamento di 200 ms, il controllore reagisce tardi e ha poco margine per correggere. Con A1326 il 47% di range dà al controllore ampio margine sia per accelerare il riscaldamento sia per frenare correttamente prima di superare il setpoint.

### Perché i produttori spediscono A1316 in tutto il mondo

L'A1316 è una deliberata decisione di ottimizzazione dei costi: un unico SKU copre ogni mercato. Sulle reti a 110V (USA, Giappone) funziona perfettamente — il firmware gira al ~48% del ciclo di lavoro e il controllore PD ha ampio margine. A 220–240V il saldatore si scalda e salda comunque, quindi la maggior parte dei clienti non nota nulla di sbagliato. Un inventario globale unificato è semplicemente più economico che stoccare due varianti.

Il firmware di fabbrica originale peggiora le cose: quasi certamente non implementa una corretta compensazione della tensione. Su 220V con A1316 e senza compensazione il riscaldatore gira a un ciclo di lavoro fisso elevato → sovralimentazione permanente → degrado accelerato dell'elemento ceramico.

Questo firmware personalizzato risolve il problema: limita correttamente la potenza per entrambi i tipi di riscaldatore su qualsiasi tensione di rete. Ma la fisica non può essere cambiata — A1316 su 230V lascerà sempre al controllore PD solo l'11% del ciclo di lavoro.

### Comportamento su rete a 110V (USA/Giappone)

Per completezza, ecco come si comporta ciascun riscaldatore quando lo stesso saldatore viene collegato a una rete da 110V ($V_{DC} \approx 155\text{V}$):

| | A1326 (220V, `HT=0`) | A1316 (110V, `HT=1`) |
|---|---|---|
| Ciclo di lavoro max a 110V | 100 % | 48 % |
| Potenza max con punta calda | ~30 W ❌ | ~57 W ✅ |
| Verdetto | Sottopotenziato — riscaldamento lento, perde temperatura sotto carico | Funziona correttamente |

I due riscaldatori sono quindi complementari: A1326 è ottimale per 230V, A1316 per 110V. Se si viaggia tra regioni diverse, sostituire l'elemento riscaldante (stesso formato fisico, facilmente reperibile) e cambiare `HT` nel menu garantisce le prestazioni complete ovunque.

**Raccomandazione:** se il tuo saldatore è dotato di A1316, sostituiscilo con un A1326 (facilmente reperibile, stesso formato fisico). Mantieni `HT=0` (impostazione predefinita) e goditi una migliore stabilità della temperatura e una configurazione più sicura.

Se non riesci a trovare un A1326 e devi usare un A1316 su 230V, imposta `HT=1` nel menu di configurazione — il firmware limiterà correttamente la potenza. Ma leggi prima attentamente l'avviso sopra.

## Compilazione del firmware

Sono necessari due strumenti:

- **stm8flash** — utilità di flashing ST-Link: [vdudouyt/stm8flash](https://github.com/vdudouyt/stm8flash)
- **SDCC** — Small Device C Compiler: [sdcc.sourceforge.net](https://sdcc.sourceforge.net/)

## Su macOS (Apple Silicon / M1):

### Installazione di SDCC
```
brew install sdcc
```

### Installazione di stm8flash
```
git clone https://github.com/vdudouyt/stm8flash.git
cd stm8flash
make
cp stm8flash /opt/homebrew/bin/
```

### Compilazione del firmware
```
git clone https://github.com/Lymes/cxg-e60wt.git
cd cxg-e60wt
make
make flash
```

## Su Linux:

### Installazione di SDCC
```
sudo add-apt-repository ppa:laczik/ppa
sudo apt-get update
sudo apt-get install sdcc
```

### Installazione di stm8flash
```
git clone https://github.com/vdudouyt/stm8flash.git
cd stm8flash
make
sudo make install
```

### Compilazione del firmware
```
git clone https://github.com/Lymes/cxg-e60wt.git
cd cxg-e60wt
make
make flash
```

## Build di debug (UART printf su PD5)

Una build di debug attiva un'uscita di traccia seriale su **PD5 (UART1 TX, 115200 8N1, 5V)** senza alcun impatto sul firmware di rilascio — tutto il codice di debug viene rimosso dal preprocessore quando `DEBUG` non è definito.

### Hardware

> Prima di aprire il manico, rimuovi prima i cappucci in gomma dei pulsanti.

![Lato display PCB](/images/screen2.jpeg)

![Lato componenti PCB — CON1, interruttore a mercurio, cicalino](/images/screen3.jpeg)

Collega un adattatore USB-UART (RX **compatibile 5V**) a CON1. Qualsiasi adattatore va bene — il CH340N SOP-8 mostrato qui era semplicemente quello che si trovava a portata di mano:

```
Saldatore             Adattatore USB-UART
──────────────────────────────────────
CON1 GND   ────────── GND
MCU PD5    ────────── RXD
```

<img src="/images/screen5.jpeg" width="260" align="right">

> **Nota:** l'adattatore è alimentato da USB — **non** collegare VDD+ da CON1. Sono necessari solo GND e RXD.

`CON1` è l'header di programmazione ST-Link già presente sulla scheda. `MCU PD5` è il pin UART1 TX (livello 5V).

Per l'ST-Link V2, usa la **fila sinistra** di pin per STM8 (SWIM + RST + GND + VDC 5V):

![ST-Link V2 — usa la fila sinistra per STM8](/images/screen4.jpeg)

### Compilazione e flashing

```bash
# Esegui sempre 'make clean' quando si passa tra release e debug
make clean && make debug       # solo compilazione
make clean && make flash-debug # compilazione + flash via ST-Link
```

### Output seriale

Monitora con `minicom -o -D /dev/cu.usbserial-* -b 115200` (o qualsiasi terminale seriale a 115200 8N1).

| Riga | Quando | Campi |
|---|---|---|
| `B hp=… c=… al=… ah=… sl=… ds=…` | Avvio | heatPoint, calibrazione, adcMin, adcMax, timeout sleep1/2 |
| `T=… S=… e=… a=… v=… p=…` | Ogni 200 ms | temp°C, setpoint, errore, ADC_sensor_raw, ADC_vin_raw, ciclo PWM (100=off, minPwm=piena pot.) |
| `Sp<n>` | Pressione pulsante | nuovo setpoint |
| `E!<n> a=…` | Errore sensore confermato | riscaldatore OFF, ADC grezzo mostrato |
| `OV<temp>` | Sovratemperatura | guasto fisso latched |
| `Rw T=…` | Runaway termico | transistor Q1 bloccato |

### Dimensioni della build

| Build | Flash usata | Libera (di 8192 B) |
|---|---|---|
| Release | 7094 B | 1098 B |
| Debug | 7674 B | 518 B |

## Menu di servizio
Puoi accedere al Menu di servizio tenendo premuto il tasto "+" durante l'accensione.

Un doppio clic su qualsiasi tasto cambierà ciclicamente le seguenti voci del menu:
* **SOU**: abilita/disabilita il suono, valori 0..1 (predefinito 1)
* **CAL**: offset di calibrazione in gradi, intervallo -99..99 (predefinito 0)
* **SL1**: timeout sleep in minuti, intervallo 1..30 (predefinito 3) — mantiene 100°C
* **SL2**: timeout deep sleep in minuti, intervallo 1..60 (predefinito 10) — riscaldatore OFF
* **FRC**: incremento temperatura modalità forzata in gradi, intervallo 0..100 (predefinito 0)
* **ADL**: calibrazione ADC punto freddo (sensore a temperatura ambiente) — per punta
* **ADH**: calibrazione ADC punto caldo (sensore a temperatura massima) — per punta
* **HT**: tipo riscaldatore — `0` = A1326 (220V, predefinito), `1` = A1316 (110V). **Deve essere impostato correttamente prima del primo utilizzo** — vedi [Selezione dell'elemento riscaldante](#selezione-dellelemento-riscaldante-a1316-vs-a1326).

Per uscire dal Menu di servizio basta spegnere/accendere il saldatore.

**NOTE:**
* In modalità SL1 il saldatore mantiene 100°C
* In modalità SL2 il riscaldatore è completamente OFF e il display si spegne
* Per ripristinare tutti i valori ai DEFAULT premere il tasto "-" durante l'accensione
* Premere "+" e "-" simultaneamente attiva/disattiva la modalità FORCED (il display mostra il simbolo °F)

## Codici di errore

Il firmware visualizza i seguenti codici di errore sul display a 7 segmenti. Tutti gli errori spengono immediatamente il riscaldatore.

| Codice | Display | Causa | Rimedio |
|---|---|---|---|
| **ER1** | `Er1` | **Cortocircuito del sensore** — la lettura ADC è ben al di sotto del punto freddo calibrato (`adcVal < adcMinRT / 2`). Tipicamente causato da una termocoppia/NTC danneggiata, un ponte di saldatura sul circuito del sensore, o una punta con scarso contatto termico. Con rimbalzo: richiede 500 letture consecutive errate (~500 ms) per scattare. | Reinserire o sostituire la punta; verificare il cablaggio del sensore. Ciclo di alimentazione per riprendere. |
| **ER2** | `Er2` | **Circuito aperto del sensore / sensore rotto** — la lettura ADC supera la soglia hardware di circuito aperto (`adcVal > 1000`). Tipicamente un filo della termocoppia rotto, punta mancante o elemento sensore guasto. Stesso rimbalzo di 500 letture di ER1. | Reinserire o sostituire la punta; verificare il cablaggio del sensore. Ciclo di alimentazione per riprendere. |
| **OVH** | `OuH` | **Guasto per sovratemperatura (latched)** — attivato da una delle due condizioni indipendenti: **(1) Limite rigido** — la temperatura misurata ha superato 480 °C; **(2) Runaway termico** — il riscaldatore è stato spento ma la temperatura ha continuato a salire per 8 secondi consecutivi, indicando che il transistor Q1 (IRF840) è bloccato in conduzione. Il guasto rimane latched fino al ciclo di alimentazione. | Lasciar raffreddare il saldatore. Investigare la causa principale (Q1 bloccato, sensore difettoso, calibrazione ADC errata). Ciclo di alimentazione per azzerare. |

## Calibrazione ADC per punta (ADL / ADH)

Punte diverse hanno un contatto termico diverso con il riscaldatore ceramico, con conseguente spostamento della curva di risposta ADC. Usa **ADL** e **ADH** per calibrare ogni punta:

1. Accedi al Menu di servizio (premi "+" all'accensione)
2. Naviga a **ADL** — con il saldatore a temperatura ambiente, regola finché la temperatura visualizzata corrisponde alla temperatura ambiente (~25°C)
3. Naviga a **ADH** — porta il saldatore alla temperatura massima, regola finché il display corrisponde a un riferimento noto (es. misurato con un termometro esterno a ~450°C)
4. Affina con **CAL** per una piccola correzione dell'offset se necessario

## Controllo della temperatura (PD + riduzione di potenza)

Il firmware utilizza un **controllore proporzionale-derivativo** con **riduzione di potenza** invece di un semplice bang-bang o rampa lineare:

- **Termine P**: riduce proporzionalmente la potenza del riscaldatore man mano che la temperatura si avvicina al setpoint
- **Termine D**: rileva la velocità di variazione della temperatura e frena precocemente — si adatta automaticamente all'inerzia termica di diverse dimensioni di punta
- **Riduzione di potenza**: limita la potenza massima del riscaldatore a `(diff + 15)%` dell'intervallo disponibile. A 50°C di distanza → max 65%; a 10°C di distanza → max 25%; previene l'accumulo di energia nell'elemento riscaldante che causa sovraoscillazione
- **Cutoff rigido**: il riscaldatore viene spento immediatamente quando la temperatura raggiunge o supera il setpoint

**Risultati misurati (con punta installata):**

| Setpoint | Sovraoscillazione | Precisione a regime |
|---|---|---|
| 120°C | +2°C | ±3°C |
| 250°C | 0°C | ±4°C |

La banda di ±3-4°C a regime è il limite di risoluzione dell'ADC a 10 bit con il sensore NTC.

## Protezione da sovratemperatura

Il firmware include due livelli indipendenti di protezione hardware:
* **Limite rigido (480°C)**: se la temperatura misurata supera 480°C il riscaldatore viene spento immediatamente, il display mostra **OVH** e il guasto rimane latched fino al ciclo di alimentazione
* **Runaway termico**: se il riscaldatore viene spento ma la temperatura **sale** di più di 60°C sopra il target per 8 secondi consecutivi (indicando un transistor Q1/IRF840 bloccato), viene attivato lo stesso guasto OVH. Il raffreddamento naturale (temperatura stabile o in calo) non attiva mai questo controllo.

## Compensazione della tensione di rete (110V / 220V)

Il partitore di tensione R14/R16 alimenta la tensione del bus DC all'ADC CH1. Il firmware scala automaticamente la potenza massima del riscaldatore per mantenere un'uscita termica costante indipendentemente dalla tensione di rete (110V o 220V AC), senza necessità di configurazione manuale.

## Schema elettrico CXG-E60WT

![Schema CXG-E60WT](/images/scheme.gif)

## Hardware aggiuntivo

Il seguente hardware aggiuntivo è stato installato:
- Interruttore a mercurio: https://www.aliexpress.com/item/32509962658.html?spm=a2g0s.9042311.0.0.274233edX3SZw4
- Cicalino SMD: https://www.aliexpress.com/item/4000043864737.html?spm=a2g0s.9042311.0.0.274233ediyCCli

![Lavori in corso — scheda aperta sul banco con ST-Link e adattatore UART](/images/screen6.jpeg)

Sentiti libero di utilizzare, modificare, aggiungere nuove funzionalità. Buona fortuna!

---

🌐 [English](README.md) | [Italiano](README_it.md) | [Русский](README_ru.md)
