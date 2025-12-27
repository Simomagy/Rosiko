# ROSIKO - Setup Checklist Completo

## 📋 Riepilogo Modifiche Implementate

### ✅ Sistema Selezione Colore Esercito (Early Game)

**File C++ creati:**
- `Core/RosikoGameMode.h/cpp` - GameMode per multiplayer
- `Core/UI/ColorSelectionWidget.h/cpp` - Widget UI selezione colore
- `Core/UI/GameUIController.h/cpp` - Controller UI eventi

**File C++ modificati:**
- `Core/RosikoGameManager.h/cpp` - Aggiunta fase ColorSelection + TurnOrder randomizzato
- `Core/Camera/RosikoCamera.cpp` - Rimosso AutoPossessPlayer (gestito da GameMode)
- `Troop/UI/TroopDisplayComponent.h/cpp` - Fix warning spam multiplayer

**Documentazione creata:**
- `docs/COLOR_SELECTION_SETUP.md` - Guida setup selezione colore
- `docs/MULTIPLAYER_SETUP.md` - Guida setup multiplayer + fix warning
- `docs/SETUP_CHECKLIST.md` - Questo documento

---

## 🎯 Azioni da Completare in Unreal Editor

### PRIORITÀ ALTA: Fix Warning Multiplayer

#### 1. Compila il Progetto C++
```
1. Chiudi Unreal Editor
2. Visual Studio/Rider → Build Solution
3. Riapri Unreal Editor
```

#### 2. Crea Blueprint GameMode
```
Content Browser → Right Click → Blueprint Class
├─ Parent Class: RosikoGameMode (cerca in C++ Classes)
└─ Nome: BP_RosikoGameMode

Apri BP_RosikoGameMode:
├─ Class Defaults → Default Pawn Class = BP_RosikoCamera
└─ Salva
```

#### 3. Configura World Settings
```
Window → World Settings
├─ Game Mode Override = BP_RosikoGameMode
└─ Salva livello
```

#### 4. Test Multiplayer
```
Play dropdown:
├─ Number of Players = 3
├─ Net Mode = Play As Listen Server
└─ Play

RISULTATO ATTESO:
✅ Log: "TroopDisplayComponent: Camera initialized successfully" (per ogni player)
❌ NESSUN warning: "Player pawn is not ARosikoCamera!"
```

**Guida completa:** `docs/MULTIPLAYER_SETUP.md`

---

### PRIORITÀ MEDIA: Sistema Selezione Colore

#### 1. Crea Widget Blueprint ColorSelection
```
Content Browser → Widget Blueprint
├─ Parent Class: ColorSelectionWidget (cerca in C++ Classes)
└─ Nome: WBP_ColorSelection

Layout suggerito:
Canvas Panel (Root)
├─ Overlay (Background nero semi-trasparente)
└─ Vertical Box (Centrato)
   ├─ Text: "Seleziona Colore Esercito"
   ├─ Text: [Bind to CurrentPlayerName]
   ├─ Wrap Box: [Generato dinamicamente da Blueprint]
   └─ Text: "Colori disponibili: [X]"

Event Graph:
├─ Event OnRefreshDisplay (implementa)
│  └─ For Each (AvailableColors)
│     ├─ Create Button Widget
│     ├─ Set Color Tint
│     ├─ Bind OnClick → Call OnColorSelected(Color)
│     └─ Add to Wrap Box
│
├─ Event OnSelectionSuccess (opzionale)
│  └─ Play Sound / Animation
│
└─ Event OnSelectionFailed (opzionale)
   └─ Show Error Message
```

#### 2. Crea Actor GameUIController
```
World Outliner → Place Actors
├─ Search: GameUIController
└─ Drag nel livello

Details Panel:
├─ Color Selection Widget Class = WBP_ColorSelection
└─ Salva
```

#### 3. Test Selezione Colore
```
1. MapGenerator → GenerateMap()
2. RosikoGameManager → StartGame()
3. Dovrebbe apparire WBP_ColorSelection per primo player
4. Clicca su un colore → passa al prossimo player
5. Quando tutti scelgono → distribuzione territori automatica
```

