#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TroopWidget.generated.h"

/**
 * Widget UMG base per visualizzare numero carri armati.
 * La grafica viene fatta in Blueprint (WBP_TroopDisplay).
 * Questa classe C++ espone solo i dati.
 */
UCLASS()
class ROSIKO_API UTroopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PROPRIETÀ ESPOSTE A BLUEPRINT ===

	UPROPERTY(BlueprintReadOnly, Category = "Display")
	int32 TroopCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Display")
	FLinearColor OwnerColor = FLinearColor::White;

	// === EVENTI BLUEPRINT ===

	// Chiamato quando cambia il numero (implementa in Blueprint per animazioni)
	UFUNCTION(BlueprintImplementableEvent, Category = "Display")
	void OnTroopCountChanged(int32 OldCount, int32 NewCount);

	// Chiamato quando cambia il colore
	UFUNCTION(BlueprintImplementableEvent, Category = "Display")
	void OnOwnerColorChanged(FLinearColor NewColor);

	// === METODI PUBBLICI ===

	// Aggiorna widget con nuovi valori
	UFUNCTION(BlueprintCallable, Category = "Display")
	void UpdateDisplay(int32 NewCount, FLinearColor NewColor);

protected:
	virtual void NativeConstruct() override;

private:
	// Valori precedenti (per detect changes)
	int32 PreviousTroopCount = -1;
	FLinearColor PreviousOwnerColor = FLinearColor::Black;
};

