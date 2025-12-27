#include "TerritoryActor.h"
#include "./UI/TerritoryInfoWidget.h"
#include "../../Troop/UI/TroopDisplayComponent.h"
#include "../../Troop/UI/TroopVisualManager.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"

// Log Category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoTerritoryActor, Log, All);

ATerritoryActor::ATerritoryActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Crea il componente mesh procedurale
	TerritoryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerritoryMesh"));
	RootComponent = TerritoryMesh;

	// Abilita collision per permettere il click
	TerritoryMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerritoryMesh->SetCollisionObjectType(ECC_WorldStatic);
	TerritoryMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TerritoryMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Per il mouse click
	TerritoryMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Crea il componente display carri (widget 3D) - LEGACY
	TroopDisplay = CreateDefaultSubobject<UTroopDisplayComponent>(TEXT("TroopDisplay"));
	TroopDisplay->SetupAttachment(RootComponent);
	TroopDisplay->SetRelativeLocation(FVector(0, 0, 300)); // Sopra il territorio (aumentato per superare mesh altezza variabile)
	TroopDisplay->SetDisplayVisible(false); // Nascosto inizialmente

	// Crea il nuovo manager visualizzazione truppe (LOD dinamico)
	TroopVisualManager = CreateDefaultSubobject<UTroopVisualManager>(TEXT("TroopVisualManager"));
	TroopVisualManager->SetupAttachment(RootComponent);
	TroopVisualManager->SetRelativeLocation(FVector(0, 0, 0)); // Centrato su territorio

	// Abilita il click su questo actor
	SetActorEnableCollision(true);
}

void ATerritoryActor::BeginPlay()
{
	Super::BeginPlay();
}

void ATerritoryActor::NotifyActorOnClicked(FKey ButtonPressed)
{
	Super::NotifyActorOnClicked(ButtonPressed);

	// Gestisci solo click sinistro
	if (ButtonPressed == EKeys::LeftMouseButton)
	{
		UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Territory %d clicked! Currently highlighted: %s"), TerritoryData.ID, bIsHighlighted ? TEXT("YES") : TEXT("NO"));

		// Se questo territorio è già selezionato, deselezionalo
		if (bIsHighlighted)
		{
			UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Deselecting territory %d"), TerritoryData.ID);
			Deselect();

			// Chiudi il widget
			UTerritoryInfoWidget* Widget = FindInfoWidget();
			if (Widget)
			{
				Widget->Hide();
			}

			return;
		}

		// Altrimenti, deseleziona tutti gli altri e seleziona questo
		UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Deselecting other territories..."));
		DeselectOtherTerritories();

		// Seleziona questo territorio
		bIsHighlighted = true;
		HighlightTerritory(true);
		UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Territory %d now selected"), TerritoryData.ID);

		// Trova il widget e aggiornalo con dati correnti dal GameManager
		UTerritoryInfoWidget* Widget = FindInfoWidget();
		if (Widget)
		{
			Widget->SetTerritoryID(TerritoryData.ID); // Usa nuovo metodo che legge dal GameManager
			Widget->Show();
		}
		else
		{
			UE_LOG(LogRosikoTerritoryActor, Warning, TEXT("TerritoryActor: TerritoryInfoWidget not found in viewport! Make sure to add it to the screen."));
		}
	}
}

void ATerritoryActor::SetTerritoryData(const FGeneratedTerritory& Data)
{
	TerritoryData = Data;
}

void ATerritoryActor::HighlightTerritory_Implementation(bool bHighlight)
{
	bIsHighlighted = bHighlight;

	// Implementazione base: modifica il colore della mesh
	// Questa può essere sovrascritto in Blueprint per effetti più complessi
	if (TerritoryMesh)
	{
		// Qui potresti cambiare il materiale, aggiungere un outline, etc.
		// Per ora lasciamo vuoto, verrà implementato in Blueprint
	}
}

void ATerritoryActor::Deselect()
{
	if (bIsHighlighted)
	{
		UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Deselecting territory %d"), TerritoryData.ID);
		bIsHighlighted = false;
		HighlightTerritory(false);
	}
}

UTerritoryInfoWidget* ATerritoryActor::FindInfoWidget()
{
	// Se abbiamo già una cache valida, usala
	if (CachedInfoWidget && CachedInfoWidget->IsValidLowLevel())
	{
		return CachedInfoWidget;
	}

	// Altrimenti cerca il widget nel viewport
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return nullptr;

	// Trova tutti i widget di tipo TerritoryInfoWidget
	TArray<UUserWidget*> FoundWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, FoundWidgets, UTerritoryInfoWidget::StaticClass(), false);

	if (FoundWidgets.Num() > 0)
	{
		CachedInfoWidget = Cast<UTerritoryInfoWidget>(FoundWidgets[0]);
		return CachedInfoWidget;
	}

	return nullptr;
}

void ATerritoryActor::DeselectOtherTerritories()
{
	// Itera tutti i TerritoryActor nel mondo e deselezionali
	int32 DeselectedCount = 0;
	for (TActorIterator<ATerritoryActor> It(GetWorld()); It; ++It)
	{
		ATerritoryActor* Territory = *It;
		if (Territory && Territory != this && Territory->IsSelected())
		{
			Territory->Deselect();
			DeselectedCount++;
		}
	}
	UE_LOG(LogRosikoTerritoryActor, Log, TEXT("Deselected %d other territories"), DeselectedCount);
}
