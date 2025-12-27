#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ColorSelectionWidget.generated.h"

/**
 * Widget per selezione colore esercito durante early game setup.
 * Mostra palette di colori disponibili e permette al player corrente di sceglierne uno.
 */
UCLASS()
class ROSIKO_API UColorSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === CONFIGURAZIONE UI ===

	// Dimensione dei quadrati colore nella palette
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Config")
	float ColorButtonSize = 80.0f;

	// Padding tra i bottoni colore
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Config")
	float ColorButtonSpacing = 10.0f;

	// === STATO ===

	// ID del player che deve scegliere
	UPROPERTY(BlueprintReadOnly, Category = "State")
	int32 CurrentPlayerID = -1;

	// Nome del player corrente
	UPROPERTY(BlueprintReadOnly, Category = "State")
	FString CurrentPlayerName;

	// Colori disponibili per selezione
	UPROPERTY(BlueprintReadOnly, Category = "State")
	TArray<FLinearColor> AvailableColors;

	// === STATO UI PER OVERLAY ===

	// TRUE se è il turno del player locale (può scegliere ora)
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsMyTurn = false;

	// TRUE se il player locale ha già selezionato il suo colore
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bHasAlreadySelected = false;

	// === METODI PUBBLICI ===

	// Mostra widget per selezione colore
	UFUNCTION(BlueprintCallable, Category = "Color Selection")
	void ShowColorSelection(int32 PlayerID, const FString& PlayerName, const TArray<FLinearColor>& Colors);

	// Nasconde widget
	UFUNCTION(BlueprintCallable, Category = "Color Selection")
	void HideColorSelection();

	// Chiamato quando player seleziona un colore (da Blueprint/UI)
	UFUNCTION(BlueprintCallable, Category = "Color Selection")
	void OnColorSelected(FLinearColor SelectedColor);

	// === EVENTI BLUEPRINT (implementabili in BP per customizzazione UI) ===

	// Chiamato quando si deve aggiornare la visualizzazione
	UFUNCTION(BlueprintImplementableEvent, Category = "Color Selection")
	void OnRefreshDisplay();

	// Chiamato quando selezione colore ha successo
	UFUNCTION(BlueprintImplementableEvent, Category = "Color Selection")
	void OnSelectionSuccess();

	// Chiamato quando selezione colore fallisce (es. colore già preso)
	UFUNCTION(BlueprintImplementableEvent, Category = "Color Selection")
	void OnSelectionFailed(const FString& ErrorMessage);

protected:
	virtual void NativeConstruct() override;

private:
	// Reference al GameManager
	UPROPERTY()
	class ARosikoGameManager* GameManager;

	// Trova GameManager nel livello
	void FindGameManager();
};

