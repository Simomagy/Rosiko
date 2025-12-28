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
	DOREPLIFETIME(ARosikoPlayerState, EliminatedBy);

	// Replica obiettivi SOLO al proprietario (per segretezza)
	DOREPLIFETIME_CONDITION(ARosikoPlayerState, MainObjective, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARosikoPlayerState, SecondaryObjectives, COND_OwnerOnly);
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
			// Nota: EliminatedBy deve essere settato dal GameManager durante la conquista
			// qui mettiamo -1 come fallback (eliminato dal sistema)
			if (EliminatedBy < 0)
			{
				EliminatedBy = -1;
			}
			UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - ELIMINATED (no territories left, eliminated by Player %d)"),
			       GameManagerPlayerID, EliminatedBy);
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

// === OBIETTIVI - QUERY METHODS ===

int32 ARosikoPlayerState::GetTotalVictoryPoints() const
{
	int32 TotalPoints = 0;

	// Conta punti da obiettivi secondari completati
	for (const FAssignedObjective& Objective : SecondaryObjectives)
	{
		if (Objective.bCompleted)
		{
			TotalPoints += Objective.Definition.VictoryPoints;
		}
	}

	return TotalPoints;
}

int32 ARosikoPlayerState::GetCompletedSecondaryCount() const
{
	int32 Count = 0;
	for (const FAssignedObjective& Objective : SecondaryObjectives)
	{
		if (Objective.bCompleted)
		{
			Count++;
		}
	}
	return Count;
}

// === REP NOTIFIES ===

void ARosikoPlayerState::OnRep_MainObjective()
{
	// Chiamato sul client quando MainObjective viene replicato dal server
	UE_LOG(LogRosikoPlayerState, Warning, TEXT("🔔 OnRep_MainObjective: Objective %d assigned!"), MainObjective.ObjectiveIndex);

	// Broadcast evento per notificare UI
	OnObjectivesUpdated.Broadcast();
}

void ARosikoPlayerState::OnRep_SecondaryObjectives()
{
	// Chiamato sul client quando SecondaryObjectives vengono replicati dal server
	UE_LOG(LogRosikoPlayerState, Warning, TEXT("🔔 OnRep_SecondaryObjectives: %d objectives assigned!"), SecondaryObjectives.Num());

	// Broadcast evento per notificare UI
	OnObjectivesUpdated.Broadcast();
}

bool ARosikoPlayerState::HasCompletedAllSecondaryObjectives() const
{
	if (SecondaryObjectives.Num() == 0)
	{
		return false; // Nessun obiettivo assegnato
	}

	for (const FAssignedObjective& Objective : SecondaryObjectives)
	{
		if (!Objective.bCompleted)
		{
			return false;
		}
	}
	return true;
}

TArray<FAssignedObjective> ARosikoPlayerState::GetAllObjectives() const
{
	TArray<FAssignedObjective> AllObjectives;

	// Aggiungi obiettivo principale (se assegnato)
	if (MainObjective.ObjectiveIndex >= 0)
	{
		AllObjectives.Add(MainObjective);
	}

	// Aggiungi obiettivi secondari
	AllObjectives.Append(SecondaryObjectives);

	return AllObjectives;
}

// === OBIETTIVI - ASSIGNMENT METHODS ===

void ARosikoPlayerState::AssignMainObjective(const FObjectiveDefinition& Objective, int32 ObjectiveIndex)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoPlayerState, Error, TEXT("AssignMainObjective called on client - must be called on server!"));
		return;
	}

	MainObjective.Definition = Objective;
	MainObjective.ObjectiveIndex = ObjectiveIndex;
	MainObjective.bCompleted = false;
	MainObjective.CompletionTurn = -1;
	MainObjective.CompletionTimeSeconds = -1.0f;

	UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Assigned main objective: '%s' (Index: %d)"),
	       GameManagerPlayerID, *Objective.DisplayName.ToString(), ObjectiveIndex);
}

void ARosikoPlayerState::AssignSecondaryObjective(const FObjectiveDefinition& Objective, int32 ObjectiveIndex)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoPlayerState, Error, TEXT("AssignSecondaryObjective called on client - must be called on server!"));
		return;
	}

	FAssignedObjective NewObjective;
	NewObjective.Definition = Objective;
	NewObjective.ObjectiveIndex = ObjectiveIndex;
	NewObjective.bCompleted = false;
	NewObjective.CompletionTurn = -1;
	NewObjective.CompletionTimeSeconds = -1.0f;

	SecondaryObjectives.Add(NewObjective);

	UE_LOG(LogRosikoPlayerState, Log, TEXT("Player %d - Assigned secondary objective %d: '%s' (Index: %d, %d points)"),
	       GameManagerPlayerID, SecondaryObjectives.Num(), *Objective.DisplayName.ToString(),
	       ObjectiveIndex, Objective.VictoryPoints);
}

void ARosikoPlayerState::CompleteMainObjective(int32 CompletionTurn, float CompletionTime)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoPlayerState, Error, TEXT("CompleteMainObjective called on client - must be called on server!"));
		return;
	}

	if (MainObjective.bCompleted)
	{
		UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - Main objective already completed!"), GameManagerPlayerID);
		return;
	}

	MainObjective.bCompleted = true;
	MainObjective.CompletionTurn = CompletionTurn;
	MainObjective.CompletionTimeSeconds = CompletionTime;

	UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - COMPLETED MAIN OBJECTIVE: '%s' (Turn: %d)"),
	       GameManagerPlayerID, *MainObjective.Definition.DisplayName.ToString(), CompletionTurn);
}

void ARosikoPlayerState::CompleteSecondaryObjective(int32 SecondaryIndex, int32 CompletionTurn, float CompletionTime)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoPlayerState, Error, TEXT("CompleteSecondaryObjective called on client - must be called on server!"));
		return;
	}

	if (!SecondaryObjectives.IsValidIndex(SecondaryIndex))
	{
		UE_LOG(LogRosikoPlayerState, Error, TEXT("Player %d - Invalid secondary objective index: %d"),
		       GameManagerPlayerID, SecondaryIndex);
		return;
	}

	FAssignedObjective& Objective = SecondaryObjectives[SecondaryIndex];

	if (Objective.bCompleted)
	{
		UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - Secondary objective %d already completed!"),
		       GameManagerPlayerID, SecondaryIndex);
		return;
	}

	Objective.bCompleted = true;
	Objective.CompletionTurn = CompletionTurn;
	Objective.CompletionTimeSeconds = CompletionTime;

	UE_LOG(LogRosikoPlayerState, Warning, TEXT("Player %d - COMPLETED SECONDARY OBJECTIVE: '%s' (+%d points, Turn: %d)"),
	       GameManagerPlayerID, *Objective.Definition.DisplayName.ToString(),
	       Objective.Definition.VictoryPoints, CompletionTurn);
}
