#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "../../Map/MapDataStructs.h"
#include "TerritoryActor.generated.h"

class UTerritoryInfoWidget;

UCLASS()
class ROSIKO_API ATerritoryActor : public AActor
{
	GENERATED_BODY()

public:
	ATerritoryActor();

protected:
	virtual void BeginPlay() override;

	// Gestione click
	virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

public:
	// Mesh procedurale del territorio
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Territory")
	UProceduralMeshComponent* TerritoryMesh;

	// Display carri armati (widget 3D sopra territorio) - LEGACY, mantieni per retrocompatibilità
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Territory")
	class UTroopDisplayComponent* TroopDisplay;

	// Manager visualizzazione truppe (LOD dinamico: mesh 3D quando zoom in, widget quando zoom out)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Territory")
	class UTroopVisualManager* TroopVisualManager;

	// Dati del territorio (geometria + logica di gioco)
	UPROPERTY(BlueprintReadOnly, Category = "Territory")
	FGeneratedTerritory TerritoryData;

	// Called to initialize the territory visual
	UFUNCTION(BlueprintImplementableEvent, Category = "Territory")
	void InitializeMesh(const FGeneratedTerritory& Data);

	// Imposta i dati del territorio (chiamato da MapGenerator)
	UFUNCTION(BlueprintCallable, Category = "Territory")
	void SetTerritoryData(const FGeneratedTerritory& Data);

	// Restituisce i dati del territorio
	UFUNCTION(BlueprintPure, Category = "Territory")
	FGeneratedTerritory GetTerritoryData() const { return TerritoryData; }

	// Evidenzia visivamente il territorio (chiamato quando viene selezionato)
	UFUNCTION(BlueprintNativeEvent, Category = "Territory")
	void HighlightTerritory(bool bHighlight);

	// Deseleziona questo territorio (chiamato esternamente o da click su altro territorio)
	UFUNCTION(BlueprintCallable, Category = "Territory")
	void Deselect();

	// Restituisce se questo territorio è attualmente selezionato
	UFUNCTION(BlueprintPure, Category = "Territory")
	bool IsSelected() const { return bIsHighlighted; }

private:
	// Flag per tracciare se il territorio è evidenziato
	bool bIsHighlighted = false;

	// Trova il widget delle info nel viewport (cached per performance)
	UTerritoryInfoWidget* FindInfoWidget();

	// Cache del widget (invalidato se null)
	UPROPERTY()
	UTerritoryInfoWidget* CachedInfoWidget;

	// Deseleziona tutti gli altri territori nel mondo
	void DeselectOtherTerritories();
};
