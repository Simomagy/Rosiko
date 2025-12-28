#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "../Configs/GameRulesConfig.h" // Per FTerritoryCard
#include "../Configs/ObjectivesConfig.h" // Per FAssignedObjective
#include "RosikoPlayerState.generated.h"

/**
 * PlayerState custom per ROSIKO.
 * Contiene informazioni persistenti del player che devono essere replicate a tutti i client.
 */
UCLASS()
class ROSIKO_API ARosikoPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ARosikoPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// === GAME MANAGER INTEGRATION ===

	// ID del player nel GameManager (0, 1, 2, ..., 9)
	// Questo è l'ID usato per tutte le operazioni di gioco
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Info")
	int32 GameManagerPlayerID = -1;

	// Colore esercito del player (assegnato durante ColorSelection)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Info")
	FLinearColor ArmyColor = FLinearColor::White;

	// Se true, questo player ha completato la selezione colore
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Player Info")
	bool bHasSelectedColor = false;

	// === STATO DI GIOCO PER-PLAYER ===

	// Truppe disponibili da piazzare (fase InitialDistribution/Reinforce)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 TroopsToPlace = 0;

	// Lista ID territori posseduti dal player
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	TArray<int32> OwnedTerritoryIDs;

	// Carte territorio in mano
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	TArray<FTerritoryCard> Hand;

	// Numero di scambi carte effettuati (per calcolo bonus progressivo)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 CardExchangeCount = 0;

	// Se true, il player è ancora vivo nella partita
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	bool bIsAlive = true;

	// Se true, il player è controllato da AI (non da umano)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	bool bIsAI = false;

	// === OBIETTIVI ===

	// Obiettivo principale assegnato (missione segreta)
	// Replicato solo al proprietario per mantenere la segretezza
	UPROPERTY(ReplicatedUsing = OnRep_MainObjective, BlueprintReadOnly, Category = "Objectives")
	FAssignedObjective MainObjective;

	// Obiettivi secondari assegnati (4 carte obiettivo per punti vittoria)
	// Replicato solo al proprietario per mantenere la segretezza
	UPROPERTY(ReplicatedUsing = OnRep_SecondaryObjectives, BlueprintReadOnly, Category = "Objectives")
	TArray<FAssignedObjective> SecondaryObjectives;

	// ID del giocatore che ha eliminato questo player (-1 se ancora vivo o eliminato dal sistema)
	// Usato per obiettivi tipo "Elimina giocatore colore X"
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 EliminatedBy = -1;

	// === METODI PUBBLICI ===

	// Imposta GameManagerPlayerID (chiamato dal GameMode quando player si connette)
	UFUNCTION(BlueprintCallable, Category = "Player Info")
	void SetGameManagerPlayerID(int32 NewPlayerID);

	// Imposta colore esercito (chiamato quando player sceglie colore)
	UFUNCTION(BlueprintCallable, Category = "Player Info")
	void SetArmyColor(FLinearColor NewColor);

	// Ottieni nome user-friendly del player (es. "Player 1", "Player 2")
	UFUNCTION(BlueprintCallable, Category = "Player Info")
	FString GetDisplayName() const;

	// === HELPER METHODS PER GAME STATE ===

	// Aggiungi truppe da piazzare
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void AddTroops(int32 NumTroops);

	// Rimuovi truppe (quando piazzate)
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void RemoveTroops(int32 NumTroops);

	// Aggiungi territorio posseduto
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void AddTerritory(int32 TerritoryID);

	// Rimuovi territorio posseduto
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void RemoveTerritory(int32 TerritoryID);

	// Aggiungi carta alla mano
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void AddCard(const FTerritoryCard& Card);

	// Rimuovi carta dalla mano
	UFUNCTION(BlueprintCallable, Category = "Game State")
	void RemoveCard(const FTerritoryCard& Card);

	// Ottieni numero territori posseduti
	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetNumTerritoriesOwned() const { return OwnedTerritoryIDs.Num(); }

	// Verifica se possiede un territorio specifico
	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool OwnsTerritory(int32 TerritoryID) const { return OwnedTerritoryIDs.Contains(TerritoryID); }

	// === OBIETTIVI - QUERY METHODS ===

	// Calcola punti vittoria totali da obiettivi secondari completati
	UFUNCTION(BlueprintPure, Category = "Objectives")
	int32 GetTotalVictoryPoints() const;

	// Verifica se l'obiettivo principale è stato completato
	UFUNCTION(BlueprintPure, Category = "Objectives")
	bool HasCompletedMainObjective() const { return MainObjective.bCompleted; }

	// Ottieni numero di obiettivi secondari completati
	UFUNCTION(BlueprintPure, Category = "Objectives")
	int32 GetCompletedSecondaryCount() const;

	// Verifica se tutti gli obiettivi secondari sono stati completati
	UFUNCTION(BlueprintPure, Category = "Objectives")
	bool HasCompletedAllSecondaryObjectives() const;

	// Ottieni lista di tutti gli obiettivi (principale + secondari) per UI
	UFUNCTION(BlueprintPure, Category = "Objectives")
	TArray<FAssignedObjective> GetAllObjectives() const;

	// === EVENTI ===

	// Evento dispatcher quando gli obiettivi vengono assegnati/aggiornati
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectivesUpdatedSignature);

	UPROPERTY(BlueprintAssignable, Category = "Objectives")
	FOnObjectivesUpdatedSignature OnObjectivesUpdated;

	// === REP NOTIFIES ===

	// Chiamato quando MainObjective viene replicato
	UFUNCTION()
	void OnRep_MainObjective();

	// Chiamato quando SecondaryObjectives vengono replicati
	UFUNCTION()
	void OnRep_SecondaryObjectives();

	// === OBIETTIVI - ASSIGNMENT METHODS (chiamati dal GameManager) ===

	// Assegna obiettivo principale
	void AssignMainObjective(const FObjectiveDefinition& Objective, int32 ObjectiveIndex);

	// Assegna obiettivo secondario (fino a 4)
	void AssignSecondaryObjective(const FObjectiveDefinition& Objective, int32 ObjectiveIndex);

	// Marca obiettivo principale come completato
	void CompleteMainObjective(int32 CompletionTurn, float CompletionTime);

	// Marca obiettivo secondario come completato (per index)
	void CompleteSecondaryObjective(int32 SecondaryIndex, int32 CompletionTurn, float CompletionTime);
};

