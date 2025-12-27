#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapDataStructs.h"
#include "ProceduralMeshComponent.h"
#include "MapGenerator.generated.h"

UENUM(BlueprintType)
enum class EMapGenerationState : uint8
{
    Idle,               // Non sta generando
    Initializing,       // Setup iniziale
    PopulatingGrid,     // Assegnazione continenti
    GeneratingSeeds,    // Creazione seed territori
    VoronoiPass,        // Jump Flood Algorithm (multiple iterations)
    BuildingGeometry,   // Creazione mesh
    SpawningVisuals,    // Spawn actor territori
    Complete            // Generazione completata
};

UCLASS()
class ROSIKO_API AMapGenerator : public AActor
{
    GENERATED_BODY()

public:
    AMapGenerator();

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
    // IL SEME: La chiave del determinismo P2P.
    // REPLICATO: Client ricevono seed e generano mappa localmente (risparmio bandwidth!)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_MapSeed, Category = "01_Config", meta = (ExposeOnSpawn = true))
    int32 MapSeed = 12345;

    // Data Asset di Configurazione (Preset)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "01_Config")
    class UMapGenerationConfig* Configuration;

    // Funzione principale che avvia tutto (VOXEL MODE) - SINCRONA (blocca tutto)
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "00_Commands")
    void GenerateMap();

    // Generazione ASINCRONA (tick-based, con progress events) - CONSIGLIATO per gameplay
    UFUNCTION(BlueprintCallable, Category = "00_Commands")
    void GenerateMapAsync();

    // Cancella generazione asincrona in corso
    UFUNCTION(BlueprintCallable, Category = "00_Commands")
    void CancelAsyncGeneration();

    // Pulisce la mappa generata
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "00_Commands")
    void ClearMap();

    // Ottieni i dati dei territori generati (per GameManager)
    UFUNCTION(BlueprintPure, Category = "Map Data")
    const TArray<FGeneratedTerritory>& GetGeneratedTerritories() const { return GeneratedData; }

    // Ottieni stato generazione corrente
    UFUNCTION(BlueprintPure, Category = "Map Data")
    EMapGenerationState GetGenerationState() const { return AsyncState; }

    // Ottieni progresso generazione (0.0 - 1.0)
    UFUNCTION(BlueprintPure, Category = "Map Data")
    float GetGenerationProgress() const { return AsyncProgress; }

    // Verifica se generazione è completa
    UFUNCTION(BlueprintPure, Category = "Map Data")
    bool IsGenerationComplete() const { return AsyncState == EMapGenerationState::Complete; }

    // === EVENTI ===

    // Broadcast quando progresso cambia (0.0 - 1.0)
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGenerationProgress, float, Progress, FString, StatusText);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGenerationProgress OnGenerationProgress;

    // Broadcast quando generazione è completata
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGenerationComplete);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGenerationComplete OnGenerationComplete;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    // === REPLICATION CALLBACKS ===

    UFUNCTION()
    void OnRep_MapSeed();

    // === STRUCT DEFINITIONS (must be before usage) ===

    // Seed structure for territory generation
    struct FGridSeed
    {
        int32 ID;
        int32 X;
        int32 Y;
        int32 ContIndex;
        float BaseHeight;
    };

    struct FVoxelCell
    {
        int32 ContinentIndex = -1; // -1 = Ocean/Invalid
        int32 TerritoryID = -1;    // ID univoco del territorio finale
        float Height = 20.0f;      // Altezza Z di questa cella (per effetto profondità)
        const FGridSeed* ClosestSeed = nullptr; // Per Jump Flood Algorithm
    };

    // === MEMBER VARIABLES ===

    // Generatore di numeri casuali deterministico
    FRandomStream RNG;

    // I dati calcolati
    TArray<FGeneratedTerritory> GeneratedData;

    // Teniamo traccia degli actor spawnati per poterli distruggere quando rigeneriamo
    UPROPERTY()
    TArray<class ATerritoryActor*> SpawnedTerritories;

    // Griglia locale. Usiamo un array 1D mappato 2D per semplicità.
    TArray<FVoxelCell> VoxelGrid;
    int32 GridSizeX = 0;
    int32 GridSizeY = 0;

    // === ASYNC GENERATION STATE ===

    // Stato corrente generazione asincrona
    EMapGenerationState AsyncState = EMapGenerationState::Idle;

    // Progresso 0.0 - 1.0
    float AsyncProgress = 0.0f;

    // Current step index (per generazione multi-step)
    int32 AsyncCurrentStep = 0;

    // Numero totale step
    int32 AsyncTotalSteps = 0;

    // Async data (persistente tra step)
    TArray<FGridSeed> AsyncSeeds;
    int32 AsyncCurrentJumpSize = 0;
    int32 AsyncCurrentGeometryIndex = 0;
    int32 AsyncCurrentSpawnIndex = 0;
    TMap<int32, TArray<FString>> AsyncUsedNamesPerContinent;

    // Timing tracking
    double AsyncStartTime = 0.0;
    double AsyncEndTime = 0.0;

    // Mesh piana per mostrare la texture di debug sotto alla mappa
    UPROPERTY(VisibleAnywhere, Category = "Debug")
    UStaticMeshComponent* DebugPlane;

    // --- Passaggi interni (SINCRONO) ---
    void GenerateVoxels(int32 GridResolution); // New Voxel Algorithm

    void SpawnVisuals();
    void DrawDebugVisuals();

    // --- Passaggi interni (ASINCRONO) ---
    void ProcessAsyncTick(); // Chiamato da Tick() quando AsyncState != Idle
    void UpdateAsyncProgress(float NewProgress, const FString& StatusText);
    void CompleteAsyncGeneration();

    // Step functions (ogni step esegue parte della generazione)
    void AsyncStep_Initialize();
    void AsyncStep_PopulateGrid();
    void AsyncStep_GenerateSeeds();
    void AsyncStep_VoronoiIteration();
    void AsyncStep_BuildGeometry();
    void AsyncStep_SpawnVisuals();

    // Helper per generazione mesh
    void AddQuadFace(FGeneratedTerritory& Data, const FVector& V1, const FVector& V2, const FVector& Normal, const FLinearColor& Color);

    // Helper per accedere alle celle della griglia
    FORCEINLINE FVoxelCell* GetCell(int32 X, int32 Y)
    {
        if (X >= 0 && X < GridSizeX && Y >= 0 && Y < GridSizeY)
        {
            return &VoxelGrid[Y * GridSizeX + X];
        }
        return nullptr;
    }
};
