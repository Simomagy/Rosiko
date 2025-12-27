#include "TroopVisualManager.h"
#include "TroopDisplayComponent.h"
#include "../../Core/Camera/RosikoCamera.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogTroopVisualManager, Log, All);

UTroopVisualManager::UTroopVisualManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Disabilitato finché non serve

	// TroopInstancedMesh creato in BeginPlay per corretta gerarchia transform
}

void UTroopVisualManager::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTroopVisualManager, Log, TEXT("TroopVisualManager BeginPlay - Owner: %s"),
	       GetOwner() ? *GetOwner()->GetName() : TEXT("NULL"));

	// Crea TroopInstancedMesh runtime (NewObject per corretta gerarchia transform)
	CreateInstancedMeshComponent();

	InitializeComponents();
	InitializeCamera();

	CreateBaseMaterial();

	UE_LOG(LogTroopVisualManager, Log, TEXT("TroopVisualManager initialized - Mesh: %s, Widget: %s"),
	       TroopInstancedMesh ? TEXT("OK") : TEXT("NULL"),
	       WidgetComponent ? TEXT("OK") : TEXT("NULL"));
}

void UTroopVisualManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear timer
	if (GetWorld() && DistanceCheckTimer.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(DistanceCheckTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void UTroopVisualManager::CreateInstancedMeshComponent()
{
	if (TroopInstancedMesh) return; // Già creato

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Crea il componente con NewObject (runtime) per corretta gerarchia
	TroopInstancedMesh = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("TroopInstancedMesh"));
	if (TroopInstancedMesh)
	{
		// Configura PRIMA della registrazione
		TroopInstancedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TroopInstancedMesh->SetCastShadow(false);
		TroopInstancedMesh->SetVisibility(false);
		TroopInstancedMesh->NumCustomDataFloats = 0;
		TroopInstancedMesh->bUseDefaultCollision = false;

		// Carica mesh: usa custom se fornita, altrimenti placeholder Cone
		UStaticMesh* MeshToUse = TroopMeshAsset;
		if (!MeshToUse)
		{
			MeshToUse = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
		}

		if (MeshToUse)
		{
			TroopInstancedMesh->SetStaticMesh(MeshToUse);
		}

		// Registra il componente (necessario per NewObject runtime)
		TroopInstancedMesh->RegisterComponent();

		// Attacca DOPO registrazione con AttachToComponent
		TroopInstancedMesh->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

		UE_LOG(LogTroopVisualManager, Log, TEXT("Created TroopInstancedMesh - Mesh: %s, WorldLoc: %s, ParentLoc: %s"),
		       MeshToUse ? *MeshToUse->GetName() : TEXT("NULL"),
		       *TroopInstancedMesh->GetComponentLocation().ToString(),
		       *GetComponentLocation().ToString());
	}
}

void UTroopVisualManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Aggiorna fade transition se in corso
	if (CurrentMode != TargetMode || FadeProgress < 1.0f)
	{
		UpdateFadeTransition(DeltaTime);
		return; // Durante transizione, non fare altro
	}

	// Se in WidgetMode e transizione completata, disabilita tick
	if (CurrentMode == ETroopDisplayMode::WidgetMode)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// MeshMode: aggiorna scala SOLO se zoom è cambiato significativamente
	if (bDynamicScaling && TroopInstancedMesh && CurrentTroopCount > 0)
	{
		float CurrentZoom = GetCurrentZoomDistance();

		// Aggiorna solo se zoom cambiato oltre threshold
		if (FMath::Abs(CurrentZoom - LastScaleUpdateZoom) > ZoomChangeThreshold)
		{
			LastScaleUpdateZoom = CurrentZoom;

			float DynamicScale = GetDynamicMeshScale();
			FVector Scale = FVector(DynamicScale * BaseTroopScale / 100.0f);

			// Batch update: aggiorna array locale
			for (int32 i = 0; i < InstanceTransforms.Num(); i++)
			{
				InstanceTransforms[i].SetScale3D(Scale);
			}

			// Single batch update al GPU
			UpdateInstanceTransforms();
		}
	}
}

void UTroopVisualManager::InitializeComponents()
{
	if (bComponentsInitialized) return;

	// Cerca TroopDisplayComponent nello stesso actor
	AActor* Owner = GetOwner();
	if (Owner)
	{
		WidgetComponent = Owner->FindComponentByClass<UTroopDisplayComponent>();
		if (WidgetComponent)
		{
			UE_LOG(LogTroopVisualManager, Log, TEXT("Found TroopDisplayComponent"));
		}
		else
		{
			UE_LOG(LogTroopVisualManager, Warning, TEXT("TroopDisplayComponent not found! Widget mode will not work."));
		}
	}

	bComponentsInitialized = true;
}

