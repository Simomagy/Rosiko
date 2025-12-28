#include "VictoryPointsWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "../RosikoPlayerState.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogVictoryPoints, Log, All);

void UVictoryPointsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Ottieni PlayerState
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		OwningPlayerState = PC->GetPlayerState<ARosikoPlayerState>();
	}

	// Update iniziale
	UpdatePoints();
}

void UVictoryPointsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-update periodico
	UpdateTimer += InDeltaTime;
	if (UpdateTimer >= UpdateInterval)
	{
		UpdateTimer = 0.0f;
		UpdatePoints();
	}
}

void UVictoryPointsWidget::UpdatePoints()
{
	if (!OwningPlayerState || !PointsText)
	{
		return;
	}

	// Calcola punti totali
	int32 TotalPoints = OwningPlayerState->GetTotalVictoryPoints();

	// Evita refresh se non cambiati
	if (TotalPoints == LastDisplayedPoints)
	{
		return;
	}

	LastDisplayedPoints = TotalPoints;

	// Setta testo
	FString PointsStr = FString::Printf(TEXT("Punti: %d"), TotalPoints);
	PointsText->SetText(FText::FromString(PointsStr));

	// Setta colore in base a punteggio
	FLinearColor TargetColor;
	if (TotalPoints >= 30)
	{
		TargetColor = HighScoreColor; // Gold
	}
	else if (TotalPoints >= 15)
	{
		TargetColor = MediumScoreColor; // Silver
	}
	else
	{
		TargetColor = LowScoreColor; // White
	}

	PointsText->SetColorAndOpacity(TargetColor);

	UE_LOG(LogVictoryPoints, Verbose, TEXT("Updated victory points display: %d"), TotalPoints);
}
