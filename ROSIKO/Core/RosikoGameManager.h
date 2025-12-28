#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Configs/GameRulesConfig.h"
#include "ROSIKO/Configs/ObjectivesConfig.h"
#include "RosikoGameManager.generated.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	Setup,              // Pre-game: lobby, configurazione
	ColorSelection,     // Selezione colore esercito (early game)
	InitialDistribution, // Distribuzione carri iniziali
	Reinforce,          // Fase rinforzi
	Attack,             // Fase attacco
	Fortify,            // Fase movimento carri
	GameOver            // Partita terminata
};

USTRUCT(BlueprintType)
struct FPlayerGameState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 PlayerID = -1;

	UPROPERTY(BlueprintReadWrite)
	FString PlayerName = "Player";

	UPROPERTY(BlueprintReadWrite)
	FLinearColor PlayerColor = FLinearColor::White;

	UPROPERTY(BlueprintReadWrite)
	int32 TroopsToPlace = 0; // Carri rimanenti da piazzare (fase distribuzione/rinforzi)

	UPROPERTY(BlueprintReadWrite)
	TArray<int32> OwnedTerritories; // Lista ID territori posseduti

	UPROPERTY(BlueprintReadWrite)
	TArray<FTerritoryCard> Hand; // Carte territorio in mano

	UPROPERTY(BlueprintReadWrite)
	int32 CardExchangeCount = 0; // Numero di scambi carte effettuati (per bonus progressivo)

	UPROPERTY(BlueprintReadWrite)
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadWrite)
	bool bIsAI = false; // Se true, questo player è controllato da AI

	UPROPERTY(BlueprintReadWrite)
	bool bHasSelectedColor = false; // Se true, questo player ha già scelto il colore
};

USTRUCT(BlueprintType)
struct FTerritoryGameState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 TerritoryID = -1;

	UPROPERTY(BlueprintReadWrite)
	int32 OwnerID = -1; // PlayerID proprietario (-1 = neutrale)

	UPROPERTY(BlueprintReadWrite)
	int32 Troops = 0; // Numero carri armati
};

/**
 * Manager centrale per logica di gioco ROSIKO.
 * DESIGN: Per ora è un Actor replicato. In futuro diventerà GameState con full replication.
 * Gestisce lo stato di gioco autoritativo (server) e lo replica ai client.
 */
UCLASS()
class ROSIKO_API ARosikoGameManager : public AActor
{
	GENERATED_BODY()

public:
	ARosikoGameManager();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// === CONFIGURAZIONE ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UGameRulesConfig* GameRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UObjectivesConfig* ObjectivesConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "3", ClampMax = "10"))
	int32 NumPlayers = 3; // Per ora hardcoded, poi verrà da lobby multiplayer

