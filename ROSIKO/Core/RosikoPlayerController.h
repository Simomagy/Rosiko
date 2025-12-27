#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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

private:
	// Cached reference al GameManager
	UPROPERTY()
	class ARosikoGameManager* GameManager;

	// Trova GameManager nel livello
	void FindGameManager();
};

