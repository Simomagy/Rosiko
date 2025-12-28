# Assegnazione obbiettivi

# Titolo: Assegnazione carte obiettivo (1 principale + 4 secondarie) – Setup ROS!KO

## Contesto

In fase di setup, dopo territori, colori e riserve, ogni giocatore deve ricevere:
- 1 carta **Obiettivo Principale** (tipo “missione segreta” alla Risiko, ma adattata al design ROS!KO).
- 4 carte **Obiettivi Secondari**, che forniscono **punti vittoria** usati nel calcolo del vincitore a fine partita.[1][2]

***

## Obiettivi del task

1. Definire una struttura dati per obiettivi principali e secondari (testo, tipo, valore in punti, condizioni di completamento a livello di data).
2. Implementare un sistema di **pesca/assegnazione automatica**:
   - 1 obiettivo principale univoco per giocatore (o secondo regole di duplicazione definite nel design).
   - 4 obiettivi secondari per giocatore, evitando combinazioni vietate/doppioni non desiderati.
3. Salvare su `PlayerState` le carte assegnate e marcare internamente quali sono principali e quali secondarie.
4. Preparare il sistema perché, durante la partita, i punti forniti dagli obiettivi possano essere sommati e usati per determinare il vincitore (anche in caso di mancata eliminazione totale degli avversari).[3][4]

***

## Requisiti funzionali

- In **setup**:
  - Separare i mazzi:
    - Mazzo Obiettivi Principali.
    - Mazzo Obiettivi Secondari.
  - Filtrare/validare le carte in base al numero di giocatori e ai colori presenti (come succede per le missioni che eliminano un colore non in partita nelle varianti di Risiko).[5][2]
- Per ogni giocatore:
  - Pescare una carta obiettivo principale valida e assegnarla.
  - Pescare 4 carte obiettivi secondari valide (secondo regole di unicità definite: es. nessun duplicato esatto per lo stesso giocatore).
- Le carte devono essere **nascoste agli altri giocatori**, ma visibili nella UI del proprietario.
- Prevedere API che, più avanti, permettano al sistema di punteggio di interrogare:
  - Lista obiettivi completati/non completati.
  - Punti totali generati dagli obiettivi secondari.

---
