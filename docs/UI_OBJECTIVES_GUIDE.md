# Guida Implementazione UI Obiettivi ROS!KO

## 📋 Panoramica

Il sistema UI obiettivi utilizza un **approccio hybrid C++/Blueprint**:
- **Logica in C++**: Tutta la business logic, replicazione, tracking completamenti
- **Layout in Blueprint**: Solo design visuale, binding widget, configurazione colori/spacing

**Questo significa**: devi solo creare il layout nel Designer e bindare i widget. Zero codice Blueprint da scrivere!

---

## 🎯 Classi C++ Disponibili

Tutte le classi base sono già implementate in C++:

| Classe C++ | Scopo | File |
|------------|-------|------|
| `UObjectiveCardWidget` | Singola carta obiettivo | `ROSIKO/Core/UI/ObjectiveCardWidget.h/.cpp` |
| `UObjectivesPanelWidget` | Pannello principale obiettivi | `ROSIKO/Core/UI/ObjectivesPanelWidget.h/.cpp` |
| `UVictoryPointsWidget` | Indicatore punti HUD | `ROSIKO/Core/UI/VictoryPointsWidget.h/.cpp` |
| `UObjectiveNotificationWidget` | Popup completamento | `ROSIKO/Core/UI/ObjectiveNotificationWidget.h/.cpp` |

---

## 📦 Step 1: WBP_ObjectiveCard

### 1.1 Creare Widget Blueprint

1. Content Browser → `Content/UI/Objectives/` (crea cartella se non esiste)
2. Right-click → **User Interface** → **Widget Blueprint**
3. Nome: `WBP_ObjectiveCard`
4. Apri il widget

### 1.2 Impostare Parent Class

**CRITICO**: Il widget DEVE ereditare dalla classe C++!

1. Toolbar → **Class Settings** (icona ingranaggio)
2. Details panel → **Parent Class**
3. Cerca e seleziona: `ObjectiveCardWidget`
4. **Compile** e **Save**

### 1.3 Designer - Layout

Crea questa hierarchy nel Designer:

```
Canvas Panel
└── Border [CardBorder] ← Is Variable ✓
    └── Vertical Box (padding 10)
        ├── Horizontal Box (header)
        │   ├── Text Block [TypeText] ← Is Variable ✓
        │   │   └─ Text: "OBIETTIVO PRINCIPALE"
        │   │   └─ Size: 16
        │   └── Image [CompletionIcon] ← Is Variable ✓
        │       └─ Size: 32x32
        │       └─ Visibility: Hidden
        │
        ├── Spacer (20px height)
        │
        ├── Text Block [DisplayNameText] ← Is Variable ✓
        │   └─ Text: "Nome Obiettivo"
        │   └─ Size: 24, Bold
        │
        ├── Spacer (10px)
        │
        ├── Text Block [DescriptionText] ← Is Variable ✓
        │   └─ Text: "Descrizione obiettivo..."
        │   └─ Size: 16
        │   └─ Auto Wrap Text: TRUE
        │
        ├── Spacer (10px)
        │
        └── Horizontal Box [VictoryPointsBox] ← Is Variable ✓
            ├── Image (stella icon)
            └── Text Block [VictoryPointsText] ← Is Variable ✓
                └─ Text: "10 pt"
```

### 1.4 Widget Binding (NOMI ESATTI!)

**IMPORTANTE**: I nomi tra parentesi quadre `[Nome]` DEVONO corrispondere esattamente ai nomi in C++.

Seleziona ogni widget e:
1. Details panel → spunta **"Is Variable"**
2. Rinomina esattamente come indicato (case-sensitive!)

**Tabella binding richiesti**:

| Widget nel Designer | Nome Variabile (esatto!) | Note |
|---------------------|--------------------------|------|
| Border principale | `CardBorder` | Sfondo carta |
| Text tipo obiettivo | `TypeText` | "PRINCIPALE" / "SECONDARIO" |
| Text nome | `DisplayNameText` | Titolo obiettivo |
| Text descrizione | `DescriptionText` | Multi-line wrap |
| Image completamento | `CompletionIcon` | Checkmark/stella |
| Horizontal Box footer | `VictoryPointsBox` | Container punti |
| Text punti | `VictoryPointsText` | "X pt" |

