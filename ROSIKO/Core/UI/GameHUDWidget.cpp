#include "GameHUDWidget.h"
#include "../RosikoGameState.h"
#include "../RosikoPlayerState.h"
#include "../RosikoGameManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoHUD, Log, All);

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogRosikoHUD, Log, TEXT("GameHUDWidget NativeConstruct called"));

	// Trova GameState e PlayerState
	FindGameState();
	FindLocalPlayerState();

	// Bind eventi
	BindGameStateEvents();
	BindPlayerStateEvents();

	// Setup timer per update periodici
	if (GetWorld())
	{
		// Timer per aggiornare tempo ogni secondo
		GetWorld()->GetTimerManager().SetTimer(
			GameTimeUpdateTimer,
			this,
			&UGameHUDWidget::UpdateGameTime,
			1.0f,
			true // Loop
		);

		// Timer per aggiornare info player ogni 2 secondi
		GetWorld()->GetTimerManager().SetTimer(
			PlayersInfoUpdateTimer,
			this,
			&UGameHUDWidget::UpdatePlayersInfo,
			2.0f,
			true // Loop
		);
	}

	// Prima update immediata
	UpdateGameTime();
	UpdatePlayersInfo();
	UpdateLocalPlayerTroops();
}

void UGameHUDWidget::NativeDestruct()
{
	UE_LOG(LogRosikoHUD, Log, TEXT("GameHUDWidget NativeDestruct called"));

	// Clear timers
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(GameTimeUpdateTimer);
		GetWorld()->GetTimerManager().ClearTimer(PlayersInfoUpdateTimer);
	}

	// Unbind eventi
	UnbindGameStateEvents();
	UnbindPlayerStateEvents();

	Super::NativeDestruct();
}

void UGameHUDWidget::ShowHUD()
{
	// SelfHitTestInvisible: widget visibile ma non cattura input (trasparente ai click)
	// Permette al player di cliccare sui territori sottostanti
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UE_LOG(LogRosikoHUD, Log, TEXT("HUD shown (SelfHitTestInvisible - click-through enabled)"));
}

void UGameHUDWidget::HideHUD()
{
	SetVisibility(ESlateVisibility::Collapsed);
	UE_LOG(LogRosikoHUD, Log, TEXT("HUD hidden"));
}

void UGameHUDWidget::FindGameState()
{
	GameState = GetWorld()->GetGameState<ARosikoGameState>();

	if (GameState)
	{
		UE_LOG(LogRosikoHUD, Log, TEXT("GameState found"));
	}
	else
	{
		UE_LOG(LogRosikoHUD, Warning, TEXT("GameState not found - retrying..."));

		// Riprova dopo un piccolo delay
		FTimerHandle RetryTimer;
		GetWorld()->GetTimerManager().SetTimer(RetryTimer, [this]()
		{
			FindGameState();
			if (GameState)
			{
				BindGameStateEvents();
			}
		}, 0.5f, false);
	}
}

void UGameHUDWidget::FindLocalPlayerState()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		LocalPlayerState = PC->GetPlayerState<ARosikoPlayerState>();

		if (LocalPlayerState && LocalPlayerState->GameManagerPlayerID >= 0)
		{
			LocalPlayerID = LocalPlayerState->GameManagerPlayerID;
			UE_LOG(LogRosikoHUD, Log, TEXT("LocalPlayerState found - PlayerID: %d"), LocalPlayerID);
		}
		else
		{
			UE_LOG(LogRosikoHUD, Warning, TEXT("LocalPlayerState not ready - retrying..."));

			// Riprova dopo un piccolo delay
			FTimerHandle RetryTimer;
			GetWorld()->GetTimerManager().SetTimer(RetryTimer, [this]()
			{
				FindLocalPlayerState();
				if (LocalPlayerState)
				{
					BindPlayerStateEvents();
				}
			}, 0.5f, false);
		}
	}
}

void UGameHUDWidget::BindGameStateEvents()
{
	if (!GameState)
	{
		UE_LOG(LogRosikoHUD, Warning, TEXT("Cannot bind GameState events - GameState is null"));
		return;
	}

	// Trova GameManager per bind evento cambio fase
	for (TActorIterator<ARosikoGameManager> It(GetWorld()); It; ++It)
	{
		ARosikoGameManager* GM = *It;
		if (GM)
		{
			GM->OnPhaseChanged.AddDynamic(this, &UGameHUDWidget::HandleCurrentPhaseChanged);
			UE_LOG(LogRosikoHUD, Log, TEXT("Bound to GameManager OnPhaseChanged event"));
			break;
		}
	}

	UE_LOG(LogRosikoHUD, Log, TEXT("GameState events bound"));
}

