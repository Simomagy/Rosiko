#include "ObjectiveNotificationWidget.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectiveNotification, Log, All);

void UObjectiveNotificationWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UObjectiveNotificationWidget::ShowNotification(const FAssignedObjective& CompletedObjective)
{
	if (!ObjectiveNameText || !HeaderText)
	{
		UE_LOG(LogObjectiveNotification, Error, TEXT("Widget components not bound!"));
		return;
	}

	// === POPOLA DATI ===

	// Nome obiettivo
	ObjectiveNameText->SetText(CompletedObjective.Definition.DisplayName);

	// Configurazione in base a tipo
	bool bIsMain = CompletedObjective.Definition.bIsMainObjective;

	if (bIsMain)
	{
		// Obiettivo principale
		HeaderText->SetText(FText::FromString(TEXT("OBIETTIVO PRINCIPALE COMPLETATO!")));
		HeaderText->SetColorAndOpacity(MainObjectiveHeaderColor);

		if (PointsText)
		{
			PointsText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		// Obiettivo secondario
		HeaderText->SetText(FText::FromString(TEXT("OBIETTIVO COMPLETATO!")));
		HeaderText->SetColorAndOpacity(SecondaryObjectiveHeaderColor);

		if (PointsText)
		{
			FString PointsStr = FString::Printf(TEXT("+%d punti"), CompletedObjective.Definition.VictoryPoints);
			PointsText->SetText(FText::FromString(PointsStr));
			PointsText->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// === PLAY ANIMAZIONE ===

	if (FadeInOut)
	{
		PlayAnimation(FadeInOut);
	}
	else
	{
		UE_LOG(LogObjectiveNotification, Warning, TEXT("FadeInOut animation not found! Create animation in Blueprint."));
	}

	// === PLAY AUDIO ===

	USoundBase* SoundToPlay = bIsMain ? MainObjectiveCompletedSound : ObjectiveCompletedSound;
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySound2D(this, SoundToPlay);
	}

	// === AUTO-REMOVE ===

	// Cancella timer precedente se esiste
	if (RemoveTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(RemoveTimerHandle);
	}

	// Setta nuovo timer
	GetWorld()->GetTimerManager().SetTimer(
		RemoveTimerHandle,
		this,
		&UObjectiveNotificationWidget::RemoveNotification,
		AutoRemoveDelay,
		false
	);

	UE_LOG(LogObjectiveNotification, Log, TEXT("Showing notification: %s (Main: %s, Points: %d)"),
	       *CompletedObjective.Definition.DisplayName.ToString(),
	       bIsMain ? TEXT("YES") : TEXT("NO"),
	       CompletedObjective.Definition.VictoryPoints);
}

void UObjectiveNotificationWidget::RemoveNotification()
{
	RemoveFromParent();
	UE_LOG(LogObjectiveNotification, Verbose, TEXT("Notification removed from viewport"));
}