void UTroopVisualManager::InitializeCamera()
{
	if (CachedCamera && CachedSpringArm) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Cerca player controller locale
	APlayerController* PC = nullptr;
	if (World->GetFirstLocalPlayerFromController())
	{
		PC = World->GetFirstLocalPlayerFromController()->GetPlayerController(World);
	}

	if (!PC)
	{
		PC = World->GetFirstPlayerController();
	}

	if (PC && PC->GetPawn())
	{
		CachedCamera = Cast<ARosikoCamera>(PC->GetPawn());

		if (CachedCamera)
		{
			// Cache SpringArm subito (evita FindComponentByClass ogni frame)
			CachedSpringArm = CachedCamera->FindComponentByClass<USpringArmComponent>();

			if (CachedSpringArm)
			{
				UE_LOG(LogTroopVisualManager, Log, TEXT("Camera and SpringArm initialized successfully"));
			}
			else
			{
				UE_LOG(LogTroopVisualManager, Warning, TEXT("SpringArm not found on camera!"));
			}
		}
		else if (!bCameraWarningLogged)
		{
			UE_LOG(LogTroopVisualManager, Warning, TEXT("Player pawn is not ARosikoCamera! Distance-based LOD disabled."));
			bCameraWarningLogged = true;
		}
	}
}

void UTroopVisualManager::CreateBaseMaterial()
{
	// Crea material instance dinamico per colorare le mesh
	// Per ora usa material base engine, può essere sostituito con custom material
	UMaterial* BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	if (BaseMaterial)
	{
		TroopMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);

		// Applica material al componente instanced
		if (TroopInstancedMesh)
		{
			TroopInstancedMesh->SetMaterial(0, TroopMaterialInstance);
		}

		UE_LOG(LogTroopVisualManager, Log, TEXT("Created dynamic material instance"));
	}
}

void UTroopVisualManager::UpdateTroopDisplay(int32 TroopCount, FLinearColor OwnerColor)
{
	// Aggiorna dati solo se cambiati
	bool bCountChanged = (TroopCount != CurrentTroopCount);
	bool bColorChanged = !OwnerColor.Equals(CurrentOwnerColor, 0.01f);

	if (!bCountChanged && !bColorChanged)
	{
		return;
	}

	UE_LOG(LogTroopVisualManager, Verbose, TEXT("UpdateTroopDisplay: Count=%d, Color=(%f,%f,%f)"),
	       TroopCount, OwnerColor.R, OwnerColor.G, OwnerColor.B);

	// Se è la prima volta che abbiamo truppe, avvia il timer distance check
	bool bFirstTroops = (CurrentTroopCount == 0 && TroopCount > 0);

	CurrentTroopCount = TroopCount;
	CurrentOwnerColor = OwnerColor;
	bDataChanged = true;

	// Avvia timer SOLO se abbiamo truppe per la prima volta
	if (bFirstTroops && GetWorld() && !DistanceCheckTimer.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			DistanceCheckTimer,
			this,
			&UTroopVisualManager::CheckCameraDistance,
			DistanceCheckInterval,
			true // Loop
		);
		UE_LOG(LogTroopVisualManager, Warning, TEXT("[%s] Started distance check timer (has troops now)"),
		       *GetOwner()->GetName());
	}

	// Aggiorna visualizzazione in base a modalità corrente
	if (CurrentMode == ETroopDisplayMode::MeshMode)
	{
		UpdateMeshDisplay();
	}
	else
	{
		UpdateWidgetDisplay();
	}
}

void UTroopVisualManager::ForceRefresh()
{
	bDataChanged = true;

	if (CurrentMode == ETroopDisplayMode::MeshMode)
	{
		UpdateMeshDisplay();
	}
	else
	{
		UpdateWidgetDisplay();
	}
}

