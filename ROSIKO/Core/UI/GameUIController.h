#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameUIController.generated.h"

/**
 * Controller centrale per gestione UI di gioco.
 * Si sottoscrive agli eventi del GameManager e mostra/nasconde i widget appropriati.
 */
UCLASS()
class ROSIKO_API AGameUIController : public AActor
{
	GENERATED_BODY()

public:
	AGameUIController();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// === RIFERIMENTI WIDGET ===

	// === WIDGET CLASSES (configurabili in Editor) ===

	// Classe widget per selezione colore
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UColorSelectionWidget> ColorSelectionWidgetClass;

	// Classe widget per loading screen
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class ULoadingScreenWidget> LoadingScreenWidgetClass;

	// Classe widget per territory info
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UUserWidget> TerritoryInfoWidgetClass;

	// Classe widget per game HUD
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<class UGameHUDWidget> GameHUDWidgetClass;

	// === WIDGET INSTANCES ===

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UColorSelectionWidget* ColorSelectionWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class ULoadingScreenWidget* LoadingScreenWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UUserWidget* TerritoryInfoWidget;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UGameHUDWidget* GameHUDWidget;

	// === METODI PUBBLICI ===

	// Crea e inizializza tutti i widget
	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeUI();

private:
	// Reference al GameManager
	UPROPERTY()
	class ARosikoGameManager* GameManager;

	// Reference al MapGenerator
	UPROPERTY()
	class AMapGenerator* MapGenerator;

	// PlayerID locale per questo client (0, 1, 2, etc.)
	int32 LocalPlayerID = -1;

	// Flag per tracking se mappa è pronta (per client)
	bool bMapGenerationComplete = false;

	// Queue per eventi color selection ricevuti prima che widget sia pronto
	struct FPendingColorSelection
	{
		int32 PlayerID;
		TArray<FLinearColor> AvailableColors;
	};
	TArray<FPendingColorSelection> PendingColorSelectionEvents;

	// === CALLBACK EVENTI GAMEMANAGER ===

	UFUNCTION()
	void OnColorSelectionRequired(int32 PlayerID, const TArray<FLinearColor>& AvailableColors);

	UFUNCTION()
	void OnPhaseChanged(EGamePhase OldPhase, EGamePhase NewPhase);

	// === CALLBACK EVENTI MAPGENERATOR ===

	UFUNCTION()
	void OnMapGenerationComplete();

	// === CALLBACK EVENTI GAMESTATE ===

	UFUNCTION()
	void OnReadyPlayersChanged();

	// === HELPERS ===
	void FindGameManager();
	void FindMapGenerator();
	void BindGameManagerEvents();
	void BindMapGeneratorEvents();
	void BindGameStateEvents();
	void UnbindGameManagerEvents();
	void UnbindMapGeneratorEvents();
	void SetupPlayerController(); // Abilita click/hover su PlayerController
	void CheckAllPlayersReady(); // Verifica se tutti i player sono pronti e chiude LoadingScreen
};

