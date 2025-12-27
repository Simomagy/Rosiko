#include "GameUIController.h"
#include "ColorSelectionWidget.h"
#include "LoadingScreenWidget.h"
#include "GameHUDWidget.h"
#include "../RosikoGameManager.h"
#include "../RosikoPlayerState.h"
#include "../RosikoGameState.h"
#include "../RosikoPlayerController.h"
#include "../../Map/MapGenerator.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerState.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoUIController, Log, All);

AGameUIController::AGameUIController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGameUIController::BeginPlay()
{
	Super::BeginPlay();

	// Determina LocalPlayerID usando RosikoPlayerState
	APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (LocalPC)
	{
		ARosikoPlayerState* PS = LocalPC->GetPlayerState<ARosikoPlayerState>();
		if (PS && PS->GameManagerPlayerID >= 0)
		{
			// PlayerState pronto con GameManagerPlayerID assegnato
			LocalPlayerID = PS->GameManagerPlayerID;

			UE_LOG(LogRosikoUIController, Warning, TEXT("GameUIController::BeginPlay() - LocalPlayerID: %d (GameManagerPlayerID from PlayerState, PlayerName: %s)"),
			       LocalPlayerID, *PS->GetPlayerName());
		}
		else
		{
			// PlayerState non ancora pronto: aspetta e riprova
			UE_LOG(LogRosikoUIController, Warning, TEXT("RosikoPlayerState not ready yet, retrying..."));
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
			{
				APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
				if (PC)
				{
					ARosikoPlayerState* RetryPS = PC->GetPlayerState<ARosikoPlayerState>();
					if (RetryPS && RetryPS->GameManagerPlayerID >= 0)
					{
						LocalPlayerID = RetryPS->GameManagerPlayerID;
						UE_LOG(LogRosikoUIController, Warning, TEXT("LocalPlayerID determined (delayed): %d"), LocalPlayerID);
					}
					else
					{
						UE_LOG(LogRosikoUIController, Error, TEXT("RosikoPlayerState still not ready after delay!"));
					}
				}
			}, 0.5f, false); // 0.5s delay per essere sicuri
		}
	}

	FindGameManager();
	FindMapGenerator();
	InitializeUI();
	BindGameManagerEvents();
	BindMapGeneratorEvents();
	SetupPlayerController();
}

void AGameUIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindGameManagerEvents();
	UnbindMapGeneratorEvents();
	Super::EndPlay(EndPlayReason);
}

void AGameUIController::FindGameManager()
{
	if (GameManager) return;

	for (TActorIterator<ARosikoGameManager> It(GetWorld()); It; ++It)
	{
		GameManager = *It;
		UE_LOG(LogRosikoUIController, Log, TEXT("Found GameManager in level"));
		break;
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("GameManager not found in level!"));
	}
}

void AGameUIController::FindMapGenerator()
{
	if (MapGenerator) return;

	for (TActorIterator<AMapGenerator> It(GetWorld()); It; ++It)
	{
		MapGenerator = *It;
		UE_LOG(LogRosikoUIController, Warning, TEXT("Found MapGenerator in level - MapSeed: %d"), MapGenerator->MapSeed);
		break;
	}

	if (!MapGenerator)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("MapGenerator not found in level!"));
	}
}

void AGameUIController::InitializeUI()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("Cannot create widgets: PlayerController not found"));
		return;
	}

	UE_LOG(LogRosikoUIController, Warning, TEXT("InitializeUI() - PlayerController: %s, IsLocalController: %s"),
	       *PC->GetName(), PC->IsLocalController() ? TEXT("TRUE") : TEXT("FALSE"));

	// Crea LoadingScreen (visibile immediatamente)
	if (LoadingScreenWidgetClass)
	{
		LoadingScreenWidget = CreateWidget<ULoadingScreenWidget>(PC, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(999); // Massima priorità

			// Bind al MapGenerator se presente
			if (MapGenerator)
			{
				LoadingScreenWidget->BindToMapGenerator(MapGenerator);
			}

			LoadingScreenWidget->ShowLoadingScreen();
			UE_LOG(LogRosikoUIController, Warning, TEXT("LoadingScreenWidget created and shown"));
		}
		else
		{
			UE_LOG(LogRosikoUIController, Error, TEXT("Failed to create LoadingScreenWidget"));
		}
	}
	else
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("LoadingScreenWidgetClass not set! Assign in Editor."));
	}

	// Crea widget selezione colore (nascosto inizialmente)
	if (ColorSelectionWidgetClass)
	{
		ColorSelectionWidget = CreateWidget<UColorSelectionWidget>(PC, ColorSelectionWidgetClass);
		if (ColorSelectionWidget)
		{
			ColorSelectionWidget->AddToViewport(100);
			ColorSelectionWidget->SetVisibility(ESlateVisibility::Hidden);
			UE_LOG(LogRosikoUIController, Log, TEXT("ColorSelectionWidget created"));
		}
		else
		{
			UE_LOG(LogRosikoUIController, Error, TEXT("Failed to create ColorSelectionWidget"));
		}
	}
	else
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("ColorSelectionWidgetClass not set"));
	}

	// TerritoryInfo verrà creato dopo generazione mappa (in OnMapGenerationComplete)
}

