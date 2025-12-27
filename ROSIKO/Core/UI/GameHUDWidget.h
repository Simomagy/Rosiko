#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameHUDWidget.generated.h"

/**
 * Struct per informazioni player da mostrare nella HUD.
 */
USTRUCT(BlueprintType)
struct FPlayerHUDInfo
{
	GENERATED_BODY()

	// Nome del player (es. "Player 1", "Player 2")
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	FString PlayerName;

	// Colore esercito del player
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	FLinearColor ArmyColor = FLinearColor::White;

	// Numero di territori posseduti
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	int32 TerritoriesOwned = 0;

	// Totale truppe sui territori del player
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	int32 TotalTroops = 0;

	// Se true, è il turno di questo player
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	bool bIsCurrentTurn = false;

	// Se true, il player è ancora vivo
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	bool bIsAlive = true;

	// Se true, questo è il player locale (per highlight UI)
	UPROPERTY(BlueprintReadOnly, Category = "Player Info")
	bool bIsLocalPlayer = false;
};

/**
 * Widget HUD principale per il gioco.
 * Mostra tempo di gioco, turno corrente, truppe disponibili e info player.
 * La grafica viene fatta in Blueprint, questa classe espone solo dati/logica.
 */
UCLASS()
class ROSIKO_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// === PROPRIETÀ ESPOSTE A BLUEPRINT ===

	// Tempo di gioco formattato (MM:SS, HH:MM:SS o DD:HH:MM:SS)
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString FormattedGameTime = TEXT("00:00");

	// Numero round corrente (1-based)
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 CurrentRound = 1;

	// Nome del player attivo
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString CurrentPlayerName = TEXT("");

	// Colore del player attivo
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FLinearColor CurrentPlayerColor = FLinearColor::White;

	// Truppe disponibili per il player locale
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 LocalPlayerTroops = 0;

	// Array di informazioni su tutti i player
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TArray<FPlayerHUDInfo> PlayersInfo;

	// === EVENTI BLUEPRINT (per aggiornare la UI) ===

	// Chiamato quando il tempo di gioco cambia
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnGameTimeUpdated(const FString& NewFormattedTime);

	// Chiamato quando il turno cambia
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnCurrentTurnChanged(const FString& PlayerName, FLinearColor PlayerColor, int32 RoundNumber);

	// Chiamato quando le truppe del player locale cambiano
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnLocalPlayerTroopsChanged(int32 NewTroops);

	// Chiamato quando info player vengono aggiornate
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnPlayersInfoUpdated(const TArray<FPlayerHUDInfo>& NewPlayersInfo);

	// === METODI PUBBLICI ===

	// Mostra HUD
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowHUD();

	// Nascondi HUD
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideHUD();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// === RIFERIMENTI ===

	UPROPERTY()
	class ARosikoGameState* GameState;

	UPROPERTY()
	class ARosikoPlayerState* LocalPlayerState;

	// ID del player locale
	int32 LocalPlayerID = -1;

	// Timer handles per update periodici
	FTimerHandle GameTimeUpdateTimer;
	FTimerHandle PlayersInfoUpdateTimer;

	// === BINDING EVENTI ===

	void BindGameStateEvents();
	void BindPlayerStateEvents();
	void UnbindGameStateEvents();
	void UnbindPlayerStateEvents();

	// === CALLBACKS REPLICAZIONE ===

	UFUNCTION()
	void HandleCurrentPhaseChanged(EGamePhase OldPhase, EGamePhase NewPhase);

	UFUNCTION()
	void HandleCurrentTurnChanged();

	// === UPDATE METHODS ===

	// Aggiorna tempo di gioco (chiamato da timer ogni secondo)
	void UpdateGameTime();

	// Aggiorna info su tutti i player (chiamato da timer ogni 2 secondi)
	void UpdatePlayersInfo();

	// Aggiorna truppe del player locale
	void UpdateLocalPlayerTroops();

	// === HELPER METHODS ===

	// Formatta secondi in formato dinamico (MM:SS → HH:MM:SS → DD:HH:MM:SS)
	FString FormatGameTime(float Seconds) const;

	// Calcola numero round corrente basato su CurrentPlayerTurn e TurnOrder
	int32 CalculateCurrentRound() const;

	// Calcola truppe totali di un player sommando truppe su tutti i suoi territori
	int32 CalculatePlayerTotalTroops(int32 PlayerID) const;

	// Trova GameState nel world
	void FindGameState();

	// Trova PlayerState locale
	void FindLocalPlayerState();
};

