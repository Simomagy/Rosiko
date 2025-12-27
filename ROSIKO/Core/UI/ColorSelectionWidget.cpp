#include "ColorSelectionWidget.h"
#include "../RosikoGameManager.h"
#include "../RosikoPlayerController.h"
#include "../RosikoPlayerState.h"
#include "EngineUtils.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoColorSelectionWidget, Log, All);

void UColorSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FindGameManager();
}

void UColorSelectionWidget::FindGameManager()
{
	if (GameManager)
	{
		return; // Già trovato
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRosikoColorSelectionWidget, Error, TEXT("Cannot find GameManager: World is null"));
		return;
	}

	// Cerca ARosikoGameManager nel livello
	// IMPORTANTE: In multiplayer, trova l'istanza replicata (non quella locale)
	for (TActorIterator<ARosikoGameManager> It(World); It; ++It)
	{
		ARosikoGameManager* GM = *It;

		UE_LOG(LogRosikoColorSelectionWidget, Warning, TEXT("Found GameManager - HasAuthority: %s, NetRole: %d, RemoteRole: %d"),
		       GM->HasAuthority() ? TEXT("TRUE") : TEXT("FALSE"),
		       (int32)GM->GetLocalRole(),
		       (int32)GM->GetRemoteRole());

		// Usa il primo GameManager trovato (dovrebbe essere quello replicato)
		GameManager = GM;
		break;
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoColorSelectionWidget, Warning, TEXT("GameManager not found in level!"));
	}
}

void UColorSelectionWidget::ShowColorSelection(int32 PlayerID, const FString& PlayerName, const TArray<FLinearColor>& Colors)
{
	CurrentPlayerID = PlayerID;
	CurrentPlayerName = PlayerName;
	AvailableColors = Colors;

	// Determina se è il turno del player locale
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		ARosikoPlayerState* PS = PC->GetPlayerState<ARosikoPlayerState>();
		if (PS)
		{
			int32 LocalPlayerID = PS->GameManagerPlayerID;

			// È il mio turno se il PlayerID corrente corrisponde al mio
			bIsMyTurn = (PlayerID == LocalPlayerID);

			// Ho già selezionato se il flag nel PlayerState è true
			bHasAlreadySelected = PS->bHasSelectedColor;

			UE_LOG(LogRosikoColorSelectionWidget, Warning, TEXT("ShowColorSelection - LocalPlayerID: %d, CurrentPlayerID: %d, IsMyTurn: %s, HasAlreadySelected: %s"),
			       LocalPlayerID, PlayerID, bIsMyTurn ? TEXT("TRUE") : TEXT("FALSE"), bHasAlreadySelected ? TEXT("TRUE") : TEXT("FALSE"));
		}
	}

	// Rendi widget visibile
	SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogRosikoColorSelectionWidget, Log, TEXT("Showing color selection for %s (ID: %d), %d colors available"),
	       *PlayerName, PlayerID, Colors.Num());

	// Notifica Blueprint per aggiornare UI
	OnRefreshDisplay();
}

void UColorSelectionWidget::HideColorSelection()
{
	SetVisibility(ESlateVisibility::Hidden);

	CurrentPlayerID = -1;
	CurrentPlayerName.Empty();
	AvailableColors.Empty();
	bIsMyTurn = false;
	bHasAlreadySelected = false;

	UE_LOG(LogRosikoColorSelectionWidget, Log, TEXT("Color selection hidden"));
}

void UColorSelectionWidget::OnColorSelected(FLinearColor SelectedColor)
{
	if (!GameManager)
	{
		FindGameManager();
		if (!GameManager)
		{
			UE_LOG(LogRosikoColorSelectionWidget, Error, TEXT("Cannot select color: GameManager not found"));
			OnSelectionFailed(TEXT("GameManager not found"));
			return;
		}
	}

	// Ottieni PlayerController locale
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogRosikoColorSelectionWidget, Error, TEXT("Cannot select color: PlayerController not found"));
		OnSelectionFailed(TEXT("PlayerController non trovato"));
		return;
	}

	// Cast a RosikoPlayerController
	ARosikoPlayerController* RPC = Cast<ARosikoPlayerController>(PC);
	if (!RPC)
	{
		UE_LOG(LogRosikoColorSelectionWidget, Error, TEXT("Cannot select color: PlayerController is not ARosikoPlayerController"));
		OnSelectionFailed(TEXT("PlayerController non valido"));
		return;
	}

	// Ottieni PlayerState per determinare il nostro GameManagerPlayerID
	ARosikoPlayerState* PS = PC->GetPlayerState<ARosikoPlayerState>();
	if (!PS)
	{
		UE_LOG(LogRosikoColorSelectionWidget, Error, TEXT("Cannot select color: PlayerState not found"));
		OnSelectionFailed(TEXT("PlayerState non trovato"));
		return;
	}

	int32 MyPlayerID = PS->GameManagerPlayerID;

	UE_LOG(LogRosikoColorSelectionWidget, Warning, TEXT("Player %d (%s) selected color RGB(%.2f, %.2f, %.2f)"),
	       MyPlayerID, *PS->GetPlayerName(), SelectedColor.R, SelectedColor.G, SelectedColor.B);

	// Invia comando al server tramite PlayerController RPC con il NOSTRO PlayerID
	// Il server verificherà che stiamo chiamando per noi stessi e che sia il nostro turno
	RPC->Server_SelectPlayerColor(MyPlayerID, SelectedColor);

	// Feedback immediato all'utente (il widget verrà aggiornato quando il server conferma)
	OnSelectionSuccess();

	// NOTA: Il widget verrà aggiornato automaticamente quando il server conferma
	// e il GameManager notifica tutti i client via Multicast
}