### 1.5 Configurazione Colori (Class Defaults)

1. Toolbar → **Class Defaults**
2. Details panel → modifica questi valori:

| Proprietà | Valore RGB | Descrizione |
|-----------|------------|-------------|
| Main Objective Color | (1.0, 0.84, 0.0, 1.0) | Gold |
| Secondary Objective Color | (0.75, 0.75, 0.75, 1.0) | Silver |
| Completed Color | (0.0, 1.0, 0.0, 0.3) | Green glow |
| Normal Color | (0.25, 0.25, 0.25, 0.8) | Dark gray |

### 1.6 Stili Consigliati

**CardBorder**:
- Brush: Solid Color o texture pergamena
- Padding: (10, 10, 10, 10)
- Border Thickness: 2px

**DisplayNameText**:
- Font Size: 24
- Weight: Bold

**DescriptionText**:
- Font Size: 14
- Auto Wrap: TRUE
- Justification: Left

### 1.7 (Opzionale) Animazione Completamento

Se vuoi un effetto custom quando l'obiettivo viene completato:

**Event Graph**:
1. Right-click → Add Event → Event On Objective Completed
2. Aggiungi: Play Animation (bounce/glow)

```
Event On Objective Completed
└─→ Play Animation (es. "BounceEffect")
```

**Questo è tutto!** La carta è pronta. La logica C++ gestisce automaticamente:
- Popolazione dati
- Cambio colori main/secondary
- Visibilità completamento
- Punti vittoria

---

## 📦 Step 2: WBP_ObjectivesPanel

### 2.1 Creare Widget Blueprint

1. Content Browser → `Content/UI/Objectives/`
2. Right-click → Widget Blueprint
3. Nome: `WBP_ObjectivesPanel`

### 2.2 Impostare Parent Class

1. Class Settings → Parent Class: `ObjectivesPanelWidget`
2. Compile e Save

### 2.3 Designer - Layout

```
Canvas Panel (Fill screen)
└── Border (background blur, semi-transparent)
    └── Vertical Box (Center, padding 40)
        │
        ├── Text Block (titolo)
        │   └─ Text: "I TUOI OBIETTIVI"
        │   └─ Size: 32, Bold, Center
        │
        ├── Spacer (30px)
        │
        ├── WBP_ObjectiveCard [MainObjectiveCard] ← Is Variable ✓
        │   └─ Size: 500x250
        │
        ├── Spacer (40px)
        │
        ├── Text Block [SecondariesTitle] ← Is Variable ✓
        │   └─ Text: "Obiettivi Secondari"
        │   └─ Size: 24
        │
        ├── Spacer (20px)
        │
        └── Vertical Box [SecondaryCardsContainer] ← Is Variable ✓
            └── (VUOTO - popolato in C++)
```

### 2.4 Widget Binding Richiesti

| Widget | Nome Esatto | Note |
|--------|-------------|------|
| WBP_ObjectiveCard (main) | `MainObjectiveCard` | Carta principale |
| Vertical Box (container) | `SecondaryCardsContainer` | Container carte secondarie |
| Text Block (subtitle) | `SecondariesTitle` | Titolo sezione secondari |

### 2.5 Configurazione (Class Defaults)

| Proprietà | Valore | Descrizione |
|-----------|--------|-------------|
| Secondary Card Class | `WBP_ObjectiveCard` | Classe per carte secondarie |
| Card Spacing | 15.0 | Pixel tra carte |
| Auto Update Interval | 0.5 | Secondi tra check |

### 2.6 (Opzionale) Animazione Apertura

Crea animazione "SlideIn":

1. Animations panel → `+ Animation` → Nome: `SlideIn`
2. Track su Canvas Panel:
   - **Render Transform → Translation X**:
     - 0.0s: 1920 (fuori schermo destra)
     - 0.3s: 0 (posizione finale)
   - **Render Opacity**:
     - 0.0s: 0.0
     - 0.3s: 1.0

