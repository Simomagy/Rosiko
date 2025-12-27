#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RosikoGameManager.h" // Per EGamePhase e FTerritoryGameState
#include "EngineUtils.h" // Per TActorIterator
#include "RosikoGameState.generated.h"

/**
 * GameState per ROSIKO.
 * Contiene lo stato globale della partita, replicato a tutti i client.
 *
 * RESPONSABILITA':
 * - Fase di gioco corrente
 * - Turno corrente e ordine turni
 * - Territori e loro stato
 * - Colori disponibili per selezione
 * - Tempo di gioco
 * - MapSeed per generazione procedurale
 */
UCLASS()
class ROSIKO_API ARosikoGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARosikoGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	// === STATO GLOBALE PARTITA ===

	// Fase di gioco corrente
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentPhase, Category = "Game State")
	EGamePhase CurrentPhase = EGamePhase::Setup;

	// Indice del turno corrente nell'array TurnOrder
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentPlayerTurn, Category = "Game State")
	int32 CurrentPlayerTurn = 0;

	// Ordine turni randomizzato (array di PlayerID)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	TArray<int32> TurnOrder;

	// Stato di tutti i territori
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	TArray<FTerritoryGameState> Territories;

	// Colori ancora disponibili per selezione
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	TArray<FLinearColor> AvailableColors;

	// Tempo totale di gioco in secondi (calcolato da GameStartTimestamp)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	float GameTimeSeconds = 0.0f;

	// Timestamp Unix UTC di inizio partita (secondi dal 1970-01-01)
	// Usato per calcolare GameTimeSeconds anche dopo crash/riconnessioni
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	int64 GameStartTimestamp = 0;

	// Seed per generazione procedurale mappa
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	int32 MapSeed = 0;

	// Numero di player attesi per questa partita
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Game State")
	int32 ExpectedPlayerCount = 3;

	// Lista di PlayerID che hanno completato il caricamento e sono pronti
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ReadyPlayerIDs, Category = "Game State")
	TArray<int32> ReadyPlayerIDs;

	// === EVENTI REPLICAZIONE ===

	UFUNCTION()
	void OnRep_CurrentPhase();

	UFUNCTION()
	void OnRep_CurrentPlayerTurn();

	UFUNCTION()
	void OnRep_ReadyPlayerIDs();

	// === DELEGATE PER UI ===

	// Chiamato quando la lista di player pronti cambia (per aggiornare LoadingScreen)
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnReadyPlayersChanged);
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnReadyPlayersChanged OnReadyPlayersChanged;

	// === HELPER METHODS ===

	// Ottieni territorio per ID (solo C++, ritorna puntatore per efficienza)
	FTerritoryGameState* GetTerritory(int32 TerritoryID);

	// Ottieni territorio per ID (Blueprint-safe, ritorna copia)
	UFUNCTION(BlueprintCallable, Category = "Game State")
	FTerritoryGameState GetTerritoryByID(int32 TerritoryID, bool& bFound) const;

	// Ottieni PlayerID del giocatore attivo
	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetCurrentPlayerID() const;

	// Inizia il timer di gioco (salva timestamp corrente)
	// Chiamato dal GameManager quando inizia la partita vera (InitialDistribution)
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void StartGameTimer();

	// Aggiorna tempo di gioco calcolando dalla differenza con GameStartTimestamp
	// Chiamato periodicamente per mantenere GameTimeSeconds aggiornato
	void UpdateGameTime();

	// === READY STATE MANAGEMENT ===

	// Segna un player come pronto (chiamato dal server quando client notifica)
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void MarkPlayerReady(int32 PlayerID);

	// Verifica se un player è pronto
	UFUNCTION(BlueprintPure, Category = "Game State")
	bool IsPlayerReady(int32 PlayerID) const;

	// Ottieni numero di player pronti
	UFUNCTION(BlueprintPure, Category = "Game State")
	int32 GetNumReadyPlayers() const { return ReadyPlayerIDs.Num(); }

	// Verifica se tutti i player attesi sono pronti
	UFUNCTION(BlueprintPure, Category = "Game State")
	bool AreAllPlayersReady() const;

	// Ottieni lista nomi player non ancora pronti
	UFUNCTION(BlueprintPure, Category = "Game State")
	TArray<FString> GetNotReadyPlayerNames() const;
};

