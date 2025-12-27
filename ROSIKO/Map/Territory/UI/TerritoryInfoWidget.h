#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../../../Map/MapDataStructs.h"
#include "TerritoryInfoWidget.generated.h"

/**
 * Widget UMG per mostrare le informazioni di un territorio.
 * Questa è la classe base C++, il layout grafico va fatto in Blueprint.
 */
UCLASS()
class ROSIKO_API UTerritoryInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PROPRIETÀ ESPOSTE A BLUEPRINT ===

	// ID del territorio
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	int32 TerritoryID;

	// Nome del territorio
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	FString TerritoryName;

	// Numero di truppe
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	int32 Troops;

	// ID del proprietario (-1 = neutrale)
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	int32 OwnerID;

	// Se è una capitale
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	bool bIsCapital;

	// ID del continente
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	int32 ContinentID;

	// Bonus armata del continente
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	int32 ContinentBonusArmies;

	// Lista degli ID dei territori confinanti
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	TArray<int32> NeighborIDs;

	// Colore del territorio (per debug/visualizzazione)
	UPROPERTY(BlueprintReadOnly, Category = "Territory Info")
	FLinearColor TerritoryColor;

	// === METODI ===

	// Imposta i dati del territorio e aggiorna il widget (DEPRECATED - usa SetTerritoryID)
	UFUNCTION(BlueprintCallable, Category = "Territory Info")
	void SetTerritoryData(const FGeneratedTerritory& Data);

	// Aggiorna widget con dati correnti dal GameManager (NUOVO - consigliato)
	UFUNCTION(BlueprintCallable, Category = "Territory Info")
	void SetTerritoryID(int32 InTerritoryID);

	// Mostra il widget con animazione (implementabile in Blueprint)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Territory Info")
	void Show();

	// Nascondi il widget con animazione (implementabile in Blueprint)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Territory Info")
	void Hide();

	// Aggiorna i valori visualizzati (chiamato automaticamente da SetTerritoryData)
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory Info")
	void UpdateDisplay();

protected:
	// Override per fare setup iniziale
	virtual void NativeConstruct() override;

	// Override per catturare click sul background del widget
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
