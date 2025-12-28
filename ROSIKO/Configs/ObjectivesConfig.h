#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectivesConfig.generated.h"

/**
 * Tipi di condizioni per obiettivi ROS!KO.
 * Ogni tipo rappresenta una categoria di obiettivo verificabile.
 */
UENUM(BlueprintType)
enum class EObjectiveConditionType : uint8
{
	// Conquista N continenti specifici dalla lista
	ConquerContinents,

	// Conquista N territori totali (in qualsiasi posizione)
	ConquerTerritories,

	// Conquista N territori all'interno di continenti specifici
	ConquerTerritoriesInContinents,

	// Elimina il giocatore di un colore specifico
	EliminatePlayerColor,

	// Controlla N territori confinanti consecutivi
	ControlAdjacentTerritories,

	// Sopravvivi fino al turno N con almeno X territori
	SurviveUntilTurn,

	// Scambia N tris di carte territorio
	ExchangeCardSets,

	// Controlla tutti i territori di un continente (completamente)
	ControlFullContinent,

	// Custom (per logiche speciali future)
	Custom
};

/**
 * Singola condizione verificabile per un obiettivo.
 * Più condizioni possono essere combinate in AND (tutte devono essere soddisfatte).
 */
USTRUCT(BlueprintType)
struct FObjectiveCondition
{
	GENERATED_BODY()

	// Tipo di condizione da verificare
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	EObjectiveConditionType Type = EObjectiveConditionType::ConquerTerritories;

	// Lista continenti target (per ConquerContinents, ConquerTerritoriesInContinents, ControlFullContinent)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition",
	          meta = (EditCondition = "Type == EObjectiveConditionType::ConquerContinents || Type == EObjectiveConditionType::ConquerTerritoriesInContinents || Type == EObjectiveConditionType::ControlFullContinent"))
	TArray<int32> TargetContinentIDs;

	// Colori target (per EliminatePlayerColor)
	// Nota: Durante validazione, colori non presenti in partita rendono obiettivo invalido
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition",
	          meta = (EditCondition = "Type == EObjectiveConditionType::EliminatePlayerColor"))
	TArray<FLinearColor> TargetColors;

	// Numero richiesto (continenti, territori, tris, ecc.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	// Turno minimo richiesto (per SurviveUntilTurn)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition",
	          meta = (EditCondition = "Type == EObjectiveConditionType::SurviveUntilTurn", ClampMin = "1"))
	int32 RequiredTurn = 1;

	// Numero minimo territori richiesto (per SurviveUntilTurn)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition",
	          meta = (EditCondition = "Type == EObjectiveConditionType::SurviveUntilTurn", ClampMin = "1"))
	int32 MinTerritories = 1;

	// === VALIDAZIONE ===

	// Se true, questo obiettivo è valido solo con un numero minimo di giocatori
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bRequiresMinPlayers = false;

	// Numero minimo giocatori richiesto (usato se bRequiresMinPlayers = true)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation",
	          meta = (EditCondition = "bRequiresMinPlayers", ClampMin = "3", ClampMax = "10"))
	int32 MinPlayers = 3;

	// Se true, questo obiettivo è valido solo se esiste almeno un colore target in partita
	// (per obiettivi EliminatePlayerColor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
	bool bRequiresTargetColorInGame = false;
};

/**
 * Definizione completa di un obiettivo ROS!KO.
 * Contiene tutte le info necessarie per display UI e verifica completamento.
 */
USTRUCT(BlueprintType)
struct FObjectiveDefinition
{
	GENERATED_BODY()

	// Nome obiettivo (es. "Conquistatore d'Europa")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText DisplayName = FText::FromString(TEXT("Unnamed Objective"));

	// Descrizione estesa (es. "Conquista Europa e Asia")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	FText Description = FText::FromString(TEXT("No description"));

	// Lista condizioni (tutte devono essere soddisfatte in AND)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Conditions")
	TArray<FObjectiveCondition> Conditions;

	// Punti vittoria assegnati al completamento
	// 0 per obiettivi principali (vittoria immediata se completati)
	// >0 per obiettivi secondari (contribuiscono al punteggio finale)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring", meta = (ClampMin = "0"))
	int32 VictoryPoints = 0;

	// Se true, questo è un obiettivo principale (missione segreta alla Risiko)
	// Se false, è un obiettivo secondario (fornisce punti)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type")
	bool bIsMainObjective = false;