	// Colori disponibili per giocatori (10 colori per 10 player max)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TArray<FLinearColor> PlayerColors = {
		FLinearColor(1.0f, 0.0f, 0.0f),    // Rosso
		FLinearColor(0.0f, 0.3f, 1.0f),    // Blu
		FLinearColor(0.0f, 0.8f, 0.0f),    // Verde
		FLinearColor(1.0f, 1.0f, 0.0f),    // Giallo
		FLinearColor(1.0f, 0.5f, 0.0f),    // Arancione
		FLinearColor(0.6f, 0.0f, 0.8f),    // Viola
		FLinearColor(0.0f, 1.0f, 1.0f),    // Ciano
		FLinearColor(1.0f, 0.0f, 1.0f),    // Magenta
		FLinearColor(0.6f, 0.4f, 0.2f),    // Marrone
		FLinearColor(1.0f, 0.75f, 0.8f)    // Rosa
	};

	// === STATO GIOCO (MIGRATO A GAMESTATE/PLAYERSTATE) ===
	// NOTA: Dati ora in ARosikoGameState e ARosikoPlayerState
	// Usare helper methods GetRosikoGameState() e GetRosikoPlayerState() per accesso

	// DEPRECATO: Ora in ARosikoGameState::CurrentPhase
	// UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentPhase, Category = "Game State")
	// EGamePhase CurrentPhase = EGamePhase::Setup;

	// DEPRECATO: Ora in ARosikoGameState::CurrentPlayerTurn
	// UPROPERTY(ReplicatedUsing = OnRep_CurrentPlayerTurn, BlueprintReadOnly, Category = "Game State")
	// int32 CurrentPlayerTurn = 0;

	// DEPRECATO: Dati per-player ora in ARosikoPlayerState
	// UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	// TArray<FPlayerGameState> Players;

	// DEPRECATO: Ora in ARosikoGameState::Territories
	// UPROPERTY(BlueprintReadOnly, Category = "Game State")
	// TMap<int32, FTerritoryGameState> Territories;

	// Mazzo carte territorio (rimane qui, non serve replicare)
	UPROPERTY(BlueprintReadOnly, Category = "Game State")
	TArray<FTerritoryCard> CardDeck;

	// Mazzi obiettivi filtrati per configurazione corrente (server-side only)
	TArray<FObjectiveDefinition> ValidMainObjectives;
	TArray<FObjectiveDefinition> ValidSecondaryObjectives;

	// DEPRECATO: Ora in ARosikoGameState::TurnOrder
	// UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	// TArray<int32> TurnOrder;

	// DEPRECATO: Ora in ARosikoGameState::AvailableColors
	// UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	// TArray<FLinearColor> AvailableColors;

	// === COMANDI PUBBLICI (diventeranno Server RPC in multiplayer) ===

	// Inizia partita (chiamato dopo generazione mappa)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGame();

	// Player seleziona colore esercito (durante fase ColorSelection)
	// DEPRECATO: Chiamare tramite PlayerController invece
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void SelectPlayerColor(int32 PlayerID, FLinearColor ChosenColor);

	// Chiamata diretta dal server (via PlayerController RPC)
	void SelectPlayerColor_Direct(int32 PlayerID, FLinearColor ChosenColor);

	// Ottieni colori ancora disponibili per selezione
	UFUNCTION(BlueprintPure, Category = "Game State")
	TArray<FLinearColor> GetAvailableColors() const;

	// === HELPER METHODS PER ACCESSO GAMESTATE/PLAYERSTATE ===

	// Ottieni RosikoGameState (cast rapido)
	UFUNCTION(BlueprintPure, Category = "Game State")
	class ARosikoGameState* GetRosikoGameState() const;

	// Ottieni RosikoPlayerState per PlayerID (GameManagerPlayerID)
	UFUNCTION(BlueprintPure, Category = "Game State")
	class ARosikoPlayerState* GetRosikoPlayerState(int32 PlayerID) const;

	// Ottieni tutti i PlayerStates attivi
	UFUNCTION(BlueprintPure, Category = "Game State")
	TArray<class ARosikoPlayerState*> GetAllPlayerStates() const;

	// === OBIETTIVI ===

	// Assegna obiettivi (1 principale + 4 secondari) a tutti i giocatori
	// Chiamato dopo ColorSelection, prima di DistributeInitialTerritories
	UFUNCTION(BlueprintCallable, Category = "Objectives")
	void AssignObjectivesToAllPlayers();

	// Verifica completamento obiettivi per un giocatore specifico
	// Chiamato automaticamente a fine turno
	UFUNCTION(BlueprintCallable, Category = "Objectives")
	bool CheckPlayerObjectives(int32 PlayerID);

	// Verifica completamento obiettivi per tutti i giocatori
	// Usato per calcolo vittoria finale
	UFUNCTION(BlueprintCallable, Category = "Objectives")
	void CheckAllObjectivesCompletion();

