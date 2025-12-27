#include "RosikoPlayerState.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogRosikoPlayerState, Log, All);

ARosikoPlayerState::ARosikoPlayerState()
{
	// Abilita replicazione (PlayerState è già replicato di default, ma confermiamo)
	bReplicates = true;
	bAlwaysRelevant = true; // Sempre visibile a tutti i client
}

void ARosikoPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replica GameManagerPlayerID a tutti i client
	DOREPLIFETIME(ARosikoPlayerState, GameManagerPlayerID);

	// Replica colore esercito a tutti i client
	DOREPLIFETIME(ARosikoPlayerState, ArmyColor);

	// Replica flag selezione colore
	DOREPLIFETIME(ARosikoPlayerState, bHasSelectedColor);

	// Replica stato di gioco per-player
	DOREPLIFETIME(ARosikoPlayerState, TroopsToPlace);
	DOREPLIFETIME(ARosikoPlayerState, OwnedTerritoryIDs);
	DOREPLIFETIME(ARosikoPlayerState, Hand);
	DOREPLIFETIME(ARosikoPlayerState, CardExchangeCount);
	DOREPLIFETIME(ARosikoPlayerState, bIsAlive);
	DOREPLIFETIME(ARosikoPlayerState, bIsAI);
}

void ARosikoPlayerState::SetGameManagerPlayerID(int32 NewPlayerID)
{
	if (HasAuthority()) // Solo server può modificare
	{
		GameManagerPlayerID = NewPlayerID;
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player '%s' assigned GameManagerPlayerID: %d"),
		       *GetPlayerName(), NewPlayerID);
	}
}

void ARosikoPlayerState::SetArmyColor(FLinearColor NewColor)
{
	if (HasAuthority()) // Solo server può modificare
	{
		ArmyColor = NewColor;
		bHasSelectedColor = true;
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player '%s' (ID: %d) selected color RGB(%.2f, %.2f, %.2f)"),
		       *GetPlayerName(), GameManagerPlayerID, NewColor.R, NewColor.G, NewColor.B);
	}
}

FString ARosikoPlayerState::GetDisplayName() const
{
	if (GameManagerPlayerID >= 0)
	{
		return FString::Printf(TEXT("Player %d"), GameManagerPlayerID + 1); // +1 per display (Player 1, 2, 3...)
	}
	return TEXT("Unknown Player");
}

// === HELPER METHODS PER GAME STATE ===

void ARosikoPlayerState::AddTroops(int32 NumTroops)
{
	if (HasAuthority())
	{
		TroopsToPlace += NumTroops;
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Added %d troops (Total: %d)"),
		       GameManagerPlayerID, NumTroops, TroopsToPlace);
	}
}

void ARosikoPlayerState::RemoveTroops(int32 NumTroops)
{
	if (HasAuthority())
	{
		TroopsToPlace = FMath::Max(0, TroopsToPlace - NumTroops);
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Removed %d troops (Remaining: %d)"),
		       GameManagerPlayerID, NumTroops, TroopsToPlace);
	}
}

void ARosikoPlayerState::AddTerritory(int32 TerritoryID)
{
	if (HasAuthority())
	{
		if (!OwnedTerritoryIDs.Contains(TerritoryID))
		{
			OwnedTerritoryIDs.Add(TerritoryID);
			UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Added territory %d (Total: %d)"),
			       GameManagerPlayerID, TerritoryID, OwnedTerritoryIDs.Num());
		}
	}
}

void ARosikoPlayerState::RemoveTerritory(int32 TerritoryID)
{
	if (HasAuthority())
	{
		OwnedTerritoryIDs.Remove(TerritoryID);
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Removed territory %d (Remaining: %d)"),
		       GameManagerPlayerID, TerritoryID, OwnedTerritoryIDs.Num());

		// Se non ha più territori, è eliminato
		if (OwnedTerritoryIDs.Num() == 0 && bIsAlive)
		{
			bIsAlive = false;
			UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - ELIMINATED (no territories left)"),
			       GameManagerPlayerID);
		}
	}
}

void ARosikoPlayerState::AddCard(const FTerritoryCard& Card)
{
	if (HasAuthority())
	{
		Hand.Add(Card);
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Added card for territory %d (Hand size: %d)"),
		       GameManagerPlayerID, Card.TerritoryID, Hand.Num());
	}
}

void ARosikoPlayerState::RemoveCard(const FTerritoryCard& Card)
{
	if (HasAuthority())
	{
		Hand.Remove(Card);
		UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Removed card for territory %d (Hand size: %d)"),
		       GameManagerPlayerID, Card.TerritoryID, Hand.Num());
	}
}