**Guida completa:** `docs/COLOR_SELECTION_SETUP.md`

---

## 🔍 Verifica Setup

### Test 1: Multiplayer Camera Fix
```bash
# Test Listen Server 3 player
Play → Number of Players: 3 → Net Mode: Listen Server

# Verifica log (filtro: LogTroopsDisplay)
✅ "Camera initialized successfully" × 3
❌ ZERO warning "Player pawn is not ARosikoCamera!"
```

### Test 2: Selezione Colore (Single Player)
```bash
# Test base flow
1. GenerateMap()
2. StartGame()
3. Fase ColorSelection attiva
4. Widget appare per Player random
5. Selezione colore funziona
6. Transizione automatica a InitialDistribution

# Verifica log (filtro: LogRosikoGameManager)
✅ "Turn order randomized: X, Y, Z, ..."
✅ "Color selection started. First player: ..."
✅ "Player X selected color RGB(...)"
✅ "All players selected colors. Starting territory distribution..."
```

### Test 3: Multiplayer + Selezione Colore
```bash
# Test completo
Play → Number of Players: 3 → Net Mode: Listen Server
1. Tutti e 3 i player vedono le loro camere ✅
2. Solo Player in turno vede widget selezione colore
3. Dopo selezione, passa al prossimo player
4. Quando tutti scelgono, distribuzione territori con colori corretti

# NOTA: Per ora solo server gestisce logica (listen server authority)
# Client vedono solo visuals (truppe con colori corretti)
```

---

## 📁 Struttura File Progetto

```
ROSIKO/Source/
├─ ROSIKO/
│  ├─ Core/
│  │  ├─ Camera/
│  │  │  ├─ RosikoCamera.h/cpp [MODIFICATO]
│  │  ├─ UI/
│  │  │  ├─ ColorSelectionWidget.h/cpp [NUOVO]
│  │  │  ├─ GameUIController.h/cpp [NUOVO]
│  │  ├─ RosikoGameManager.h/cpp [MODIFICATO]
│  │  ├─ RosikoGameMode.h/cpp [NUOVO] ← FIX MULTIPLAYER
│  ├─ Troop/
│  │  ├─ UI/
│  │  │  ├─ TroopDisplayComponent.h/cpp [MODIFICATO]
│  ├─ Configs/
│  │  ├─ GameRulesConfig.h/cpp
│  ├─ Map/
│  │  ├─ MapGenerator.h/cpp
│  │  ├─ Territory/
│  │  │  ├─ TerritoryActor.h/cpp
├─ docs/
│  ├─ CAMERA_SETUP.md
│  ├─ LOADING_SCREEN_SETUP.md
│  ├─ COLOR_SELECTION_SETUP.md [NUOVO]
│  ├─ MULTIPLAYER_SETUP.md [NUOVO]
│  ├─ SETUP_CHECKLIST.md [QUESTO FILE]
```

---

## 🐛 Troubleshooting Rapido

### Problema: Warning spam "Player pawn is not ARosikoCamera!"
**Causa:** GameMode non configurato per multiplayer
**Fix:** Segui "PRIORITÀ ALTA" sopra
**Doc:** `docs/MULTIPLAYER_SETUP.md`

### Problema: Selezione colore non appare
**Causa:** GameUIController non nel livello o WBP_ColorSelection non assegnato
**Fix:** Segui "PRIORITÀ MEDIA" punto 2
**Doc:** `docs/COLOR_SELECTION_SETUP.md`

### Problema: Camera non si muove
**Causa:** Enhanced Input non configurato
**Fix:** Segui `docs/CAMERA_SETUP.md` Step 2

### Problema: Territorio senza colore/truppe
**Causa:** GameManager non inizializzato o mappa non generata
**Fix:** Chiama `MapGenerator->GenerateMap()` prima di `GameManager->StartGame()`