**Event Graph** (opzionale):

```
Event Construct
└─→ Play Animation (SlideIn)
```

**Fatto!** Il pannello è completo. Il C++ gestisce:
- Ottieni PlayerState automaticamente
- Crea dinamicamente N carte secondarie
- Update automatico ogni 0.5s
- Tracking completamenti
- Event Dispatcher `OnObjectiveCompleted`

---

## 📦 Step 3: WBP_VictoryPoints

### 3.1 Creare Widget Blueprint

1. Content Browser → `Content/UI/Objectives/`
2. Widget Blueprint → `WBP_VictoryPoints`

### 3.2 Impostare Parent Class

Parent Class: `VictoryPointsWidget`

### 3.3 Designer - Layout

```
Horizontal Box
├── Image [StarIcon] ← Is Variable ✓
│   └─ Size: 32x32
│   └─ Texture: star/trophy icon
│
├── Spacer (10px width)
│
└── Text Block [PointsText] ← Is Variable ✓
    └─ Text: "Punti: 0"
    └─ Size: 20
```

### 3.4 Widget Binding

| Widget | Nome Esatto |
|--------|-------------|
| Image stella | `StarIcon` |
| Text punti | `PointsText` |

### 3.5 Configurazione (Class Defaults)

| Proprietà | Valore | Descrizione |
|-----------|--------|-------------|
| Update Interval | 1.0 | Secondi tra update |
| High Score Color | (1.0, 0.84, 0.0, 1.0) | Gold ≥30 pt |
| Medium Score Color | (0.75, 0.75, 0.75, 1.0) | Silver ≥15 pt |
| Low Score Color | (1.0, 1.0, 1.0, 1.0) | White <15 pt |

**Fatto!** Il widget aggiorna automaticamente punti e colore.

---

## 📦 Step 4: WBP_ObjectiveNotification

### 4.1 Creare Widget Blueprint

1. Content Browser → `Content/UI/Objectives/`
2. Widget Blueprint → `WBP_ObjectiveNotification`

### 4.2 Impostare Parent Class

Parent Class: `ObjectiveNotificationWidget`

### 4.3 Designer - Layout

```
Overlay
└── Border [NotificationBorder] ← Is Variable ✓
    └─ Brush: Box blur + glow
    └─ Padding: 30

    └── Vertical Box (Center)
        │
        ├── Image [CompletionIcon] ← Is Variable ✓
        │   └─ Size: 64x64
        │   └─ Texture: checkmark/star large
        │
        ├── Spacer (15px)
        │
        ├── Text Block [HeaderText] ← Is Variable ✓
        │   └─ Text: "OBIETTIVO COMPLETATO!"
        │   └─ Size: 32, Bold, Center
        │
        ├── Spacer (10px)
        │
        ├── Text Block [ObjectiveNameText] ← Is Variable ✓
        │   └─ Text: "Nome Obiettivo"
        │   └─ Size: 24, Center
        │
        ├── Spacer (10px)
        │
        └── Text Block [PointsText] ← Is Variable ✓
            └─ Text: "+10 punti"
            └─ Size: 20, Center
```

### 4.4 Widget Binding

| Widget | Nome Esatto |
|--------|-------------|
| Border principale | `NotificationBorder` |
| Image icon | `CompletionIcon` |
| Text header | `HeaderText` |
| Text nome obiettivo | `ObjectiveNameText` |
| Text punti | `PointsText` |

### 4.5 Creare Animazione "FadeInOut"

**Animations panel** → `+ Animation` → Nome: `FadeInOut` (nome esatto!)

**Track su NotificationBorder**:

