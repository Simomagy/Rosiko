#include "LoadingScreenWidget.h"
#include "../../Map/MapGenerator.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoLoadingScreen, Log, All);

void ULoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Nascosto di default
	SetVisibility(ESlateVisibility::Collapsed);
}

void ULoadingScreenWidget::NativeDestruct()
{
	// Unbind eventi se necessario
	if (BoundMapGenerator)
	{
		BoundMapGenerator->OnGenerationProgress.RemoveDynamic(this, &ULoadingScreenWidget::HandleGenerationProgress);
		BoundMapGenerator->OnGenerationComplete.RemoveDynamic(this, &ULoadingScreenWidget::HandleGenerationComplete);
		BoundMapGenerator = nullptr;
	}

	Super::NativeDestruct();
}

void ULoadingScreenWidget::UpdateProgress(float NewProgress, const FString& NewStatusText)
{
	Progress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	StatusText = NewStatusText;

	// Notifica Blueprint
	OnProgressChanged(Progress);
	OnStatusTextChanged(StatusText);
}

void ULoadingScreenWidget::ShowLoadingScreen()
{
	SetVisibility(ESlateVisibility::Visible);
	Progress = 0.0f;
	StatusText = TEXT("Initializing...");
}

void ULoadingScreenWidget::HideLoadingScreen()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void ULoadingScreenWidget::BindToMapGenerator(AMapGenerator* MapGenerator)
{
	if (!MapGenerator)
	{
		UE_LOG(LogRosikoLoadingScreen, Warning, TEXT("LoadingScreenWidget: Cannot bind to null MapGenerator"));
		return;
	}

	// Unbind previous
	if (BoundMapGenerator)
	{
		BoundMapGenerator->OnGenerationProgress.RemoveDynamic(this, &ULoadingScreenWidget::HandleGenerationProgress);
		BoundMapGenerator->OnGenerationComplete.RemoveDynamic(this, &ULoadingScreenWidget::HandleGenerationComplete);
	}

	// Bind new
	BoundMapGenerator = MapGenerator;
	BoundMapGenerator->OnGenerationProgress.AddDynamic(this, &ULoadingScreenWidget::HandleGenerationProgress);
	BoundMapGenerator->OnGenerationComplete.AddDynamic(this, &ULoadingScreenWidget::HandleGenerationComplete);

	UE_LOG(LogRosikoLoadingScreen, Log, TEXT("LoadingScreenWidget: Bound to MapGenerator"));
}

void ULoadingScreenWidget::HandleGenerationProgress(float InProgress, FString InStatusText)
{
	UpdateProgress(InProgress, InStatusText);
}

void ULoadingScreenWidget::HandleGenerationComplete()
{
	// Update to 100% - NON nascondiamo qui!
	// Il GameUIController gestirà la chiusura quando tutti i player sono pronti
	UpdateProgress(1.0f, TEXT("Map generated! Waiting for other players..."));

	UE_LOG(LogRosikoLoadingScreen, Log, TEXT("LoadingScreenWidget: Map generation complete, waiting for all players..."));
}

void ULoadingScreenWidget::UpdateWaitingMessage(const FString& Message)
{
	StatusText = Message;
	OnStatusTextChanged(StatusText);

	UE_LOG(LogRosikoLoadingScreen, Log, TEXT("LoadingScreenWidget: %s"), *Message);
}