### Problema: Compile error dopo pull/merge
**Causa:** Nuovo file non incluso nel progetto
**Fix:**
```
1. Chiudi Editor
2. Tasto destro su .uproject → Generate Visual Studio project files
3. Ricompila
4. Riapri Editor
```

---

## 📊 Stato Implementazione Features

| Feature | Stato | Note |
|---------|-------|------|
| Camera Strategica | ✅ Completo | Supporta single + multiplayer |
| Generazione Mappa | ✅ Completo | 70 territori procedurali |
| GameManager (Core Logic) | ✅ Completo | Single Source of Truth |
| Selezione Colore Early Game | ✅ C++ Completo | Richiede setup Blueprint in Editor |
| Ordine Turni Randomizzato | ✅ Completo | Shuffle deterministico |
| Fix Warning Multiplayer | ✅ C++ Completo | Richiede configurazione GameMode |
| Distribuzione Territori | ✅ Completo | Round-robin con TurnOrder |
| Piazzamento Truppe Iniziale | ✅ Completo | Fase InitialDistribution |
| UI Selezione Colore | ⚠️ Parziale | Widget C++ pronto, serve Blueprint |
| UI Controller Eventi | ⚠️ Parziale | Actor C++ pronto, serve setup livello |
| Multiplayer Completo | ⏳ Futuro | Serve replicazione GameState/PlayerState |
| Sistema Combattimento | ⏳ TODO | Da implementare |
| Sistema Rinforzi | ⏳ TODO | Da implementare |
| Sistema Carte | ⏳ TODO | Deck inizializzato, serve logica exchange |

---

## 🎯 Roadmap Prossime Features

### Sprint 1 (Completamento Early Game)
- ✅ Sistema selezione colore (C++ completo)
- ⚠️ Setup Blueprint widget colore (richiede Editor)
- ⚠️ Test multiplayer colore selection

### Sprint 2 (Core Gameplay Loop)
- ⏳ Sistema rinforzi inizio turno
- ⏳ Sistema combattimento (attacco territori)
- ⏳ Sistema movimento truppe (fase Fortify)
- ⏳ Gestione carte territorio

### Sprint 3 (UI/UX Polish)
- ⏳ HUD player (info turno, truppe, carte)
- ⏳ Animazioni territorio (attacco, conquista)
- ⏳ Feedback visivi (selezione, highlight, ecc.)
- ⏳ Sound effects

### Sprint 4 (Multiplayer Networking)
- ⏳ Convertire GameManager in GameState replicato
- ⏳ Creare PlayerState replicato
- ⏳ RPC per tutti i comandi (SelectColor, PlaceTroops, Attack)
- ⏳ Lobby pre-game
- ⏳ Dedicated Server support

---

## 📞 Supporto

Per problemi o domande:

1. **Check log**: Output Log con filtri (`LogRosikoGameManager`, `LogTroopsDisplay`, `LogTemp`)
2. **Check documentazione**: File `docs/*.md` per guide dettagliate
3. **Check TODO list**: Lista task correnti e stato implementazione
4. **Verifica setup**: Segui checklist in questo documento

---

## ✅ Quick Start (Per Chi Inizia Ora)

```bash
# 1. Compila progetto
Visual Studio/Rider → Build Solution

# 2. Apri Unreal Editor
Apri ROSIKO.uproject

# 3. Setup Multiplayer Fix (IMPORTANTE)
Content Browser → BP_RosikoGameMode (crea seguendo guida)
World Settings → Game Mode Override = BP_RosikoGameMode

# 4. Test base
Play → 1 player → Verifica camera funziona

# 5. Test multiplayer
Play → 3 players → Listen Server → Verifica zero warning

# 6. Setup ColorSelection (opzionale per ora)
Crea WBP_ColorSelection + GameUIController (segui guida)

# 7. Test completo
GenerateMap() → StartGame() → Verifica flow completo
```

**Tempo stimato setup:** 15-20 minuti

---

Ultimo aggiornamento: 2025-01-23

