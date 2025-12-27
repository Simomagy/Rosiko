#include "TroopDisplayComponent.h"
#include "TroopWidget.h"
#include "../../Core/Camera/RosikoCamera.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRosikoTroopsDisplay, Log, All);

UTroopDisplayComponent::UTroopDisplayComponent()
{
	// Setup widget component
	SetWidgetSpace(EWidgetSpace::World); // Widget 3D nello spazio mondo
	SetDrawSize(FVector2D(200.0f, 100.0f)); // Dimensione widget
	SetPivot(FVector2D(0.5f, 0.5f)); // Centrato

	// Collision disabled (è solo visuale)
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Visibile di default
	SetVisibility(true);
	SetHiddenInGame(false);

	// PERFORMANCE: Tick disabilitato di default
	// TroopVisualManager gestisce quando aggiornare la scala
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTroopDisplayComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeWidget();
	InitializeCamera();

	// Salva scala base del componente
	BaseDrawScale = GetRelativeScale3D();
}

void UTroopDisplayComponent::InitializeWidget()
{
	if (!CachedWidget)
	{
		UUserWidget* WidgetObject = GetUserWidgetObject();
		if (WidgetObject)
		{
			CachedWidget = Cast<UTroopWidget>(WidgetObject);
			if (!CachedWidget)
			{
				UE_LOG(LogRosikoTroopsDisplay, Warning, TEXT("TroopDisplayComponent: Widget is not a UTroopWidget! Make sure WBP inherits from UTroopWidget C++ class."));
			}
			else
			{
				UE_LOG(LogRosikoTroopsDisplay, Log, TEXT("TroopDisplayComponent: Widget initialized successfully"));
			}
		}
		else
		{
			UE_LOG(LogRosikoTroopsDisplay, Warning, TEXT("TroopDisplayComponent: GetUserWidgetObject() returned null! Widget Class not assigned in Blueprint?"));
		}
	}
}

void UTroopDisplayComponent::SetTroopCount(int32 Count)
{
	TroopCount = Count;

	InitializeWidget();

	if (CachedWidget)
	{
		CachedWidget->UpdateDisplay(TroopCount, OwnerColor);
	}
}

void UTroopDisplayComponent::SetOwnerColor(FLinearColor Color)
{
	OwnerColor = Color;

	InitializeWidget();

	if (CachedWidget)
	{
		CachedWidget->UpdateDisplay(TroopCount, OwnerColor);
	}
}

void UTroopDisplayComponent::SetDisplayVisible(bool bShouldBeVisible)
{
	SetVisibility(bShouldBeVisible);
	SetHiddenInGame(!bShouldBeVisible);

	if (bShouldBeVisible)
	{
		FVector WorldLoc = GetComponentLocation();
		FVector Scale = GetRelativeScale3D();
		UE_LOG(LogRosikoTroopsDisplay, Warning, TEXT("Widget VISIBLE - WorldPos: %s, Scale: %s, DrawSize: %s"),
		       *WorldLoc.ToString(), *Scale.ToString(), *GetDrawSize().ToString());
	}
}

void UTroopDisplayComponent::UpdateDisplay(int32 Count, FLinearColor Color)
{
	TroopCount = Count;
	OwnerColor = Color;

	UE_LOG(LogRosikoTroopsDisplay, Log, TEXT("TroopDisplayComponent::UpdateDisplay: Count=%d, Color=(%f,%f,%f)"),
	       Count, Color.R, Color.G, Color.B);

	InitializeWidget();

	if (CachedWidget)
	{
		CachedWidget->UpdateDisplay(TroopCount, OwnerColor);
		UE_LOG(LogRosikoTroopsDisplay, Log, TEXT("TroopDisplayComponent: Widget updated successfully"));
	}
	else
	{
		// Log più dettagliato sul problema
		UE_LOG(LogRosikoTroopsDisplay, Error, TEXT("TroopDisplayComponent: CachedWidget is null! Widget Class assigned in Blueprint? Check BP_Territory->TroopDisplay->Widget Class"));
	}
}

void UTroopDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Aggiorna scala widget in base allo zoom camera
	UpdateScaleFromCamera();
}

void UTroopDisplayComponent::InitializeCamera()
{
	if (!CachedCamera)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// In multiplayer, cerca il player controller locale (non necessariamente il primo)
			APlayerController* PC = nullptr;

			// Prova prima con local player controller (corretto per multiplayer)
			if (World->GetFirstLocalPlayerFromController())
			{
				PC = World->GetFirstLocalPlayerFromController()->GetPlayerController(World);
			}

			// Fallback a first player controller (single player o server)
			if (!PC)
			{
				PC = World->GetFirstPlayerController();
			}

			if (PC && PC->GetPawn())
			{
				CachedCamera = Cast<ARosikoCamera>(PC->GetPawn());

				if (CachedCamera)
				{
					UE_LOG(LogRosikoTroopsDisplay, Log, TEXT("TroopDisplayComponent: Camera initialized successfully"));
				}
				else if (!bCameraWarningLogged)
				{
					// Log warning solo la prima volta (evita spam)
					UE_LOG(LogRosikoTroopsDisplay, Warning, TEXT("TroopDisplayComponent: Player pawn is not ARosikoCamera! Dynamic widget scaling disabled."));
					bCameraWarningLogged = true;
				}
			}
		}
	}
}

void UTroopDisplayComponent::UpdateScaleFromCamera()
{
	if (!CachedCamera)
	{
		// Rate limiting: retry inizializzazione solo ogni X secondi (non ogni frame)
		CameraInitRetryTimer -= GetWorld()->GetDeltaSeconds();

		if (CameraInitRetryTimer <= 0.0f)
		{
			InitializeCamera();
			CameraInitRetryTimer = CameraInitRetryInterval; // Reset timer
		}

		if (!CachedCamera)
		{
			// Camera non pronta, usa scala default
			UE_LOG(LogRosikoTroopsDisplay, Warning, TEXT("UpdateScaleFromCamera: Camera not ready, using default scale"));
			return;
		}
	}

	// Ottieni scala corrente dalla camera
	float DynamicScale = CachedCamera->GetCurrentWidgetScale();

	// Applica scala al componente widget (moltiplica per scala base)
	FVector NewScale = BaseDrawScale * DynamicScale;
	SetRelativeScale3D(NewScale);

	UE_LOG(LogRosikoTroopsDisplay, Verbose, TEXT("Widget scale updated: DynamicScale=%.2f, NewScale=%s"),
	       DynamicScale, *NewScale.ToString());
}
