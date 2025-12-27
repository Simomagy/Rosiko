#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "RosikoCamera.generated.h"

class UInputMappingContext;
class UInputAction;

/**
 * Telecamera strategica top-down per Rosiko
 * Supporta: Pan (WASD/Arrow), Zoom (Mouse Wheel), Rotation (Q/E opzionale)
 */
UCLASS()
class ROSIKO_API ARosikoCamera : public APawn
{
	GENERATED_BODY()

public:
	ARosikoCamera();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ============ ENHANCED INPUT ============

	// Input Mapping Context da assegnare in Blueprint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	// Input Actions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveForwardAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveRightAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* ZoomAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* RotateAction;

	// ============ COMPONENTI ============

	// Root component (punto di rotazione)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USceneComponent* CameraRoot;

	// Spring Arm per gestire lo zoom
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	// Componente telecamera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	// ============ CONFIGURAZIONE ============

	// Velocità di pan (movimento laterale) in unità/secondo
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	float PanSpeed = 2000.0f;

	// Velocità di zoom (cambio distanza) per ogni tick di mouse wheel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	float ZoomSpeed = 500.0f;

	// Velocità di interpolazione zoom (quanto velocemente raggiunge target zoom)
	// Valori più alti = zoom più veloce, valori più bassi = zoom più smooth
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float ZoomInterpSpeed = 8.0f;

	// Distanza minima della camera dal centro
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	float MinZoomDistance = 500.0f;

	// Distanza massima della camera dal centro
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	float MaxZoomDistance = 5000.0f;

	// Velocità di rotazione (gradi/secondo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	float RotationSpeed = 45.0f;

	// Se true, abilita rotazione della camera con Q/E
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Movement")
	bool bEnableRotation = false;

	// Angolo della camera (pitch, es. 45° per vista isometrica)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Setup")
	float CameraPitch = -60.0f;

	// Bounds della mappa (limita il movimento della camera)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Bounds")
	FVector2D MapBoundsMin = FVector2D(-2500.0f, -2500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Bounds")
	FVector2D MapBoundsMax = FVector2D(2500.0f, 2500.0f);

	// Se true, limita la camera ai bounds della mappa
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Bounds")
	bool bClampToMapBounds = true;

	// Se true, espande i boundaries quando zoomi vicino (per permettere movimento completo)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Bounds")
	bool bDynamicBounds = true;

	// Fattore di espansione boundaries quando zoom vicino (1.0 = nessuna espansione, 2.0 = doppio)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Bounds", meta = (ClampMin = "1.0", ClampMax = "5.0"))
	float BoundsExpansionFactor = 2.5f;

	// ============ WIDGET SCALING ============

	// Se true, scala i widget delle truppe in base allo zoom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Widget Scaling")
	bool bDynamicWidgetScaling = true;

	// Scala minima widget (quando zoom massimo vicino)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Widget Scaling", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MinWidgetScale = 0.5f;

	// Scala massima widget (quando zoom massimo lontano)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Widget Scaling", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float MaxWidgetScale = 2.0f;

	// ============ FUNZIONI ============

	// Callback per movimento forward/backward (Enhanced Input)
	void MoveForward(const FInputActionValue& Value);

	// Callback per movimento right/left (Enhanced Input)
	void MoveRight(const FInputActionValue& Value);

	// Callback per zoom (Enhanced Input)
	void Zoom(const FInputActionValue& Value);

	// Callback per rotazione (Enhanced Input)
	void Rotate(const FInputActionValue& Value);

	// Resetta la camera alla posizione iniziale
	UFUNCTION(BlueprintCallable, Category = "Camera")
	void ResetCamera();

	// Ottieni scala widget corrente basata su zoom (0.5 - 2.0)
	UFUNCTION(BlueprintPure, Category = "Camera")
	float GetCurrentWidgetScale() const;

	// Ottieni boundaries correnti adattati allo zoom
	UFUNCTION(BlueprintPure, Category = "Camera")
	void GetCurrentBounds(FVector2D& OutMin, FVector2D& OutMax) const;

private:
	// Movement input buffer (from Enhanced Input)
	float ForwardInput = 0.0f;
	float RightInput = 0.0f;

	// Posizione iniziale (salvata per reset)
	FVector InitialLocation;
	FRotator InitialRotation;
	float InitialZoomDistance;

	// Zoom target (interpolato smooth nel Tick)
	float TargetZoomDistance = 2000.0f;
};

