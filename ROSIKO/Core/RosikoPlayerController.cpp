#include "RosikoPlayerController.h"
#include "RosikoGameManager.h"
#include "RosikoPlayerState.h"
#include "RosikoGameMode.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/UserWidget.h"
#include "UI/ObjectivesPanelWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogRosikoPlayerController, Log, All);

ARosikoPlayerController::ARosikoPlayerController()
{
	// Abilita replicazione
	bReplicates = true;
}

void ARosikoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Setup Enhanced Input Mapping Context (solo su client locale)
	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (GameMappingContext)
			{
				Subsystem->AddMappingContext(GameMappingContext, GameMappingPriority);
				UE_LOG(LogRosikoPlayerController, Log, TEXT("Added Game Mapping Context (IMC_Game) with priority %d"), GameMappingPriority);
			}
			else
			{
				UE_LOG(LogRosikoPlayerController, Warning, TEXT("GameMappingContext is null! Assign IMC_Game in Blueprint Class Defaults."));
			}
		}
	}
}

void ARosikoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Setup Enhanced Input bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Bind Toggle Objectives Action
		if (ToggleObjectivesAction)
		{
			EnhancedInputComponent->BindAction(ToggleObjectivesAction, ETriggerEvent::Triggered, this, &ARosikoPlayerController::ToggleObjectivesPanel);
			UE_LOG(LogRosikoPlayerController, Log, TEXT("Bound ToggleObjectivesAction (IA_ToggleObjectives)"));
		}
		else
		{
			UE_LOG(LogRosikoPlayerController, Warning, TEXT("ToggleObjectivesAction is null! Assign IA_ToggleObjectives in Blueprint Class Defaults."));
		}
	}
	else
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("Enhanced Input Component not found! Make sure Enhanced Input Plugin is enabled."));
	}
}

void ARosikoPlayerController::FindGameManager()
{
	if (GameManager)
	{
		return; // Già trovato
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("FindGameManager: World is null"));
		return;
	}

	// Cerca ARosikoGameManager nel livello
	for (TActorIterator<ARosikoGameManager> It(World); It; ++It)
	{
		GameManager = *It;
		UE_LOG(LogRosikoPlayerController, Log, TEXT("Found GameManager in level"));
		break;
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoPlayerController, Warning, TEXT("GameManager not found in level!"));
	}
}

bool ARosikoPlayerController::Server_SelectPlayerColor_Validate(int32 PlayerID, FLinearColor ChosenColor)
{
	// VALIDAZIONE MINIMA: Solo per prevenire crash, non per gameplay logic
	// La validazione di gameplay (turno, PlayerID, ecc.) avviene in _Implementation

	// Accetta sempre (validazione soft in Implementation per non disconnettere client)
	return true;
}

void ARosikoPlayerController::Server_SelectPlayerColor_Implementation(int32 PlayerID, FLinearColor ChosenColor)
{
	// Questo viene eseguito SUL SERVER
	UE_LOG(LogRosikoPlayerController, Warning, TEXT("Server_SelectPlayerColor_Implementation - PlayerID: %d"), PlayerID);

	// === VALIDAZIONE SECURITY: Il client può chiamare SOLO per il proprio PlayerID ===
	ARosikoPlayerState* PS = GetPlayerState<ARosikoPlayerState>();
	if (!PS)
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("Server_SelectPlayerColor - PlayerState is null, ignoring request"));
		return;
	}

	// Verifica che PlayerID corrisponda al GameManagerPlayerID del chiamante
	if (PS->GameManagerPlayerID != PlayerID)
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("Server_SelectPlayerColor - CHEATING ATTEMPT BLOCKED! Client (PlayerID %d) tried to select color for PlayerID %d"),
		       PS->GameManagerPlayerID, PlayerID);
		// Non disconnettiamo, semplicemente ignoriamo la richiesta
		return;
	}

	// Trova GameManager se non già fatto
	if (!GameManager)
	{
		FindGameManager();
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("Cannot select color: GameManager not found"));
		return;
	}

	// Validazione OK: Inoltra la chiamata al GameManager (che farà validazione dettagliata: turno, colore disponibile, ecc.)
	GameManager->SelectPlayerColor_Direct(PlayerID, ChosenColor);
}

bool ARosikoPlayerController::Server_PlaceTroops_Validate(int32 PlayerID, int32 TerritoryID, int32 Amount)
{
	// Validazione base
	return Amount > 0 && PlayerID >= 0 && TerritoryID >= 0;
}

void ARosikoPlayerController::Server_PlaceTroops_Implementation(int32 PlayerID, int32 TerritoryID, int32 Amount)
{
	// Questo viene eseguito SUL SERVER
	UE_LOG(LogRosikoPlayerController, Log, TEXT("Server_PlaceTroops_Implementation - PlayerID: %d, Territory: %d, Amount: %d"),
	       PlayerID, TerritoryID, Amount);

	// Trova GameManager se non già fatto
	if (!GameManager)
	{
		FindGameManager();
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("Cannot place troops: GameManager not found"));
		return;
	}

	// Inoltra la chiamata al GameManager
	GameManager->PlaceTroops(PlayerID, TerritoryID, Amount);
}

void ARosikoPlayerController::Server_NotifyClientReady_Implementation()
{
	UE_LOG(LogRosikoPlayerController, Warning, TEXT("Server_NotifyClientReady - Client is ready"));

	// Trova il GameMode e notifica che questo client è pronto
	ARosikoGameMode* GameMode = GetWorld()->GetAuthGameMode<ARosikoGameMode>();
	if (GameMode)
	{
		GameMode->NotifyClientReady(this);
	}
	else
	{
		UE_LOG(LogRosikoPlayerController, Error, TEXT("GameMode not found!"));
	}
}

// === UI COMMANDS ===

void ARosikoPlayerController::ToggleObjectivesPanel(const FInputActionValue& Value)
{
	// Questa funzione viene chiamata quando il player preme Tab (o il tasto configurato)
	UE_LOG(LogRosikoPlayerController, Log, TEXT("ToggleObjectivesPanel called"));

	// Cerca il widget HUD per chiamare il toggle
	// NOTA: Questa è un'implementazione base. Potresti voler cachare il reference al HUD
	UUserWidget* HUDWidget = nullptr;

	// Cerca widget HUD tra i widget attivi
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		// TODO: Implementa logica per trovare GameHUD e chiamare toggle
		// Per ora logghiamo che l'input è stato ricevuto
		UE_LOG(LogRosikoPlayerController, Warning, TEXT("Toggle objectives input received - implement HUD widget toggle logic"));

		// Esempio di come implementare:
		// 1. Cache reference a GameHUDWidget in BeginPlay o quando viene creato
		// 2. Chiama GameHUDWidget->ToggleObjectivesPanel()
		// 3. O usa Event Dispatcher per notificare l'HUD
	}
}
