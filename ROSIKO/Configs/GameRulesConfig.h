#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameRulesConfig.generated.h"

USTRUCT(BlueprintType)
struct FPlayerDistributionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2", ClampMax = "10"))
	int32 NumPlayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 InitialTroops = 40; // Carri Armati iniziali

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 ApproxTerritoriesPerPlayer = 12; // Indicativo per bilanciamento
};

UENUM(BlueprintType)
enum class EInitialDistributionMode : uint8
{
	// Territori assegnati random, poi giocatori piazzano carri a turno
	RandomTerritories_TurnBasedPlacement,

	// Giocatori scelgono territori a turno (draft), poi piazzano carri
	DraftTerritories_TurnBasedPlacement,

	// Completamente random (AI distribuzione automatica)
	FullyRandom,

	// Custom (per future modalità)
	Custom
};

UENUM(BlueprintType)
enum class ETerritoryCardType : uint8
{
	Infantry,  // Fante (23 carte)
	Cavalry,   // Cavallo (24 carte)
	Artillery, // Cannone (24 carte)
	Jolly      // Jolly (3 carte)
};

USTRUCT(BlueprintType)
struct FTerritoryCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TerritoryID = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETerritoryCardType CardType = ETerritoryCardType::Infantry;

	// Operatore di confronto per TArray::Remove()
	bool operator==(const FTerritoryCard& Other) const
	{
		return TerritoryID == Other.TerritoryID && CardType == Other.CardType;
	}
};

/**
 * Configurazione regole di gioco ROSIKO.
 * Separato da MapGenerationConfig per permettere stesso seed mappa con regole diverse.
 */
UCLASS(BlueprintType)
class ROSIKO_API UGameRulesConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// === SETUP MAPPA ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Setup")
	int32 TotalTerritories = 70;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Setup", meta = (ClampMin = "3", ClampMax = "10"))
	int32 MaxPlayers = 10;

	// === DISTRIBUZIONE INIZIALE (REGOLE UFFICIALI ROSIKO) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Setup")
	EInitialDistributionMode DistributionMode = EInitialDistributionMode::RandomTerritories_TurnBasedPlacement;

	// Tabella ufficiale carri armati per numero giocatori (3-10)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Setup",
	          meta = (DisplayName = "Official Distribution Table"))
	TArray<FPlayerDistributionRule> PlayerDistributionRules = {
		{ 3, 55, 23 },  // 3 giocatori: 55 carri, ~23 territori/giocatore
		{ 4, 50, 17 },  // 4 giocatori: 50 carri, ~17 territori/giocatore
		{ 5, 45, 14 },  // 5 giocatori: 45 carri, ~14 territori/giocatore
		{ 6, 40, 11 },  // 6 giocatori: 40 carri, ~11 territori/giocatore
		{ 7, 35, 10 },  // 7 giocatori: 35 carri, ~10 territori/giocatore
		{ 8, 30, 9 },   // 8 giocatori: 30 carri, ~9 territori/giocatore
		{ 9, 25, 8 },   // 9 giocatori: 25 carri, ~8 territori/giocatore
		{ 10, 20, 7 }   // 10 giocatori: 20 carri, ~7 territori/giocatore
	};

	// Se true, ignora tabella sopra e usa valore custom (per modalità custom)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Setup|Custom")
	bool bUseCustomTroopCount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial Setup|Custom",
	          meta = (EditCondition = "bUseCustomTroopCount", ClampMin = "1"))
	int32 CustomTroopsPerPlayer = 50;

	// === CARTE TERRITORIO (70 totali) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Cards")
	int32 NumInfantryCards = 23; // Fanti

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Cards")
	int32 NumCavalryCards = 24; // Cavalli

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Cards")
	int32 NumArtilleryCards = 24; // Cannoni

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Cards")
	int32 NumJollyCards = 3; // Jolly

	// Bonus per tris di carte (progressivo standard)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Territory Cards")
	TArray<int32> CardExchangeBonuses = { 4, 6, 8, 10, 12, 15, 20, 25, 30, 35, 40, 45, 50 };

	// === RINFORZI ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reinforcements")
	int32 MinReinforcementsPerTurn = 3; // Minimo anche con pochi territori

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reinforcements")
	int32 TerritoriesPerReinforcement = 3; // 1 carro ogni 3 territori

	// Bonus continenti (override opzionale, altrimenti usa valori da MapGenerationConfig)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reinforcements|Continents")
	TMap<int32, int32> ContinentBonusOverride; // ContinentID → Bonus armata

	// === COMBATTIMENTO ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MaxAttackDice = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MaxDefenseDice = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bAttackerWinsTies = false; // ROSIKO classico: difensore vince pareggi

	// Minimo carri che devono rimanere nel territorio attaccante
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 MinTroopsRemaining = 1;

	// === WIN CONDITIONS ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Victory")
	bool bEnableSecretObjectives = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Victory")
	bool bEnableWorldDomination = true; // Conquista tutti i territori

	// === HELPERS ===

	// Ottieni numero carri per X giocatori dalla tabella
	UFUNCTION(BlueprintPure, Category = "Game Rules")
	int32 GetTroopsForPlayerCount(int32 NumPlayers) const;

	// Valida numero giocatori (3-10)
	UFUNCTION(BlueprintPure, Category = "Game Rules")
	bool IsValidPlayerCount(int32 NumPlayers) const;

	// Ottieni bonus carte per N-esimo scambio
	UFUNCTION(BlueprintPure, Category = "Game Rules")
	int32 GetCardExchangeBonus(int32 ExchangeCount) const;
};

