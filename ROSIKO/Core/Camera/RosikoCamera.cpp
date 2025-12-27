#include "RosikoCamera.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

ARosikoCamera::ARosikoCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	// Crea il root component (punto di pivot per rotazione)
	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	RootComponent = CameraRoot;

	// Crea lo Spring Arm (gestisce la distanza/zoom)
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CameraRoot);
	SpringArm->TargetArmLength = 2000.0f; // Distanza iniziale
	SpringArm->bDoCollisionTest = false; // Disabilita collision per evitare problemi
	SpringArm->bEnableCameraLag = true; // Smooth movement
	SpringArm->CameraLagSpeed = 3.0f;

	// Crea la Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);

	// NOTA: AutoPossessPlayer non è necessario quando ARosikoCamera è spawnato
	// automaticamente dal GameMode come DefaultPawnClass.
	// Il GameMode gestisce automaticamente il possesso per ogni PlayerController.
}

void ARosikoCamera::BeginPlay()
{
	Super::BeginPlay();

	// Imposta l'angolo della camera
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));

	// Salva posizione iniziale per reset
	InitialLocation = GetActorLocation();
	InitialRotation = GetActorRotation();
	InitialZoomDistance = SpringArm->TargetArmLength;

	// Inizializza target zoom con valore corrente
	TargetZoomDistance = SpringArm->TargetArmLength;

	// Setup Enhanced Input
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ARosikoCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Applica movimento
	if (ForwardInput != 0.0f || RightInput != 0.0f)
	{
		FVector NewLocation = GetActorLocation();

		// Converti input in movimento world-space (rispetta rotazione camera)
		FRotator Rotation = GetActorRotation();
		FVector ForwardVector = FRotationMatrix(Rotation).GetScaledAxis(EAxis::X);
		FVector RightVector = FRotationMatrix(Rotation).GetScaledAxis(EAxis::Y);

		NewLocation += ForwardVector * ForwardInput * PanSpeed * DeltaTime;
		NewLocation += RightVector * RightInput * PanSpeed * DeltaTime;

		// Clamp ai bounds della mappa (dinamici se abilitato)
		if (bClampToMapBounds)
		{
			FVector2D CurrentBoundsMin, CurrentBoundsMax;
			GetCurrentBounds(CurrentBoundsMin, CurrentBoundsMax);

			NewLocation.X = FMath::Clamp(NewLocation.X, CurrentBoundsMin.X, CurrentBoundsMax.X);
			NewLocation.Y = FMath::Clamp(NewLocation.Y, CurrentBoundsMin.Y, CurrentBoundsMax.Y);
		}

		SetActorLocation(NewLocation);
	}

	// Interpola smooth zoom verso target
	if (SpringArm)
	{
		float CurrentZoom = SpringArm->TargetArmLength;
		float NewZoom = FMath::FInterpTo(CurrentZoom, TargetZoomDistance, DeltaTime, ZoomInterpSpeed);
		SpringArm->TargetArmLength = NewZoom;
	}
}

void ARosikoCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Cast to Enhanced Input Component
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Bind MoveForward action
		if (MoveForwardAction)
		{
			EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Triggered, this, &ARosikoCamera::MoveForward);
			EnhancedInputComponent->BindAction(MoveForwardAction, ETriggerEvent::Completed, this, &ARosikoCamera::MoveForward);
		}

		// Bind MoveRight action
		if (MoveRightAction)
		{
			EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Triggered, this, &ARosikoCamera::MoveRight);
			EnhancedInputComponent->BindAction(MoveRightAction, ETriggerEvent::Completed, this, &ARosikoCamera::MoveRight);
		}

		// Bind Zoom action
		if (ZoomAction)
		{
			EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ARosikoCamera::Zoom);
		}

		// Bind Rotate action (se abilitato)
		if (bEnableRotation && RotateAction)
		{
			EnhancedInputComponent->BindAction(RotateAction, ETriggerEvent::Triggered, this, &ARosikoCamera::Rotate);
			EnhancedInputComponent->BindAction(RotateAction, ETriggerEvent::Completed, this, &ARosikoCamera::Rotate);
		}
	}
}

