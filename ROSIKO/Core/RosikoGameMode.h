#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RosikoGameMode.generated.h"

/**
 * GameMode principale per ROSIKO.
 * Configura pawn predefinito (ARosikoCamera) per tutti i player in multiplayer.
 * Gestisce il flow di setup: generazione mappa → avvio gioco.
 * Configura GameState customizzato (ARosikoGameState).
 */
UCLASS()
class ROSIKO_API ARosikoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARosikoGameMode();

protected:
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

public:
	// Se true, avvia automaticamente generazione mappa e gioco al BeginPlay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Flow")
	bool bAutoStartGame = true;

	// Numero di player attesi prima di avviare il gioco (default: 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Flow", meta = (ClampMin = "1", ClampMax = "10"))
	int32 ExpectedPlayerCount = 3;

	// Avvia manualmente il flow di setup (alternativa a bAutoStartGame)
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartGameFlow();

	// Chiamato dai client quando hanno completato la generazione mappa e sono pronti
	void NotifyClientReady(APlayerController* ReadyClient);

private:
	// Reference al MapGenerator
	UPROPERTY()
	class AMapGenerator* MapGenerator;

	// Reference al GameManager
	UPROPERTY()
	class ARosikoGameManager* GameManager;

	// Traccia se la mappa è stata generata (sul server)
	bool bMapGenerationComplete = false;

	// Traccia se il gioco è già stato avviato
	bool bGameStarted = false;

	// Lista di PlayerController che hanno notificato di essere pronti
	UPROPERTY()
	TArray<APlayerController*> ReadyClients;

	// Callback quando mappa è generata
	UFUNCTION()
	void OnMapGenerationComplete();

	// Trova e crea reference ai componenti di gioco
	void FindGameActors();

	// Verifica se possiamo avviare il gioco (mappa pronta + tutti i player connessi)
	void TryStartGame();
};

