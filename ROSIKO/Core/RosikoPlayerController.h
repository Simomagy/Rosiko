#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "RosikoPlayerController.generated.h"

/**
 * PlayerController custom per ROSIKO.
 * Gestisce input, comandi client, e Server RPC per comunicare con GameManager.
 */
UCLASS()
class ROSIKO_API ARosikoPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARosikoPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

public:
	// === INPUT MAPPING CONTEXT (Enhanced Input) ===

	// Input Mapping Context per controlli di gioco (Tab per obiettivi, etc.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	class UInputMappingContext* GameMappingContext;

	// Priority per Game Mapping Context (default 0)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	int32 GameMappingPriority = 0;

	// Input Action per toggle pannello obiettivi
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Input")
	class UInputAction* ToggleObjectivesAction;

	// === SERVER RPC: GAME COMMANDS ===
	// Questi RPC permettono ai client di inviare comandi al server

	// Client richiede di selezionare un colore esercito
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Game Commands")
	void Server_SelectPlayerColor(int32 PlayerID, FLinearColor ChosenColor);

	// Client richiede di piazzare truppe su un territorio
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Game Commands")
	void Server_PlaceTroops(int32 PlayerID, int32 TerritoryID, int32 Amount);

	// Client notifica il server che ha completato la generazione mappa ed è pronto
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Game Commands")
	void Server_NotifyClientReady();

	// === UI COMMANDS (Local) ===

	// Toggle pannello obiettivi (chiamato da Input Action)
	void ToggleObjectivesPanel(const FInputActionValue& Value);

private:
	// Cached reference al GameManager
	UPROPERTY()
	class ARosikoGameManager* GameManager;

	// Trova GameManager nel livello
	void FindGameManager();
};