void UTroopVisualManager::CheckCameraDistance()
{
	if (!CachedCamera)
	{
		InitializeCamera();
		return;
	}

	// Se non ci sono truppe, non fare check (performance)
	if (CurrentTroopCount <= 0)
	{
		return;
	}

	float ZoomDistance = GetCurrentZoomDistance();

	// Hysteresis: threshold diversi per zoom in/out (evita flickering)
	ETroopDisplayMode DesiredMode = CurrentMode;

	if (ZoomDistance < ZoomInThreshold)
	{
		// Zoom in → passa a mesh
		DesiredMode = ETroopDisplayMode::MeshMode;
	}
	else if (ZoomDistance > ZoomOutThreshold)
	{
		// Zoom out → passa a widget
		DesiredMode = ETroopDisplayMode::WidgetMode;
	}
	// Altrimenti mantieni modalità corrente (zona hysteresis)

	// Transizione se necessario
	if (DesiredMode != TargetMode)
	{
		UE_LOG(LogTroopVisualManager, Warning, TEXT("[%s] Mode transition: %s -> %s (Zoom: %.0f)"),
		       *GetOwner()->GetName(),
		       CurrentMode == ETroopDisplayMode::MeshMode ? TEXT("MESH") : TEXT("WIDGET"),
		       DesiredMode == ETroopDisplayMode::MeshMode ? TEXT("MESH") : TEXT("WIDGET"),
		       ZoomDistance);
		TransitionToMode(DesiredMode);
	}
}

void UTroopVisualManager::TransitionToMode(ETroopDisplayMode NewMode)
{
	if (NewMode == TargetMode) return;

	UE_LOG(LogTroopVisualManager, Log, TEXT("Transitioning to %s mode"),
	       NewMode == ETroopDisplayMode::MeshMode ? TEXT("MESH") : TEXT("WIDGET"));

	TargetMode = NewMode;
	FadeProgress = 0.0f; // Reset fade

	// Riattiva tick per transizione/mesh mode
	SetComponentTickEnabled(true);
}

void UTroopVisualManager::UpdateFadeTransition(float DeltaTime)
{
	// Avanza fade progress
	if (FadeProgress < 1.0f)
	{
		FadeProgress += (DeltaTime / FadeDuration);
		FadeProgress = FMath::Clamp(FadeProgress, 0.0f, 1.0f);
	}

	// Smooth curve (ease in-out)
	float SmoothProgress = FMath::SmoothStep(0.0f, 1.0f, FadeProgress);

	// Calcola opacity per entrambe le modalità
	float OldModeOpacity = 1.0f - SmoothProgress;
	float NewModeOpacity = SmoothProgress;

	// Applica fade
	if (CurrentMode == ETroopDisplayMode::WidgetMode)
	{
		// Fade out widget, fade in mesh
		if (WidgetComponent)
		{
			// Usa SetDisplayVisible che gestisce anche SetHiddenInGame
			WidgetComponent->SetDisplayVisible(OldModeOpacity > 0.01f);
		}

		// Fade in mesh instanced
		if (TroopInstancedMesh)
		{
			TroopInstancedMesh->SetVisibility(NewModeOpacity > 0.01f);
		}
	}
	else
	{
		// Fade out mesh, fade in widget
		if (TroopInstancedMesh)
		{
			TroopInstancedMesh->SetVisibility(OldModeOpacity > 0.01f);
		}

		if (WidgetComponent)
		{
			// Usa SetDisplayVisible che gestisce anche SetHiddenInGame
			WidgetComponent->SetDisplayVisible(NewModeOpacity > 0.01f);
		}
	}

	// Quando fade completo, cambia modalità
	if (FadeProgress >= 1.0f && CurrentMode != TargetMode)
	{
		CurrentMode = TargetMode;

		// Aggiorna display per nuova modalità
		if (CurrentMode == ETroopDisplayMode::MeshMode)
		{
			UpdateMeshDisplay();
			if (WidgetComponent)
			{
				WidgetComponent->SetDisplayVisible(false);
			}
		}
		else
		{
			UpdateWidgetDisplay();
			ClearAllInstances();
		}

		UE_LOG(LogTroopVisualManager, Log, TEXT("Transition complete - now in %s mode"),
		       CurrentMode == ETroopDisplayMode::MeshMode ? TEXT("MESH") : TEXT("WIDGET"));
	}
}

