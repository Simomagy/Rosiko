#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "TroopDisplayComponent.generated.h"

/**
 * Componente per visualizzare numero carri armati sopra un territorio.
 * Usa un Widget 3D (UWidgetComponent) con widget UMG custom.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ROSIKO_API UTroopDisplayComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UTroopDisplayComponent();

	// Imposta numero carri visualizzati
	UFUNCTION(BlueprintCallable, Category = "Troops")
	void SetTroopCount(int32 Count);

	// Imposta colore giocatore proprietario
	UFUNCTION(BlueprintCallable, Category = "Troops")
	void SetOwnerColor(FLinearColor Color);

	// Mostra/nascondi (es. territorio neutrale all'inizio)
	UFUNCTION(BlueprintCallable, Category = "Troops")
	void SetDisplayVisible(bool bShouldBeVisible);

	// Aggiorna entrambi (count + color) in una sola chiamata
	UFUNCTION(BlueprintCallable, Category = "Troops")
	void UpdateDisplay(int32 Count, FLinearColor Color);

	// Aggiorna scala widget in base allo zoom camera (chiamato ogni frame se necessario)
	UFUNCTION(BlueprintCallable, Category = "Troops")
	void UpdateScaleFromCamera();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Troops")
	int32 TroopCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Troops")
	FLinearColor OwnerColor = FLinearColor::White;

	// Reference al widget UMG (cached per performance)
	UPROPERTY()
	class UTroopWidget* CachedWidget;

	// Reference alla camera (cached per performance)
	UPROPERTY()
	class ARosikoCamera* CachedCamera;

	// Scala base del widget (prima di dynamic scaling)
	FVector BaseDrawScale = FVector(1.0f, 1.0f, 1.0f);

private:
	// Inizializza widget reference
	void InitializeWidget();

	// Inizializza camera reference
	void InitializeCamera();

	// Timer per rate limiting camera initialization (evita spam ogni frame)
	float CameraInitRetryTimer = 0.0f;

	// Intervallo retry in secondi (evita spam)
	static constexpr float CameraInitRetryInterval = 2.0f;

	// Flag per loggare warning solo una volta
	bool bCameraWarningLogged = false;
};