void UGameHUDWidget::BindPlayerStateEvents()
{
	if (!LocalPlayerState)
	{
		UE_LOG(LogRosikoHUD, Warning, TEXT("Cannot bind PlayerState events - LocalPlayerState is null"));
		return;
	}

	// Il PlayerState non ha eventi per TroopsToPlace, quindi aggiungeremo un check nei timer
	UE_LOG(LogRosikoHUD, Log, TEXT("PlayerState events bound"));
}

void UGameHUDWidget::UnbindGameStateEvents()
{
	if (!GameState)
	{
		return;
	}

	// Trova GameManager per unbind evento
	for (TActorIterator<ARosikoGameManager> It(GetWorld()); It; ++It)
	{
		ARosikoGameManager* GM = *It;
		if (GM)
		{
			GM->OnPhaseChanged.RemoveDynamic(this, &UGameHUDWidget::HandleCurrentPhaseChanged);
			break;
		}
	}

	UE_LOG(LogRosikoHUD, Log, TEXT("GameState events unbound"));
}

void UGameHUDWidget::UnbindPlayerStateEvents()
{
	// Nessun evento da unbindare per ora
}

void UGameHUDWidget::HandleCurrentPhaseChanged(EGamePhase OldPhase, EGamePhase NewPhase)
{
	UE_LOG(LogRosikoHUD, Log, TEXT("Phase changed from %d to %d - updating HUD"), (int32)OldPhase, (int32)NewPhase);

	// Aggiorna tutto quando la fase cambia
	UpdateGameTime();
	UpdatePlayersInfo();
	UpdateLocalPlayerTroops();
}

void UGameHUDWidget::HandleCurrentTurnChanged()
{
	UE_LOG(LogRosikoHUD, Log, TEXT("Turn changed - updating HUD"));

	// Aggiorna info turno
	if (!GameState)
	{
		return;
	}

	// Calcola round number
	CurrentRound = CalculateCurrentRound();

	// Ottieni info player corrente
	int32 CurrentPlayerID = GameState->GetCurrentPlayerID();
	if (CurrentPlayerID >= 0)
	{
		// Cerca PlayerState del player corrente
		for (APlayerState* PS : GameState->PlayerArray)
		{
			ARosikoPlayerState* RPS = Cast<ARosikoPlayerState>(PS);
			if (RPS && RPS->GameManagerPlayerID == CurrentPlayerID)
			{
				CurrentPlayerName = RPS->GetDisplayName();
				CurrentPlayerColor = RPS->ArmyColor;
				break;
			}
		}
	}

	// Notifica Blueprint
	OnCurrentTurnChanged(CurrentPlayerName, CurrentPlayerColor, CurrentRound);

	// Aggiorna anche info player (potrebbero essere cambiate)
	UpdatePlayersInfo();
}

void UGameHUDWidget::UpdateGameTime()
{
	if (!GameState)
	{
		return;
	}

	// Formatta tempo e aggiorna
	FString NewFormattedTime = FormatGameTime(GameState->GameTimeSeconds);

	if (NewFormattedTime != FormattedGameTime)
	{
		FormattedGameTime = NewFormattedTime;
		OnGameTimeUpdated(FormattedGameTime);
	}
}

