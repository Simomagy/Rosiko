#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TroopVisualManager.generated.h"

/**
 * Modalità di visualizzazione truppe
 */
UENUM(BlueprintType)
enum class ETroopDisplayMode : uint8
{
	MeshMode,    // Mesh 3D individuali (zoom in)
	WidgetMode   // Widget UI con numero (zoom out)
};

/**
 * Manager per visualizzazione truppe su territorio.
 * Gestisce transizione automatica tra mesh 3D (zoom in) e widget 2D (zoom out).
 *
 * RESPONSABILITA':
 * - Pooling di mesh per truppe individuali
 * - Layout random scatter sul territorio
 * - Detection distanza camera con threshold
 * - Fade smooth tra modalità
 * - Binding a GameState per replicazione
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ROSIKO_API UTroopVisualManager : public USceneComponent
{
	GENERATED_BODY()

public:
	UTroopVisualManager();

	// Componente instanced mesh (creato runtime in BeginPlay per corretta gerarchia)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Troop Visual")
	class UInstancedStaticMeshComponent* TroopInstancedMesh = nullptr;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// === CONFIGURAZIONE ===

	// Mesh da usare per le truppe (lascia vuoto per usare placeholder engine)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Mesh")
	class UStaticMesh* TroopMeshAsset = nullptr;

	// Threshold zoom per passare a mesh (SpringArm TargetArmLength)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Thresholds")
	float ZoomInThreshold = 4000.0f; // < 4000 = mostra mesh

	// Threshold zoom per passare a widget
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Thresholds")
	float ZoomOutThreshold = 5000.0f; // > 5000 = mostra widget

	// Intervallo polling distanza camera (secondi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Performance")
	float DistanceCheckInterval = 0.2f;

	// Durata fade in/out (secondi)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Transition")
	float FadeDuration = 0.3f;

	// Scala base mesh truppe
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Mesh")
	float BaseTroopScale = 50.0f;

	// Se true, scala mesh basata su zoom camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Mesh")
	bool bDynamicScaling = true;

	// Range scala dinamica (min quando vicino, max quando lontano)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Mesh", meta = (EditCondition = "bDynamicScaling"))
	float MinMeshScale = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Mesh", meta = (EditCondition = "bDynamicScaling"))
	float MaxMeshScale = 1.2f;

	// Raggio scatter random per posizionamento mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Layout")
	float ScatterRadius = 300.0f;

	// Altezza mesh sopra territorio
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Troop Visual|Layout")
	float MeshHeightOffset = 50.0f;

	// === METODI PUBBLICI ===

	// Aggiorna visualizzazione con nuovi dati (chiamato da GameManager/replicazione)
	UFUNCTION(BlueprintCallable, Category = "Troop Visual")
	void UpdateTroopDisplay(int32 TroopCount, FLinearColor OwnerColor);

	// Ottieni modalità corrente
	UFUNCTION(BlueprintPure, Category = "Troop Visual")
	ETroopDisplayMode GetCurrentMode() const { return CurrentMode; }

	// Forza refresh immediato (utile per debug)
	UFUNCTION(BlueprintCallable, Category = "Troop Visual")
	void ForceRefresh();

private:
	// === STATO INTERNO ===

	// Modalità corrente
	ETroopDisplayMode CurrentMode = ETroopDisplayMode::WidgetMode;

	// Modalità target (per fade smooth)
	ETroopDisplayMode TargetMode = ETroopDisplayMode::WidgetMode;

	// Dati correnti
	int32 CurrentTroopCount = 0;
	FLinearColor CurrentOwnerColor = FLinearColor::White;

	// Flag per tracciare se dati sono cambiati
	bool bDataChanged = false;

	// Progresso fade (0.0 = completamente in vecchia modalità, 1.0 = completamente in nuova)
	float FadeProgress = 1.0f;

	// Timer handles
	FTimerHandle DistanceCheckTimer;

	// Reference componenti
	UPROPERTY()
	class UTroopDisplayComponent* WidgetComponent;

	UPROPERTY()
	class ARosikoCamera* CachedCamera;

	// Cached SpringArm (evita FindComponentByClass ogni frame)
	UPROPERTY()
	class USpringArmComponent* CachedSpringArm;

	// Material instance dinamico per colorare mesh
	UPROPERTY()
	UMaterialInstanceDynamic* TroopMaterialInstance;

	// Ultimo zoom distance usato per scale update (evita ricalcoli inutili)
	float LastScaleUpdateZoom = -1.0f;

	// Threshold per considerare significativo un cambio zoom (evita micro-update)
	static constexpr float ZoomChangeThreshold = 50.0f;

	// Cache posizioni istanze (per aggiornamenti parziali)
	TArray<FTransform> InstanceTransforms;

	// === METODI INTERNI ===

	// Inizializzazione
	void CreateInstancedMeshComponent();
	void InitializeComponents();
	void InitializeCamera();
	void CreateBaseMaterial();

	// Detection e transizione
	void CheckCameraDistance();
	void TransitionToMode(ETroopDisplayMode NewMode);
	void UpdateFadeTransition(float DeltaTime);

	// Mesh management (Instanced)
	void UpdateMeshDisplay();
	void UpdateInstanceTransforms();
	void UpdateInstanceColors();
	void ClearAllInstances();

	// Widget management
	void UpdateWidgetDisplay();

	// Helpers
	float GetCurrentZoomDistance() const;
	float GetDynamicMeshScale() const;
	FVector GetRandomScatterPosition() const;

	// Flags
	bool bComponentsInitialized = false;
	bool bCameraWarningLogged = false;
};
