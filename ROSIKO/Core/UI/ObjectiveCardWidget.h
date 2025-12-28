#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ROSIKO/Configs/ObjectivesConfig.h"
#include "ObjectiveCardWidget.generated.h"

/**
 * Widget per visualizzare una singola carta obiettivo (principale o secondario).
 * Riutilizzabile per entrambi i tipi di obiettivi.
 */
UCLASS()
class ROSIKO_API UObjectiveCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PUBLIC API ===

	// Setta dati obiettivo e aggiorna UI
	UFUNCTION(BlueprintCallable, Category = "Objective Card")
	void SetObjectiveData(const FAssignedObjective& InObjectiveData, bool bInIsMainObjective);

	// Aggiorna UI da dati correnti (forza refresh)
	UFUNCTION(BlueprintCallable, Category = "Objective Card")
	void RefreshDisplay();

	// Getter per accesso da Blueprint
	UFUNCTION(BlueprintPure, Category = "Objective Card")
	FAssignedObjective GetObjectiveData() const { return ObjectiveData; }

	UFUNCTION(BlueprintPure, Category = "Objective Card")
	bool IsMainObjective() const { return bIsMainObjective; }

protected:
	virtual void NativeConstruct() override;

	// === WIDGET BINDINGS (BindWidget in Blueprint Designer) ===

	// Tipo obiettivo ("PRINCIPALE" / "SECONDARIO")
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TypeText;

	// Nome obiettivo
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DisplayNameText;

	// Descrizione obiettivo
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* DescriptionText;

	// Punti vittoria (solo secondari)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* VictoryPointsText;

	// Icona completamento (checkmark)
	UPROPERTY(meta = (BindWidget))
	class UImage* CompletionIcon;

	// Border/sfondo carta
	UPROPERTY(meta = (BindWidget))
	class UBorder* CardBorder;

	// Container punti vittoria (per nascondere su main)
	UPROPERTY(meta = (BindWidget))
	class UHorizontalBox* VictoryPointsBox;

	// === STYLING COLORS (editabili in Blueprint) ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor MainObjectiveColor = FLinearColor(1.0f, 0.84f, 0.0f); // Gold

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor SecondaryObjectiveColor = FLinearColor(0.75f, 0.75f, 0.75f); // Silver

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor CompletedColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.3f); // Green glow

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor NormalColor = FLinearColor(0.25f, 0.25f, 0.25f, 0.8f); // Dark gray

	// === EVENTI BLUEPRINT (implementabili) ===

	// Chiamato quando l'obiettivo viene completato (per animazioni custom)
	UFUNCTION(BlueprintImplementableEvent, Category = "Objective Card")
	void OnObjectiveCompleted();

private:
	// Dati obiettivo corrente
	UPROPERTY()
	FAssignedObjective ObjectiveData;

	// Se true, è un obiettivo principale
	UPROPERTY()
	bool bIsMainObjective = false;

	// Se true, mostra testo segreto come "???" (per obiettivi di altri player)
	UPROPERTY()
	bool bShowSecret = false;
};