void ARosikoCamera::MoveForward(const FInputActionValue& Value)
{
	// Get forward/backward input (-1 to 1)
	ForwardInput = Value.Get<float>();
}

void ARosikoCamera::MoveRight(const FInputActionValue& Value)
{
	// Get right/left input (-1 to 1)
	RightInput = Value.Get<float>();
}

void ARosikoCamera::Zoom(const FInputActionValue& Value)
{
	if (SpringArm)
	{
		float ZoomValue = Value.Get<float>();

		// Cambia il target zoom (verrà interpolato smooth nel Tick)
		TargetZoomDistance -= (ZoomValue * ZoomSpeed);
		TargetZoomDistance = FMath::Clamp(TargetZoomDistance, MinZoomDistance, MaxZoomDistance);
	}
}

void ARosikoCamera::Rotate(const FInputActionValue& Value)
{
	if (bEnableRotation)
	{
		float RotationValue = Value.Get<float>();
		FRotator NewRotation = GetActorRotation();
		NewRotation.Yaw += RotationValue * RotationSpeed * GetWorld()->GetDeltaSeconds();
		SetActorRotation(NewRotation);
	}
}

void ARosikoCamera::ResetCamera()
{
	SetActorLocation(InitialLocation);
	SetActorRotation(InitialRotation);

	if (SpringArm)
	{
		SpringArm->TargetArmLength = InitialZoomDistance;
		TargetZoomDistance = InitialZoomDistance; // Reset anche il target per smooth interpolation
	}

}

float ARosikoCamera::GetCurrentWidgetScale() const
{
	if (!bDynamicWidgetScaling || !SpringArm)
	{
		return 1.0f; // Default scale
	}

	// Calcola fattore di zoom (0.0 = max vicino, 1.0 = max lontano)
	float ZoomRange = MaxZoomDistance - MinZoomDistance;
	float CurrentZoom = SpringArm->TargetArmLength;
	float ZoomFactor = (CurrentZoom - MinZoomDistance) / ZoomRange;
	ZoomFactor = FMath::Clamp(ZoomFactor, 0.0f, 1.0f);

	// Interpola tra MinWidgetScale (vicino) e MaxWidgetScale (lontano)
	float Scale = FMath::Lerp(MinWidgetScale, MaxWidgetScale, ZoomFactor);

	return Scale;
}

void ARosikoCamera::GetCurrentBounds(FVector2D& OutMin, FVector2D& OutMax) const
{
	OutMin = MapBoundsMin;
	OutMax = MapBoundsMax;

	if (!bDynamicBounds || !SpringArm)
	{
		return; // Usa bounds statici
	}

	// Calcola fattore di zoom (0.0 = max vicino, 1.0 = max lontano)
	float ZoomRange = MaxZoomDistance - MinZoomDistance;
	float CurrentZoom = SpringArm->TargetArmLength;
	float ZoomFactor = (CurrentZoom - MinZoomDistance) / ZoomRange;
	ZoomFactor = FMath::Clamp(ZoomFactor, 0.0f, 1.0f);

	// Quando sei vicino (ZoomFactor = 0), espandi i boundaries
	// Quando sei lontano (ZoomFactor = 1), usa boundaries originali
	float ExpansionAmount = (1.0f - ZoomFactor) * BoundsExpansionFactor;

	// Espandi boundaries proporzionalmente
	FVector2D BoundsSize = MapBoundsMax - MapBoundsMin;
	FVector2D Expansion = BoundsSize * ExpansionAmount * 0.5f; // 0.5 perché espandiamo da entrambi i lati

	OutMin = MapBoundsMin - Expansion;
	OutMax = MapBoundsMax + Expansion;
}