| Property | Keyframe | Time | Value |
|----------|----------|------|-------|
| Render Opacity | 1 | 0.0s | 0.0 |
| Render Opacity | 2 | 0.3s | 1.0 |
| Render Opacity | 3 | 2.5s | 1.0 |
| Render Opacity | 4 | 3.0s | 0.0 |
| Render Transform Scale | 1 | 0.0s | (0.5, 0.5) |
| Render Transform Scale | 2 | 0.3s | (1.1, 1.1) |
| Render Transform Scale | 3 | 0.5s | (1.0, 1.0) |
| Render Transform Scale | 4 | 3.0s | (0.8, 0.8) |

**IMPORTANTE**: L'animazione DEVE chiamarsi `FadeInOut` (il C++ cerca questo nome).

### 4.6 Configurazione Audio (Class Defaults)

| Proprietà | Valore | Descrizione |
|-----------|--------|-------------|
| Objective Completed Sound | (Sound Cue) | Audio secondari |
| Main Objective Completed Sound | (Sound Cue) | Audio principale (epico) |
| Auto Remove Delay | 3.0 | Secondi prima auto-remove |

**Fatto!** La notifica funziona automaticamente.

---

## 🎮 Step 5: Integrare in GameHUDWidget

### 5.1 Aprire GameHUDWidget esistente

Apri il tuo widget HUD principale (es. `WBP_GameHUD`).

### 5.2 Aggiungere Widget al Designer

**Canvas Panel (root esistente)**:

Aggiungi questi 3 widget:

#### A. VictoryPoints (Top-Right)

- Widget: `WBP_VictoryPoints`
- Nome variabile: `VictoryPointsWidget` (spunta Is Variable)
- **Anchors**: Top-Right (0.0, 1.0, 0.0, 0.0)
- **Position**: X: -20, Y: 20
- **Alignment**: X: 1.0, Y: 0.0
- **Size**: 200x50

#### B. ObjectivesPanel (Full Screen)

- Widget: `WBP_ObjectivesPanel`
- Nome variabile: `ObjectivesPanel` (spunta Is Variable)
- **Anchors**: Fill (0.0, 1.0, 0.0, 1.0)
- **Offsets**: tutti a 0
- **Visibility**: Hidden (default chiuso)
- **Z-Order**: 100

#### C. NotificationsContainer (Top-Center)

- Widget: **Canvas Panel** (vuoto)
- Nome variabile: `NotificationsContainer` (spunta Is Variable)
- **Anchors**: Top-Center (0.5, 0.5, 0.0, 0.0)
- **Position**: X: -300, Y: 50
- **Size**: 600x200

### 5.3 Setup Input (Enhanced Input System)

#### A. Verifica che hai creato (già fatto da te):

- ✅ `IA_ToggleObjectives` (Input Action)
- ✅ `IMC_Game` (Input Mapping Context con IA_ToggleObjectives → Tab)

#### B. Configurare PlayerController

Il PlayerController C++ è già configurato per Enhanced Input!

**Devi solo assegnare i reference in Blueprint**:

1. Apri il tuo PlayerController Blueprint (o crea override di `BP_RosikoPlayerController`)
2. **Class Defaults** panel:
   - `Game Mapping Context` → seleziona `IMC_Game`
   - `Toggle Objectives Action` → seleziona `IA_ToggleObjectives`
   - `Game Mapping Priority` → 0 (default)
3. Compile e Save

Il C++ gestisce automaticamente:
- Registrazione IMC in `BeginPlay()`
- Binding Input Action in `SetupInputComponent()`
- Callback `ToggleObjectivesPanel()` quando premi Tab

### 5.4 Implementare Toggle nel HUD

Il PlayerController chiama già `ToggleObjectivesPanel()`, ma devi implementare la logica nel tuo HUD.

**Opzione 1: Event Dispatcher (Consigliata)**

Nel tuo **GameHUD Widget**:

1. Event Dispatchers → `+ Event Dispatcher`
   - Nome: `OnToggleObjectives`

2. Event Graph:

```
Custom Event: ToggleObjectives (crea nuovo event)
└─→ ObjectivesPanel (Get)
    └─→ Get Visibility
        └─→ == Visible? (Branch)
            ├─[True]→ Set Visibility (Hidden)
            └─[False]→ Set Visibility (Visible)
```