void UGameHUDWidget::UpdatePlayersInfo()
{
	if (!GameState)
	{
		return;
	}

	// Calcola round number
	int32 NewRound = CalculateCurrentRound();
	bool bRoundChanged = (NewRound != CurrentRound);
	CurrentRound = NewRound;

	TArray<FPlayerHUDInfo> NewPlayersInfo;

	// Ottieni ID del player corrente
	int32 CurrentPlayerID = GameState->GetCurrentPlayerID();
	bool bTurnChanged = false;

	// Itera su tutti i PlayerState
	for (APlayerState* PS : GameState->PlayerArray)
	{
		ARosikoPlayerState* RPS = Cast<ARosikoPlayerState>(PS);
		if (RPS && RPS->GameManagerPlayerID >= 0)
		{
			FPlayerHUDInfo Info;
			Info.PlayerName = RPS->GetDisplayName();
			Info.ArmyColor = RPS->ArmyColor;
			Info.TerritoriesOwned = RPS->GetNumTerritoriesOwned();
			Info.TotalTroops = CalculatePlayerTotalTroops(RPS->GameManagerPlayerID);
			Info.bIsCurrentTurn = (RPS->GameManagerPlayerID == CurrentPlayerID);
			Info.bIsAlive = RPS->bIsAlive;
			Info.bIsLocalPlayer = (RPS->GameManagerPlayerID == LocalPlayerID);

			NewPlayersInfo.Add(Info);

			// Se questo è il player corrente, aggiorna CurrentPlayerName e Color
			if (RPS->GameManagerPlayerID == CurrentPlayerID)
			{
				// Check se turno è cambiato
				if (CurrentPlayerName != Info.PlayerName || CurrentPlayerColor != Info.ArmyColor)
				{
					bTurnChanged = true;
				}

				CurrentPlayerName = Info.PlayerName;
				CurrentPlayerColor = Info.ArmyColor;
			}
		}
	}

	// Ordina per PlayerID (per consistenza UI)
	NewPlayersInfo.Sort([](const FPlayerHUDInfo& A, const FPlayerHUDInfo& B)
	{
		// Local player sempre per primo, poi gli altri in ordine
		if (A.bIsLocalPlayer && !B.bIsLocalPlayer) return true;
		if (!A.bIsLocalPlayer && B.bIsLocalPlayer) return false;
		return A.PlayerName < B.PlayerName;
	});

	// Aggiorna e notifica Blueprint
	PlayersInfo = NewPlayersInfo;
	OnPlayersInfoUpdated(PlayersInfo);

	// Se turno o round è cambiato, notifica anche quello
	if (bTurnChanged || bRoundChanged)
	{
		OnCurrentTurnChanged(CurrentPlayerName, CurrentPlayerColor, CurrentRound);
	}

	// Aggiorna anche truppe del player locale (potrebbe essere cambiato il suo turno)
	UpdateLocalPlayerTroops();
}

void UGameHUDWidget::UpdateLocalPlayerTroops()
{
	if (!LocalPlayerState)
	{
		return;
	}

	int32 NewTroops = LocalPlayerState->TroopsToPlace;

	if (NewTroops != LocalPlayerTroops)
	{
		LocalPlayerTroops = NewTroops;
		OnLocalPlayerTroopsChanged(LocalPlayerTroops);
	}
}

FString UGameHUDWidget::FormatGameTime(float Seconds) const
{
	int32 TotalSeconds = FMath::FloorToInt(Seconds);
	int32 Days = TotalSeconds / 86400;
	int32 Hours = (TotalSeconds % 86400) / 3600;
	int32 Minutes = (TotalSeconds % 3600) / 60;
	int32 Secs = TotalSeconds % 60;

	// Formato dinamico basato su durata
	if (Days > 0)
	{
		// DD:HH:MM:SS
		return FString::Printf(TEXT("%02d:%02d:%02d:%02d"), Days, Hours, Minutes, Secs);
	}
	else if (Hours > 0)
	{
		// HH:MM:SS
		return FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Minutes, Secs);
	}
	else
	{
		// MM:SS
		return FString::Printf(TEXT("%02d:%02d"), Minutes, Secs);
	}
}

int32 UGameHUDWidget::CalculateCurrentRound() const
{
	if (!GameState || GameState->TurnOrder.Num() == 0)
	{
		return 1;
	}

	// Round = (CurrentPlayerTurn / NumPlayers) + 1
	// Es: 3 player, turn 0-2 = round 1, turn 3-5 = round 2, etc.
	return (GameState->CurrentPlayerTurn / GameState->TurnOrder.Num()) + 1;
}

int32 UGameHUDWidget::CalculatePlayerTotalTroops(int32 PlayerID) const
{
	if (!GameState)
	{
		return 0;
	}

	int32 Total = 0;

	// Somma truppe su tutti i territori del player
	for (const FTerritoryGameState& Territory : GameState->Territories)
	{
		if (Territory.OwnerID == PlayerID)
		{
			Total += Territory.Troops;
		}
	}

	return Total;
}