void AGameUIController::BindGameManagerEvents()
{
	if (!GameManager)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("Cannot bind events: GameManager is null"));
		return;
	}

	// Bind evento selezione colore
	GameManager->OnColorSelectionRequired.AddDynamic(this, &AGameUIController::OnColorSelectionRequired);

	// Bind evento cambio fase (per nascondere widget quando fase ColorSelection finisce)
	GameManager->OnPhaseChanged.AddDynamic(this, &AGameUIController::OnPhaseChanged);

	UE_LOG(LogRosikoUIController, Log, TEXT("GameManager events bound"));
}

void AGameUIController::BindMapGeneratorEvents()
{
	if (!MapGenerator)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("Cannot bind MapGenerator events: MapGenerator is null"));
		return;
	}

	MapGenerator->OnGenerationComplete.AddDynamic(this, &AGameUIController::OnMapGenerationComplete);
	UE_LOG(LogRosikoUIController, Log, TEXT("MapGenerator events bound"));
}

void AGameUIController::UnbindGameManagerEvents()
{
	if (GameManager)
	{
		GameManager->OnColorSelectionRequired.RemoveDynamic(this, &AGameUIController::OnColorSelectionRequired);
		GameManager->OnPhaseChanged.RemoveDynamic(this, &AGameUIController::OnPhaseChanged);
		UE_LOG(LogRosikoUIController, Log, TEXT("GameManager events unbound"));
	}
}

void AGameUIController::UnbindMapGeneratorEvents()
{
	if (MapGenerator)
	{
		MapGenerator->OnGenerationComplete.RemoveDynamic(this, &AGameUIController::OnMapGenerationComplete);
		UE_LOG(LogRosikoUIController, Log, TEXT("MapGenerator events unbound"));
	}
}

void AGameUIController::SetupPlayerController()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("PlayerController not found for setup"));
		return;
	}

	// Abilita input mouse
	PC->bShowMouseCursor = true;
	PC->bEnableClickEvents = true;
	PC->bEnableMouseOverEvents = true;

	UE_LOG(LogRosikoUIController, Log, TEXT("PlayerController setup complete (mouse, click, hover enabled)"));
}

void AGameUIController::OnColorSelectionRequired(int32 PlayerID, const TArray<FLinearColor>& AvailableColors)
{
	UE_LOG(LogRosikoUIController, Warning, TEXT("OnColorSelectionRequired called - PlayerID: %d, AvailableColors: %d, MapReady: %s, WidgetExists: %s"),
	       PlayerID, AvailableColors.Num(), bMapGenerationComplete ? TEXT("YES") : TEXT("NO"),
	       ColorSelectionWidget ? TEXT("YES") : TEXT("NO"));

	// Se la mappa non è ancora pronta o il widget non esiste, metti in queue
	if (!bMapGenerationComplete || !ColorSelectionWidget)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("Map not ready or widget missing, queuing color selection event"));

		FPendingColorSelection Pending;
		Pending.PlayerID = PlayerID;
		Pending.AvailableColors = AvailableColors;
		PendingColorSelectionEvents.Add(Pending);
		return;
	}

	ARosikoPlayerState* PS = GameManager ? GameManager->GetRosikoPlayerState(PlayerID) : nullptr;
	if (!GameManager || !PS)
	{
		UE_LOG(LogRosikoUIController, Error, TEXT("Invalid PlayerID: %d"), PlayerID);
		return;
	}

	// Determina se è il turno di questo client
	bool bIsMyTurn = (PlayerID == LocalPlayerID);
	FString DisplayMessage;

	if (bIsMyTurn)
	{
		DisplayMessage = FString::Printf(TEXT("YOUR TURN! Choose your army color"));
		UE_LOG(LogRosikoUIController, Warning, TEXT("Showing color selection - MY TURN (LocalPlayerID: %d)"), LocalPlayerID);
	}
	else
	{
		DisplayMessage = FString::Printf(TEXT("Waiting for %s to choose..."), *PS->GetDisplayName());
		UE_LOG(LogRosikoUIController, Warning, TEXT("Showing color selection - Waiting for %s (PlayerID: %d, LocalPlayerID: %d)"),
		       *PS->GetDisplayName(), PlayerID, LocalPlayerID);
	}

	// Mostra widget con messaggio appropriato
	ColorSelectionWidget->ShowColorSelection(PlayerID, DisplayMessage, AvailableColors);
}

