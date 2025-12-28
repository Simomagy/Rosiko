#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VictoryPointsWidget.generated.h"

/**
 * Widget per mostrare i punti vittoria totali del giocatore.
 * Sempre visibile in HUD, aggiornato automaticamente.
 */
UCLASS()
class ROSIKO_API UVictoryPointsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Aggiorna punti visualizzati
	UFUNCTION(BlueprintCallable, Category = "Victory Points")
	void UpdatePoints();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// === WIDGET BINDINGS ===

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* PointsText;

	UPROPERTY(meta = (BindWidget))
	class UImage* StarIcon;

	// === CONFIGURAZIONE ===

	// Frequenza aggiornamento (secondi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Victory Points")
	float UpdateInterval = 1.0f;

	// Colori per diversi livelli di punteggio
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor HighScoreColor = FLinearColor(1.0f, 0.84f, 0.0f); // Gold (>= 30)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor MediumScoreColor = FLinearColor(0.75f, 0.75f, 0.75f); // Silver (>= 15)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Styling")
	FLinearColor LowScoreColor = FLinearColor(1.0f, 1.0f, 1.0f); // White (< 15)

private:
	UPROPERTY()
	class ARosikoPlayerState* OwningPlayerState;

	float UpdateTimer = 0.0f;
	int32 LastDisplayedPoints = -1; // Per evitare refresh inutili
};