	// Se true, l'obiettivo rimane segreto agli altri giocatori fino al completamento
	// (default: true per obiettivi principali, può essere false per secondari)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
	bool bIsSecretUntilCompleted = true;

	// ID univoco per debug/logging (opzionale, può essere lasciato vuoto)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FString DebugID;
};

/**
 * Struct per obiettivo assegnato a un giocatore.
 * Contiene la definizione originale + stato di completamento.
 * Usato in ARosikoPlayerState per tracking obiettivi del giocatore.
 */
USTRUCT(BlueprintType)
struct FAssignedObjective
{
	GENERATED_BODY()

	// Definizione obiettivo assegnato
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	FObjectiveDefinition Definition;

	// Se true, questo obiettivo è stato completato
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	bool bCompleted = false;

	// Turno in cui l'obiettivo è stato completato (-1 se non ancora completato)
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	int32 CompletionTurn = -1;

	// Index dell'obiettivo nel mazzo originale (per debug/logging)
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	int32 ObjectiveIndex = -1;

	// Timestamp di completamento (GameTimeSeconds)
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float CompletionTimeSeconds = -1.0f;
};

/**
 * Configurazione obiettivi ROS!KO.
 * Data Asset contenente tutti gli obiettivi principali e secondari disponibili.
 *
 * Durante il setup, il GameManager:
 * 1. Filtra obiettivi validi in base a NumPlayers e colori attivi
 * 2. Shuffle deterministico con GameRNG
 * 3. Assegna 1 principale + 4 secondari a ogni giocatore
 */
UCLASS(BlueprintType)
class ROSIKO_API UObjectivesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// === MAZZI OBIETTIVI ===

	// Lista obiettivi principali (missioni segrete)
	// Il giocatore ne riceve 1 all'inizio della partita
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Objectives",
	          meta = (DisplayName = "Main Objectives Pool"))
	TArray<FObjectiveDefinition> MainObjectives;

	// Lista obiettivi secondari (forniscono punti vittoria)
	// Il giocatore ne riceve 4 all'inizio della partita
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Secondary Objectives",
	          meta = (DisplayName = "Secondary Objectives Pool"))
	TArray<FObjectiveDefinition> SecondaryObjectives;

	// === REGOLE ASSEGNAZIONE ===

	// Numero di obiettivi secondari da assegnare a ogni giocatore (0 = nessuno, tipicamente 4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assignment Rules", meta = (ClampMin = "0", ClampMax = "10"))
	int32 NumSecondaryObjectivesPerPlayer = 4;

	// Se true, un obiettivo principale può essere assegnato a più giocatori
	// (default: false, ogni obiettivo principale è univoco)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assignment Rules")
	bool bAllowDuplicateMainObjectives = false;

	// Se true, obiettivi secondari possono ripetersi tra giocatori
	// (default: true, pool più piccolo di obiettivi secondari)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assignment Rules")
	bool bAllowDuplicateSecondaryObjectives = true;

	// Se true, uno stesso giocatore può ricevere obiettivi secondari duplicati
	// (default: false, evita confusione)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assignment Rules")
	bool bAllowDuplicatesPerPlayer = false;

	// === HELPER METHODS ===

	// Filtra obiettivi principali validi per la configurazione di partita corrente
	// Esclude obiettivi che richiedono più giocatori di quelli attivi o colori non presenti
	UFUNCTION(BlueprintCallable, Category = "Objectives Config")
	TArray<FObjectiveDefinition> FilterValidMainObjectives(int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const;

	// Filtra obiettivi secondari validi per la configurazione di partita corrente
	UFUNCTION(BlueprintCallable, Category = "Objectives Config")
	TArray<FObjectiveDefinition> FilterValidSecondaryObjectives(int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const;

	// Valida una singola definizione obiettivo
	UFUNCTION(BlueprintPure, Category = "Objectives Config")
	bool IsObjectiveValid(const FObjectiveDefinition& Objective, int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const;

	// Verifica se una condizione è valida per la configurazione corrente
	UFUNCTION(BlueprintPure, Category = "Objectives Config")
	bool IsConditionValid(const FObjectiveCondition& Condition, int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const;

private:
	// Helper per controllare se un colore esiste nella lista con tolleranza float
	bool ContainsColor(const TArray<FLinearColor>& ColorList, const FLinearColor& ColorToFind) const;
};
