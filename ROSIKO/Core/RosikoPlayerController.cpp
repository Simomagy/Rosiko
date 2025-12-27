#include "RosikoPlayerController.h"
#include "RosikoGameManager.h"
#include "RosikoPlayerState.h"
#include "RosikoGameMode.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogRosikoPlayerController, Log, All);

ARosikoPlayerController::ARosikoPlayerController()
{
	// Abilita replicazione
	bReplicates = true;
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