**Opzione 2: Direttamente nel PlayerController (C++)**

Se preferisci gestire tutto in C++, nel `RosikoPlayerController.cpp` modifica `ToggleObjectivesPanel()`:

```cpp
void ARosikoPlayerController::ToggleObjectivesPanel(const FInputActionValue& Value)
{
    // Cache reference al widget HUD
    if (!CachedHUDWidget)
    {
        // Trova widget HUD (implementa caching)
        return;
    }

    // Cast a ObjectivesPanelWidget e chiama toggle
    if (UObjectivesPanelWidget* Panel = CachedHUDWidget->ObjectivesPanel)
    {
        ESlateVisibility CurrentVis = Panel->GetVisibility();
        Panel->SetVisibility(
            CurrentVis == ESlateVisibility::Visible
            ? ESlateVisibility::Hidden
            : ESlateVisibility::Visible
        );
    }
}
```

### 5.5 Event Graph - Bind OnObjectiveCompleted

**Event Construct**:

```
Event Construct
└─→ ObjectivesPanel (Get)
    └─→ Assign On Objective Completed (Event Dispatcher)
        └─→ Event generato:
            ├─ CompletedObjective (input)
            │
            └─→ Create Widget
                ├─ Class: WBP_ObjectiveNotification
                ├─ Owning Player: Get Player Controller
                │
                └─→ Show Notification (function)
                │   └─ Input: CompletedObjective
                │
                └─→ Add Child to Canvas Panel
                    ├─ Target: NotificationsContainer
                    └─ Content: (widget creato)
```

**Come creare**:
1. Trascina `ObjectivesPanel` → Get
2. Dal pin, cerca "Assign On Objective Completed"
3. Connetti Exec da Event Construct
4. Dal nodo Event generato (verde), collega alla logica Create Widget
5. Usa `Create Widget` (class: WBP_ObjectiveNotification)
6. Chiama `Show Notification` sul widget creato
7. `Add Child to Canvas Panel` (target: NotificationsContainer)

**Fatto!** Tutto integrato e funzionante.

---

## ✅ Testing Checklist

### Test Base

- [ ] Obiettivi assegnati dopo ColorSelection
- [ ] Pannello mostra 1 main + N secondari (configurabile)
- [ ] Toggle pannello con Tab funziona
- [ ] VictoryPoints aggiorna in tempo reale
- [ ] Notifica appare al completamento

### Test Replicazione Multiplayer

- [ ] Obiettivi replicati solo al proprietario
- [ ] Altri player NON vedono i miei obiettivi
- [ ] Completamenti sincronizzati client-server

### Test Edge Cases

- [ ] Funziona con 0 secondari (solo main)
- [ ] Funziona con 10 secondari
- [ ] Non crasha se PlayerState è null
- [ ] Responsive a diverse risoluzioni

---

## 🎨 Styling Suggerito

### Colori Tema

| Elemento | Colore RGB | Hex | Uso |
|----------|------------|-----|-----|
| Gold | (1.0, 0.84, 0.0) | #FFD700 | Main objective, high score |
| Silver | (0.75, 0.75, 0.75) | #C0C0C0 | Secondary objective, medium score |
| Green | (0.0, 1.0, 0.0) | #00FF00 | Completato |
| Dark Gray | (0.25, 0.25, 0.25) | #404040 | Background normale |

### Font Sizes

- **Titoli**: 32px, Bold
- **Sottotitoli**: 24px, Bold
- **Nomi obiettivi**: 20-24px
- **Descrizioni**: 14-16px, Wrap
- **UI Info**: 16-18px

### Spacing

- **Padding card**: 10-15px
- **Spacing tra carte**: 15-20px
- **Margini pannello**: 40px
- **Border thickness**: 2px

---

## 🚀 Estensioni Opzionali

### Tooltip Progresso

Hover su carta mostra progresso condizioni:

```
Obiettivo: Conquistatore
Progresso:
  ✓ Europa (14/14 territori)
  ⊗ Asia (5/18 territori)
  ⊗ Africa (0/12 territori)
```