void AGameUIController::OnMapGenerationComplete()
{
	UE_LOG(LogRosikoUIController, Warning, TEXT("OnMapGenerationComplete callback received (LOCAL map generation done)"));

	// Segna che la mappa è pronta
	bMapGenerationComplete = true;

	// NON chiudiamo il LoadingScreen qui! Aspettiamo che tutti i player siano pronti.
	// Aggiorniamo solo il messaggio
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->UpdateWaitingMessage(TEXT("Map generated! Waiting for other players..."));
	}

	// IMPORTANTE: Notifica il server che questo client è pronto (solo per i client, non per il server)
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && !PC->HasAuthority())
	{
		ARosikoPlayerController* RPC = Cast<ARosikoPlayerController>(PC);
		if (RPC)
		{
			UE_LOG(LogRosikoUIController, Warning, TEXT("Notifying server that client is ready..."));
			RPC->Server_NotifyClientReady();
		}
	}

	// Bind eventi GameState per ricevere aggiornamenti sui player pronti
	BindGameStateEvents();

	// Crea TerritoryInfo widget
	if (TerritoryInfoWidgetClass && PC)
	{
		TerritoryInfoWidget = CreateWidget<UUserWidget>(PC, TerritoryInfoWidgetClass);
		if (TerritoryInfoWidget)
		{
			TerritoryInfoWidget->AddToViewport(0); // Z-order basso (sotto tutto)
			UE_LOG(LogRosikoUIController, Warning, TEXT("TerritoryInfoWidget created and added to viewport"));
		}
		else
		{
			UE_LOG(LogRosikoUIController, Error, TEXT("Failed to create TerritoryInfoWidget"));
		}
	}
	else if (!TerritoryInfoWidgetClass)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("TerritoryInfoWidgetClass not set"));
	}

	// Crea GameHUD widget (nascosto inizialmente, verrà mostrato quando inizia il gioco)
	if (GameHUDWidgetClass && PC)
	{
		GameHUDWidget = CreateWidget<UGameHUDWidget>(PC, GameHUDWidgetClass);
		if (GameHUDWidget)
		{
			GameHUDWidget->AddToViewport(50); // Z-order medio (sopra territory info, sotto popup)
			GameHUDWidget->HideHUD(); // Nascosto inizialmente
			UE_LOG(LogRosikoUIController, Warning, TEXT("GameHUDWidget created and added to viewport (hidden)"));
		}
		else
		{
			UE_LOG(LogRosikoUIController, Error, TEXT("Failed to create GameHUDWidget"));
		}
	}
	else if (!GameHUDWidgetClass)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("GameHUDWidgetClass not set"));
	}

	// Verifica subito se tutti sono già pronti (potrebbe esserlo se siamo l'ultimo)
	CheckAllPlayersReady();

	// Processa eventi ColorSelection in queue (arrivati prima che mappa fosse pronta)
	if (PendingColorSelectionEvents.Num() > 0)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("Processing %d pending color selection events"), PendingColorSelectionEvents.Num());

		// Aspetta un frame per essere sicuri che tutte le repliche siano arrivate
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			for (const FPendingColorSelection& Pending : PendingColorSelectionEvents)
			{
				OnColorSelectionRequired(Pending.PlayerID, Pending.AvailableColors);
			}
			PendingColorSelectionEvents.Empty();
		}, 0.1f, false);
	}
}

