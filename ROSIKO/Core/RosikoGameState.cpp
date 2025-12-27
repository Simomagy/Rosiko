#include "RosikoGameState.h"
#include "RosikoPlayerState.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogRosikoGameState, Log, All);

ARosikoGameState::ARosikoGameState()
{
	// Abilita replicazione
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARosikoGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARosikoGameState, CurrentPhase);
	DOREPLIFETIME(ARosikoGameState, CurrentPlayerTurn);
	DOREPLIFETIME(ARosikoGameState, TurnOrder);
	DOREPLIFETIME(ARosikoGameState, Territories);
	DOREPLIFETIME(ARosikoGameState, AvailableColors);
	DOREPLIFETIME(ARosikoGameState, GameTimeSeconds);
	DOREPLIFETIME(ARosikoGameState, GameStartTimestamp);
	DOREPLIFETIME(ARosikoGameState, MapSeed);
	DOREPLIFETIME(ARosikoGameState, ExpectedPlayerCount);
	DOREPLIFETIME(ARosikoGameState, ReadyPlayerIDs);
}

void ARosikoGameState::OnRep_CurrentPhase()
{
	UE_LOG(LogRosikoGameState, Warning, TEXT("OnRep_CurrentPhase - NewPhase: %d"), (int32)CurrentPhase);

	// Trova il GameManager e notifica il cambio di fase
	// IMPORTANTE: Questo viene eseguito sui CLIENT quando CurrentPhase viene replicato
	for (TActorIterator<ARosikoGameManager> It(GetWorld()); It; ++It)
	{
		ARosikoGameManager* GM = *It;
		if (GM)
		{
			// Broadcast evento OnPhaseChanged del GameManager
			// NOTA: Non possiamo sapere OldPhase sui client, quindi usiamo Setup come placeholder
			GM->OnPhaseChanged.Broadcast(EGamePhase::Setup, CurrentPhase);
			UE_LOG(LogRosikoGameState, Warning, TEXT("Client: Notified GameManager of phase change to %d"), (int32)CurrentPhase);
			break;
		}
	}
}

void ARosikoGameState::OnRep_CurrentPlayerTurn()
{
	UE_LOG(LogRosikoGameState, Warning, TEXT("OnRep_CurrentPlayerTurn - NewTurn: %d"), CurrentPlayerTurn);

	// Broadcast evento per UI
	// Nota: EventDispatcher verrà aggiunto se necessario
}

void ARosikoGameState::OnRep_ReadyPlayerIDs()
{
	UE_LOG(LogRosikoGameState, Warning, TEXT("OnRep_ReadyPlayerIDs - %d/%d players ready"),
	       ReadyPlayerIDs.Num(), ExpectedPlayerCount);

	// Notifica UI che la lista di player pronti è cambiata
	OnReadyPlayersChanged.Broadcast();
}

FTerritoryGameState* ARosikoGameState::GetTerritory(int32 TerritoryID)
{
	for (FTerritoryGameState& Territory : Territories)
	{
		if (Territory.TerritoryID == TerritoryID)
		{
			return &Territory;
		}
	}

	UE_LOG(LogRosikoGameState, Warning, TEXT("GetTerritory - Territory %d not found!"), TerritoryID);
	return nullptr;
}

FTerritoryGameState ARosikoGameState::GetTerritoryByID(int32 TerritoryID, bool& bFound) const
{
	for (const FTerritoryGameState& Territory : Territories)
	{
		if (Territory.TerritoryID == TerritoryID)
		{
			bFound = true;
			return Territory;
		}
	}

	bFound = false;
	FTerritoryGameState Invalid;
	Invalid.TerritoryID = -1;
	return Invalid;
}

int32 ARosikoGameState::GetCurrentPlayerID() const
{
	if (TurnOrder.IsValidIndex(CurrentPlayerTurn))
	{
		return TurnOrder[CurrentPlayerTurn];
	}

	UE_LOG(LogRosikoGameState, Error, TEXT("GetCurrentPlayerID - Invalid CurrentPlayerTurn index: %d"), CurrentPlayerTurn);
	return -1;
}

void ARosikoGameState::StartGameTimer()
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoGameState, Warning, TEXT("StartGameTimer called on client - ignoring"));
		return;
	}

	// Salva timestamp corrente (Unix UTC in secondi)
	GameStartTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	UE_LOG(LogRosikoGameState, Warning, TEXT("Game timer started - Timestamp: %lld"), GameStartTimestamp);

	// Prima update immediata
	UpdateGameTime();
}

void ARosikoGameState::UpdateGameTime()
{
	// Solo il server aggiorna il tempo
	if (!HasAuthority())
	{
		return;
	}

	// Se il timer non è ancora partito, non calcolare nulla
	if (GameStartTimestamp == 0)
	{
		GameTimeSeconds = 0.0f;
		return;
	}

	// Calcola tempo trascorso dalla differenza tra timestamp corrente e inizio
	int64 CurrentTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	GameTimeSeconds = static_cast<float>(CurrentTimestamp - GameStartTimestamp);

	// Log ogni 60 secondi per debug
	static int32 LastLoggedMinute = -1;
	int32 CurrentMinute = FMath::FloorToInt(GameTimeSeconds / 60.0f);
	if (CurrentMinute != LastLoggedMinute && CurrentMinute > 0)
	{
		LastLoggedMinute = CurrentMinute;
		UE_LOG(LogRosikoGameState, Log, TEXT("Game time: %d minutes"), CurrentMinute);
	}
}

// === READY STATE MANAGEMENT ===

void ARosikoGameState::MarkPlayerReady(int32 PlayerID)
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoGameState, Warning, TEXT("MarkPlayerReady called on client - ignoring"));
		return;
	}

	if (!ReadyPlayerIDs.Contains(PlayerID))
	{
		ReadyPlayerIDs.Add(PlayerID);
		UE_LOG(LogRosikoGameState, Warning, TEXT("Player %d marked as ready (%d/%d ready)"),
		       PlayerID, ReadyPlayerIDs.Num(), ExpectedPlayerCount);

		// Broadcast anche sul server (OnRep non viene chiamato sul server)
		OnReadyPlayersChanged.Broadcast();
	}
}

bool ARosikoGameState::IsPlayerReady(int32 PlayerID) const
{
	return ReadyPlayerIDs.Contains(PlayerID);
}

bool ARosikoGameState::AreAllPlayersReady() const
{
	return ReadyPlayerIDs.Num() >= ExpectedPlayerCount;
}

TArray<FString> ARosikoGameState::GetNotReadyPlayerNames() const
{
	TArray<FString> NotReadyPlayers;

	// Itera su tutti i PlayerID attesi (0 to ExpectedPlayerCount-1)
	for (int32 i = 0; i < ExpectedPlayerCount; i++)
	{
		// Salta se già pronto
		if (ReadyPlayerIDs.Contains(i))
		{
			continue;
		}

		// Player non pronto - cerca il suo nome nel PlayerArray
		FString PlayerName;
		bool bFound = false;

		for (APlayerState* PS : PlayerArray)
		{
			ARosikoPlayerState* RPS = Cast<ARosikoPlayerState>(PS);
			if (RPS && RPS->GameManagerPlayerID == i)
			{
				PlayerName = RPS->GetDisplayName();
				bFound = true;
				break;
			}
		}

		// Se non trovato nel PlayerArray, usa nome generico
		if (!bFound)
		{
			PlayerName = FString::Printf(TEXT("Player %d"), i + 1);
		}

		NotReadyPlayers.Add(PlayerName);
	}

	return NotReadyPlayers;
}

