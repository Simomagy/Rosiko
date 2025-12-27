#include "TerritoryInfoWidget.h"
#include "../TerritoryActor.h"
#include "../../../Core/RosikoGameManager.h"
#include "../../MapGenerator.h"
#include "EngineUtils.h"

// Log Category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoTerritoryInfo, Log, All);

void UTerritoryInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Setup iniziale se necessario
	// Il widget parte nascosto
	SetVisibility(ESlateVisibility::Hidden);
}

FReply UTerritoryInfoWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Se clicco con il tasto sinistro sul background del widget (non su bottoni/elementi interni)
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Deseleziona tutti i territori
		UWorld* World = GetWorld();
		if (World)
		{
			for (TActorIterator<ATerritoryActor> It(World); It; ++It)
			{
				ATerritoryActor* Territory = *It;
				if (Territory && Territory->IsSelected())
				{
					Territory->Deselect();
				}
			}
		}

		// Nascondi il widget
		Hide();

		// Returna Unhandled per permettere al click di propagarsi (opzionale)
		return FReply::Handled();
	}

	// Per altri tasti, comportamento default
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UTerritoryInfoWidget::SetTerritoryData(const FGeneratedTerritory& Data)
{
	// DEPRECATED: Questo metodo usa solo dati statici dalla generazione mappa
	// Per avere Owner e Troops aggiornati, usa SetTerritoryID() invece

	// Popola tutte le proprietà
	TerritoryID = Data.ID;
	TerritoryName = Data.Name;
	Troops = Data.Troops; // ATTENZIONE: Questo è il valore iniziale, non quello corrente!
	OwnerID = Data.OwnerID; // ATTENZIONE: Questo è il valore iniziale, non quello corrente!
	bIsCapital = Data.bIsCapital;
	ContinentID = Data.ContinentID;
	ContinentBonusArmies = Data.ContinentBonusArmies;
	NeighborIDs = Data.NeighborIDs;
	TerritoryColor = Data.TerritoryColor;

	// Notifica il Blueprint di aggiornare la visualizzazione
	UpdateDisplay();
}

void UTerritoryInfoWidget::SetTerritoryID(int32 InTerritoryID)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 1. Trova GameManager per dati dinamici (Owner, Troops)
	ARosikoGameManager* GameManager = nullptr;
	for (TActorIterator<ARosikoGameManager> It(World); It; ++It)
	{
		GameManager = *It;
		break;
	}

	// 2. Trova MapGenerator per dati statici (Nome, Vicini, Continente)
	AMapGenerator* MapGenerator = nullptr;
	for (TActorIterator<AMapGenerator> It(World); It; ++It)
	{
		MapGenerator = *It;
		break;
	}

	// 3. Se GameManager non esiste ancora, usa dati statici dalla mappa
	FTerritoryGameState GameState;
	GameState.TerritoryID = InTerritoryID;
	GameState.OwnerID = -1; // Neutrale
	GameState.Troops = 0;

	if (GameManager)
	{
		// GameManager esiste, usa dati dinamici
		GameState = GameManager->GetTerritoryState(InTerritoryID);
		if (GameState.TerritoryID == -1)
		{
			UE_LOG(LogRosikoTerritoryInfo, Warning, TEXT("TerritoryInfoWidget: Invalid territory ID %d"), InTerritoryID);
			return;
		}
	}
	else
	{
		// GameManager non esiste ancora, mostra warning ma continua con dati statici
		UE_LOG(LogRosikoTerritoryInfo, Warning, TEXT("TerritoryInfoWidget: GameManager not found! Showing static data only."));
	}

	if (!MapGenerator)
	{
		UE_LOG(LogRosikoTerritoryInfo, Warning, TEXT("TerritoryInfoWidget: MapGenerator not found!"));
		return;
	}

	// 4. Ottieni dati statici dal MapGenerator
	const TArray<FGeneratedTerritory>& GeneratedTerritories = MapGenerator->GetGeneratedTerritories();
	const FGeneratedTerritory* StaticData = nullptr;
	for (const FGeneratedTerritory& Territory : GeneratedTerritories)
	{
		if (Territory.ID == InTerritoryID)
		{
			StaticData = &Territory;
			break;
		}
	}

	if (!StaticData)
	{
		UE_LOG(LogRosikoTerritoryInfo, Warning, TEXT("TerritoryInfoWidget: Territory %d not found in MapGenerator data"), InTerritoryID);
		return;
	}

	// 5. Combina dati statici + dinamici
	TerritoryID = InTerritoryID;
	TerritoryName = StaticData->Name;
	Troops = GameState.Troops; // DINAMICO - dal GameManager
	OwnerID = GameState.OwnerID; // DINAMICO - dal GameManager
	bIsCapital = StaticData->bIsCapital;
	ContinentID = StaticData->ContinentID;
	ContinentBonusArmies = StaticData->ContinentBonusArmies;
	NeighborIDs = StaticData->NeighborIDs;
	TerritoryColor = StaticData->TerritoryColor;

	// 6. Notifica il Blueprint di aggiornare la visualizzazione
	UpdateDisplay();
}

void UTerritoryInfoWidget::Show_Implementation()
{
	// Implementazione base: mostra il widget
	SetVisibility(ESlateVisibility::Visible);

	// Il Blueprint può sovrascrivere questo metodo per aggiungere animazioni
}

void UTerritoryInfoWidget::Hide_Implementation()
{
	// Implementazione base: nascondi il widget
	SetVisibility(ESlateVisibility::Hidden);

	// Il Blueprint può sovrascrivere questo metodo per aggiungere animazioni
}