void AGameUIController::OnPhaseChanged(EGamePhase OldPhase, EGamePhase NewPhase)
{
	UE_LOG(LogRosikoUIController, Warning, TEXT("OnPhaseChanged called - Old: %d, New: %d"), (int32)OldPhase, (int32)NewPhase);

	// NON nascondiamo LoadingScreen qui! Il client deve aspettare OnMapGenerationComplete LOCALE
	// Questo evento viene triggerato quando il SERVER cambia fase, ma client potrebbe ancora generare

	// Se la nuova fase NON è ColorSelection, nascondi il widget (phase-agnostic logic)
	// NOTA: OldPhase sui client è sempre Setup (non possiamo replicare storia), quindi controlliamo solo NewPhase
	if (NewPhase != EGamePhase::ColorSelection)
	{
		if (ColorSelectionWidget && ColorSelectionWidget->GetVisibility() == ESlateVisibility::Visible)
		{
			ColorSelectionWidget->HideColorSelection();
			UE_LOG(LogRosikoUIController, Warning, TEXT("Phase changed to %d, hiding ColorSelectionWidget"), (int32)NewPhase);
		}
	}

	// Se entriamo in ColorSelection, mostra il widget (gestito da OnColorSelectionRequired)
	// Se entriamo in InitialDistribution, prepara UI per piazzamento truppe
	if (NewPhase == EGamePhase::InitialDistribution)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("Entered InitialDistribution phase - ready for troop placement"));

		// Mostra GameHUD (da questo punto in poi HUD è sempre visibile)
		if (GameHUDWidget)
		{
			GameHUDWidget->ShowHUD();
			UE_LOG(LogRosikoUIController, Warning, TEXT("GameHUD shown (game started)"));
		}
	}

	// Nascondi HUD se torniamo a Setup o ColorSelection (edge case restart)
	if (NewPhase == EGamePhase::Setup || NewPhase == EGamePhase::ColorSelection)
	{
		if (GameHUDWidget)
		{
			GameHUDWidget->HideHUD();
			UE_LOG(LogRosikoUIController, Warning, TEXT("GameHUD hidden (game not started)"));
		}
	}
}

void AGameUIController::BindGameStateEvents()
{
	ARosikoGameState* GS = GetWorld()->GetGameState<ARosikoGameState>();
	if (GS)
	{
		// Sottoscrivi all'evento di cambio player pronti
		GS->OnReadyPlayersChanged.AddDynamic(this, &AGameUIController::OnReadyPlayersChanged);
		UE_LOG(LogRosikoUIController, Log, TEXT("GameState events bound (OnReadyPlayersChanged)"));
	}
	else
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("BindGameStateEvents: GameState not found, retrying..."));

		// Riprova dopo un frame
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			BindGameStateEvents();
		}, 0.1f, false);
	}
}

void AGameUIController::OnReadyPlayersChanged()
{
	UE_LOG(LogRosikoUIController, Warning, TEXT("OnReadyPlayersChanged - Checking if all players ready..."));
	CheckAllPlayersReady();
}

void AGameUIController::CheckAllPlayersReady()
{
	// Se non abbiamo finito di generare la mappa, non fare nulla
	if (!bMapGenerationComplete)
	{
		return;
	}

	ARosikoGameState* GS = GetWorld()->GetGameState<ARosikoGameState>();
	if (!GS)
	{
		UE_LOG(LogRosikoUIController, Warning, TEXT("CheckAllPlayersReady: GameState not found"));
		return;
	}

	// Verifica se tutti sono pronti
	if (GS->AreAllPlayersReady())
	{
		// Tutti pronti! Chiudi il LoadingScreen
		UE_LOG(LogRosikoUIController, Warning, TEXT("All players ready! Hiding LoadingScreen"));

		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->RemoveFromParent();
		}
	}
	else
	{
		// Aggiorna il messaggio con la lista di player non pronti
		TArray<FString> NotReadyPlayers = GS->GetNotReadyPlayerNames();

		FString WaitingMessage;
		if (NotReadyPlayers.Num() == 1)
		{
			WaitingMessage = FString::Printf(TEXT("Waiting for %s to start..."), *NotReadyPlayers[0]);
		}
		else if (NotReadyPlayers.Num() > 1)
		{
			FString PlayerList = FString::Join(NotReadyPlayers, TEXT(", "));
			WaitingMessage = FString::Printf(TEXT("Waiting for %s to start..."), *PlayerList);
		}
		else
		{
			WaitingMessage = TEXT("Waiting for players...");
		}

		UE_LOG(LogRosikoUIController, Warning, TEXT("CheckAllPlayersReady: %s (%d/%d ready)"),
		       *WaitingMessage, GS->GetNumReadyPlayers(), GS->ExpectedPlayerCount);

		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->UpdateWaitingMessage(WaitingMessage);
		}
	}
}
