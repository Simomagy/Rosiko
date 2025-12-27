#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LoadingScreenWidget.generated.h"

/**
 * Widget UMG per loading screen durante generazione mappa.
 * La grafica viene fatta in Blueprint, questa classe espone solo dati/logica.
 */
UCLASS()
class ROSIKO_API ULoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PROPRIETÀ ESPOSTE A BLUEPRINT ===

	// Progresso corrente (0.0 - 1.0)
	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	float Progress = 0.0f;

	// Testo stato corrente ("Initializing...", "Building geometry...", etc.)
	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	FString StatusText = TEXT("Loading...");

	// Se true, mostra spinner infinito (per operazioni senza progresso noto)
	UPROPERTY(BlueprintReadOnly, Category = "Loading")
	bool bShowIndeterminateSpinner = false;

	// === METODI ===

	// Aggiorna progresso e stato
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void UpdateProgress(float NewProgress, const FString& NewStatusText);

	// Mostra loading screen
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void ShowLoadingScreen();

	// Nascondi loading screen (chiamato quando generazione completa)
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void HideLoadingScreen();

	// Bind automatico a MapGenerator per ascoltare eventi
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void BindToMapGenerator(class AMapGenerator* MapGenerator);

	// Aggiorna messaggio di attesa player (es. "Waiting for Player 2, Player 3...")
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void UpdateWaitingMessage(const FString& Message);

	// === EVENTI BLUEPRINT ===

	// Chiamato quando progresso cambia (per animazioni/aggiornamenti custom)
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnProgressChanged(float NewProgress);

	// Chiamato quando testo cambia
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void OnStatusTextChanged(const FString& NewText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// Callbacks per eventi MapGenerator
	UFUNCTION()
	void HandleGenerationProgress(float InProgress, FString InStatusText);

	UFUNCTION()
	void HandleGenerationComplete();

	// Reference al MapGenerator (per unbind eventi)
	UPROPERTY()
	class AMapGenerator* BoundMapGenerator;
};

