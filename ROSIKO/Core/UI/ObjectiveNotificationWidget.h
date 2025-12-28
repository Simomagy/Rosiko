#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ROSIKO/Configs/ObjectivesConfig.h"
#include "Animation/WidgetAnimation.h"
#include "ObjectiveNotificationWidget.generated.h"

/**
 * Widget notifica popup per completamento obiettivi.
 * Appare temporaneamente quando un obiettivo viene completato.
 */
UCLASS()
class ROSIKO_API UObjectiveNotificationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Mostra notifica per obiettivo completato
	UFUNCTION(BlueprintCallable, Category = "Objective Notification")
	void ShowNotification(const FAssignedObjective& CompletedObjective);

protected:
	virtual void NativeConstruct() override;

	// === WIDGET BINDINGS ===

	UPROPERTY(meta = (BindWidget))
	class UBorder* NotificationBorder;

	UPROPERTY(meta = (BindWidget))
	class UImage* CompletionIcon;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HeaderText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* ObjectiveNameText;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PointsText;

	// === ANIMAZIONE ===

	// Animazione fade in/out (deve essere creata in Blueprint con nome esatto "FadeInOut")
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	UWidgetAnimation* FadeInOut;

	// === CONFIGURAZIONE ===

	// Audio di completamento
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	class USoundBase* ObjectiveCompletedSound;

	// Audio per main objective (più epico)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	class USoundBase* MainObjectiveCompletedSound;

	// Colori header
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor MainObjectiveHeaderColor = FLinearColor(1.0f, 0.84f, 0.0f); // Gold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor SecondaryObjectiveHeaderColor = FLinearColor(0.75f, 0.75f, 0.75f); // Silver

	// Durata auto-remove (deve matchare durata animazione)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float AutoRemoveDelay = 3.0f;

private:
	// Timer handle per auto-remove
	FTimerHandle RemoveTimerHandle;

	// Helper per rimuovere widget
	void RemoveNotification();
};