void UTroopVisualManager::UpdateMeshDisplay()
{
	if (!TroopInstancedMesh)
	{
		UE_LOG(LogTroopVisualManager, Error, TEXT("TroopInstancedMesh is null! Component not created?"));
		return;
	}

	if (CurrentTroopCount <= 0)
	{
		ClearAllInstances();
		return;
	}

	// Clear old instances
	TroopInstancedMesh->ClearInstances();
	InstanceTransforms.Empty();
	InstanceTransforms.Reserve(CurrentTroopCount); // Pre-alloca per evitare realloc

	// Calcola scala dinamica
	float DynamicScale = GetDynamicMeshScale();
	FVector Scale = FVector(DynamicScale * BaseTroopScale / 100.0f);

	// Salva zoom corrente per tracking scale updates
	LastScaleUpdateZoom = GetCurrentZoomDistance();

	// Crea nuove istanze con posizioni random scatter
	for (int32 i = 0; i < CurrentTroopCount; i++)
	{
		FVector Position = GetRandomScatterPosition();
		FRotator Rotation = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
		FTransform Transform(Rotation, Position, Scale);

		InstanceTransforms.Add(Transform);
		TroopInstancedMesh->AddInstance(Transform);
	}

	// Aggiorna colore
	UpdateInstanceColors();

	// Mostra
	TroopInstancedMesh->SetVisibility(true);

	UE_LOG(LogTroopVisualManager, Verbose, TEXT("Updated mesh display: %d instances"), CurrentTroopCount);
}

void UTroopVisualManager::UpdateInstanceTransforms()
{
	if (!TroopInstancedMesh || InstanceTransforms.Num() == 0) return;

	// Batch update: NON marcare dirty per ogni istanza (false, false, false)
	// Questo è MOLTO più performante
	const int32 NumInstances = InstanceTransforms.Num();
	for (int32 i = 0; i < NumInstances; i++)
	{
		TroopInstancedMesh->UpdateInstanceTransform(i, InstanceTransforms[i], false, false, false);
	}

	// Single dirty mark alla fine (batch)
	TroopInstancedMesh->MarkRenderStateDirty();
}

void UTroopVisualManager::UpdateInstanceColors()
{
	if (!TroopMaterialInstance) return;

	// Imposta colore nel material (applicato a tutte le istanze)
	TroopMaterialInstance->SetVectorParameterValue(FName("BaseColor"), CurrentOwnerColor);
}

void UTroopVisualManager::ClearAllInstances()
{
	if (TroopInstancedMesh)
	{
		TroopInstancedMesh->ClearInstances();
		TroopInstancedMesh->SetVisibility(false);
	}

	InstanceTransforms.Empty();
}

void UTroopVisualManager::UpdateWidgetDisplay()
{
	if (!WidgetComponent)
	{
		UE_LOG(LogTroopVisualManager, Warning, TEXT("UpdateWidgetDisplay: WidgetComponent is NULL!"));
		return;
	}

	UE_LOG(LogTroopVisualManager, Log, TEXT("UpdateWidgetDisplay: Count=%d, Visible=%s"),
	       CurrentTroopCount, CurrentTroopCount > 0 ? TEXT("YES") : TEXT("NO"));

	WidgetComponent->UpdateDisplay(CurrentTroopCount, CurrentOwnerColor);
	WidgetComponent->SetDisplayVisible(CurrentTroopCount > 0);

	// Aggiorna scala widget (dato che tick è disabilitato)
	WidgetComponent->UpdateScaleFromCamera();
}

float UTroopVisualManager::GetCurrentZoomDistance() const
{
	if (!CachedSpringArm) return 9999.0f; // Default lontano

	return CachedSpringArm->TargetArmLength;
}

float UTroopVisualManager::GetDynamicMeshScale() const
{
	if (!bDynamicScaling || !CachedSpringArm) return 1.0f;

	// Ottieni range zoom dalla camera (hardcoded per ora, potrebbe essere esposto)
	constexpr float MinZoomDistance = 1000.0f;
	constexpr float MaxZoomDistance = 10000.0f;
	constexpr float ZoomRange = MaxZoomDistance - MinZoomDistance;

	float CurrentZoom = CachedSpringArm->TargetArmLength;
	float ZoomFactor = (CurrentZoom - MinZoomDistance) / ZoomRange;
	ZoomFactor = FMath::Clamp(ZoomFactor, 0.0f, 1.0f);

	// Interpola tra MinMeshScale (vicino) e MaxMeshScale (lontano)
	return FMath::Lerp(MinMeshScale, MaxMeshScale, ZoomFactor);
}

FVector UTroopVisualManager::GetRandomScatterPosition() const
{
	// Random scatter in cerchio 2D + altezza fissa
	float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	float Distance = FMath::FRandRange(0.0f, ScatterRadius);

	float X = FMath::Cos(Angle) * Distance;
	float Y = FMath::Sin(Angle) * Distance;
	float Z = MeshHeightOffset;

	return FVector(X, Y, Z);
}