public:

	// Player piazza carri su territorio (durante fase distribuzione iniziale o rinforzi)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	bool PlaceTroops(int32 PlayerID, int32 TerritoryID, int32 Amount);

	// Termina turno corrente (passa al prossimo player)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndTurn();

	// Termina fase corrente (es. da Reinforce → Attack)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void EndPhase();

	// === QUERY (BlueprintPure = read-only) ===

	UFUNCTION(BlueprintPure, Category = "Game State")
	FTerritoryGameState GetTerritoryState(int32 TerritoryID) const;

	UFUNCTION(BlueprintPure, Category = "Game State")
	FPlayerGameState GetPlayerState(int32 PlayerID) const;

	UFUNCTION(BlueprintPure, Category = "Game State")
	FPlayerGameState GetCurrentPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Game State")
	bool CanPlaceTroops(int32 PlayerID, int32 TerritoryID) const;

	// === EVENTI (per UI/Notifiche) ===

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerritoryUpdated, int32, TerritoryID);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTerritoryUpdated OnTerritoryUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerUpdated, int32, PlayerID);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPlayerUpdated OnPlayerUpdated;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EGamePhase, OldPhase, EGamePhase, NewPhase);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnPhaseChanged OnPhaseChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnChanged, int32, NewPlayerTurn);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTurnChanged OnTurnChanged;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnColorSelectionRequired, int32, PlayerID, const TArray<FLinearColor>&, AvailableColors);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnColorSelectionRequired OnColorSelectionRequired;

private:
	// === INIZIALIZZAZIONE ===
	void InitializePlayers();
	void InitializeTerritories();
	void InitializeCardDeck();
	void InitializeTurnOrder(); // Crea ordine turni randomizzato
	void StartColorSelection(); // Avvia fase selezione colori
	void DistributeInitialTerritories();
	void StartInitialPlacement();

	// === OBIETTIVI - INTERNAL ===
	void FilterAndShuffleObjectives(); // Filtra e mescola mazzi obiettivi
	void AssignObjectivesToPlayer(ARosikoPlayerState* PS); // Assegna obiettivi a un singolo player
	bool EvaluateObjectiveCondition(const FObjectiveCondition& Condition, ARosikoPlayerState* PS); // Valuta singola condizione
	bool DoesPlayerControlContinent(int32 PlayerID, int32 ContinentID); // Helper per controllo continenti
	int32 CountPlayerTerritoriesInContinent(int32 PlayerID, int32 ContinentID); // Conta territori in continente

	// === HELPERS ===
	void BroadcastTerritoryUpdate(int32 TerritoryID); // Aggiorna visuals + evento
	void BroadcastPlayerUpdate(int32 PlayerID);
	void RefreshAllTerritoryDisplays(); // Forza refresh di tutti i territori (chiamato all'inizio)
	void ChangePhase(EGamePhase NewPhase);
	void ChangeTurn(int32 NewPlayerIndex);

	// Reference al MapGenerator (per ottenere lista territori)
	UPROPERTY()
	class AMapGenerator* MapGenerator;

	// RNG per shuffle carte/distribuzione (usa stesso seed di MapGenerator per determinismo)
	FRandomStream GameRNG;

	// Timer per aggiornare GameTimeSeconds ogni secondo
	FTimerHandle GameTimeUpdateTimer;

	// === REPLICATION CALLBACKS (DEPRECATI, ora in GameState) ===

	// DEPRECATO: Ora in ARosikoGameState::OnRep_CurrentPhase
	// UFUNCTION()
	// void OnRep_CurrentPhase();

	// DEPRECATO: Ora in ARosikoGameState::OnRep_CurrentPlayerTurn
	// UFUNCTION()
	// void OnRep_CurrentPlayerTurn();

	// Multicast RPC per notificare i client del cambio turno durante ColorSelection
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_NotifyColorSelectionTurn(int32 PlayerID, const TArray<FLinearColor>& AvailableColorsList);

	// Multicast RPC per notificare tutti i client di un update territorio
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_NotifyTerritoryUpdate(int32 TerritoryID, int32 OwnerID, int32 TroopCount);
};
