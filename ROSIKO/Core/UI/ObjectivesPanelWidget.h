#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ObjectiveCardWidget.h"
#include "ObjectivesPanelWidget.generated.h"

/**
 * Pannello principale obiettivi del giocatore.
 * Mostra 1 obiettivo principale + N obiettivi secondari (configurabile).
 */
UCLASS()
class ROSIKO_API UObjectivesPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PUBLIC API ===

	// Inizializza pannello con PlayerState del giocatore locale
	UFUNCTION(BlueprintCallable, Category = "Objectives Panel")
	void InitializePanel();

	// Aggiorna stato obiettivi da server (chiamato da timer)
	UFUNCTION(BlueprintCallable, Category = "Objectives Panel")
	void UpdateObjectivesStatus();

	// Forza refresh di tutte le carte
	UFUNCTION(BlueprintCallable, Category = "Objectives Panel")
	void RefreshAllCards();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// === WIDGET BINDINGS ===

	// Widget carta obiettivo principale
	UPROPERTY(meta = (BindWidget))
	UObjectiveCardWidget* MainObjectiveCard;

	// Container per carte secondarie (popolato dinamicamente)
	UPROPERTY(meta = (BindWidget))
	class UScrollBox* SecondaryCardsContainer;

	// Titolo sezione secondari (per nascondere se 0 secondari)
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* SecondariesTitle;

	// === CONFIGURAZIONE ===

	// Classe widget da usare per le carte secondarie
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objectives Panel")
	TSubclassOf<UObjectiveCardWidget> SecondaryCardClass;

	// Spacing tra carte secondarie (px)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objectives Panel")
	float CardSpacing = 10.0f;

	// Frequenza aggiornamento automatico (secondi, 0 = disabilitato)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objectives Panel")
	float AutoUpdateInterval = 0.5f;

	// === EVENTI ===

	// Evento dispatcher quando un obiettivo viene completato
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompletedSignature, const FAssignedObjective&, CompletedObjective);

	UPROPERTY(BlueprintAssignable, Category = "Objectives Panel")
	FOnObjectiveCompletedSignature OnObjectiveCompleted;

private:
	// Reference al PlayerState locale
	UPROPERTY()
	class ARosikoPlayerState* OwningPlayerState;

	// Array di widget carte secondarie create dinamicamente
	UPROPERTY()
	TArray<UObjectiveCardWidget*> SecondaryCardWidgets;

	// Tracking stato completamento per rilevare nuovi completamenti
	bool bWasMainCompleted = false;
	TArray<bool> PreviousSecondaryStates;

	// Timer per auto-update
	float UpdateTimer = 0.0f;

	// Flag per tracking se pannello è stato inizializzato con successo
	bool bPanelInitialized = false;

	// Helper per creare carte secondarie
	void CreateSecondaryCards();

	// Helper per verificare nuovi completamenti
	void CheckForNewCompletions();

	// Callback quando PlayerState notifica aggiornamento obiettivi
	UFUNCTION()
	void OnPlayerObjectivesUpdated();
};