Implementazione: Override evento `OnMouseEnter` nella carta, crea widget tooltip dinamico.

### Indicators su Mappa

Evidenzia territori target per obiettivi attivi (outline colorato sulla mappa).

### History Panel

Pannello che mostra tutti gli obiettivi completati da tutti i player (post-game).

### Victory Screen

Schermata finale con obiettivi rivelati, timeline, statistiche.

---

## 📚 API Reference Rapida

### UObjectiveCardWidget

| Metodo | Descrizione |
|--------|-------------|
| `SetObjectiveData(data, isMain)` | Setta obiettivo e refresh UI |
| `RefreshDisplay()` | Forza refresh visuale |
| `GetObjectiveData()` | Ottieni dati correnti |
| `OnObjectiveCompleted` (Event) | Implementabile per animazioni custom |

### UObjectivesPanelWidget

| Metodo | Descrizione |
|--------|-------------|
| `InitializePanel()` | Setup iniziale (automatico) |
| `UpdateObjectivesStatus()` | Check cambio stato (automatico) |
| `RefreshAllCards()` | Forza refresh tutte carte |
| `OnObjectiveCompleted` (Dispatcher) | Evento completamento obiettivo |

### UVictoryPointsWidget

| Metodo | Descrizione |
|--------|-------------|
| `UpdatePoints()` | Aggiorna punti visualizzati (automatico) |

### UObjectiveNotificationWidget

| Metodo | Descrizione |
|--------|-------------|
| `ShowNotification(objective)` | Mostra notifica con animazione |

---

## 🐛 Troubleshooting

### "Cannot find widget X for binding"

**Problema**: Nome widget non corrisponde al C++.

**Soluzione**:
1. Verifica nome ESATTO (case-sensitive)
2. Verifica che "Is Variable" sia spuntato
3. Ricompila Blueprint

### Pannello non si popola

**Problema**: PlayerState non trovato.

**Soluzione**:
1. Verifica che gioco sia in modalità corretta (non solo Editor)
2. Check che PlayerState sia `ARosikoPlayerState`
3. Log in Output Log per vedere errori C++

### Notifiche non appaiono

**Problema**: Event Dispatcher non bindato.

**Soluzione**:
1. Verifica binding in GameHUD Event Construct
2. Check che `NotificationsContainer` esista
3. Verifica che animazione `FadeInOut` esista

### Update non funziona

**Problema**: NativeTick disabilitato.

**Soluzione**: I widget C++ hanno Tick abilitato di default, verifica che Class Defaults non lo disabilitino.

---

## 📝 File Creati - Checklist Finale

### Blueprint Widgets (da creare)
- [ ] `Content/UI/Objectives/WBP_ObjectiveCard.uasset`
- [ ] `Content/UI/Objectives/WBP_ObjectivesPanel.uasset`
- [ ] `Content/UI/Objectives/WBP_VictoryPoints.uasset`
- [ ] `Content/UI/Objectives/WBP_ObjectiveNotification.uasset`

### Assets Necessari
- [ ] Texture checkmark/star per completamento
- [ ] Texture sfondo carta (pergamena opzionale)
- [ ] Sound Cue completamento secondario
- [ ] Sound Cue completamento principale (epico)
- [ ] Font custom (opzionale)

### Modifiche a File Esistenti
- [ ] GameHUDWidget: aggiunti 3 widget + event graph
- [ ] Project Settings: Input Action `ToggleObjectives`

---

## 🎯 Riepilogo Workflow

**Per ogni widget**:

1. ✅ Crea Widget Blueprint
2. ✅ Imposta Parent Class C++
3. ✅ Design layout nel Designer
4. ✅ Binda widget con nomi ESATTI
5. ✅ (Opzionale) Crea animazioni
6. ✅ (Opzionale) Configura Class Defaults
7. ✅ Compile e Save

**Zero codice Blueprint da scrivere** - tutto gestito in C++!

---

Fine della guida! 🎉

Il sistema è production-ready, type-safe, performante e facile da mantenere.
