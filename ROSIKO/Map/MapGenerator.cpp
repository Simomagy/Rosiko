#include "MapGenerator.h"
#include "./Territory/TerritoryActor.h"
#include "../Configs/MapGenerationConfig.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"

// Log category per MapGenerator
DEFINE_LOG_CATEGORY_STATIC(LogRosikoMapGen, Log, All);

AMapGenerator::AMapGenerator()
{
	PrimaryActorTick.bCanEverTick = true; // Abilitato per async generation

	// Abilita replicazione per multiplayer (replica solo seed, non geometria!)
	bReplicates = true;
	bAlwaysRelevant = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    DebugPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DebugPlane"));
    DebugPlane->SetupAttachment(RootComponent);

    // Carica la mesh standard "Plane" dell'Engine
    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshObj(TEXT("/Engine/BasicShapes/Plane"));
    if (PlaneMeshObj.Succeeded())
    {
        DebugPlane->SetStaticMesh(PlaneMeshObj.Object);
    }

    DebugPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DebugPlane->SetHiddenInGame(false); // Visibile anche in Play/Simulate per debug
    DebugPlane->SetVisibility(false); // Di default nascosto finché non generiamo
}

void AMapGenerator::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogRosikoMapGen, Log, TEXT("MapGenerator::BeginPlay() - HasAuthority: %s, MapSeed: %d"),
	       HasAuthority() ? TEXT("TRUE (Server)") : TEXT("FALSE (Client)"), MapSeed);

	// Client: Se MapSeed è già impostato (replicato prima di BeginPlay), genera mappa
	if (!HasAuthority() && MapSeed != 0)
	{
		UE_LOG(LogRosikoMapGen, Log, TEXT("Client: MapSeed already set, generating map locally..."));
		// Delay di 1 frame per essere sicuri che tutto sia inizializzato
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
		{
			GenerateMapAsync();
		}, 0.1f, false);
	}
}

void AMapGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replica MapSeed ai client (pochi byte invece di tutta la geometria!)
	DOREPLIFETIME(AMapGenerator, MapSeed);
}

void AMapGenerator::OnRep_MapSeed()
{
	UE_LOG(LogRosikoMapGen, Warning, TEXT("MapGenerator::OnRep_MapSeed() called - MapSeed: %d"), MapSeed);

	// Client riceve seed dal server → genera mappa localmente
	if (!HasAuthority()) // Solo client, non server
	{
		UE_LOG(LogRosikoMapGen, Warning, TEXT("Client: OnRep_MapSeed triggered, generating map with replicated seed..."));
		GenerateMapAsync();
	}
}

void AMapGenerator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Se stiamo generando in modo asincrono, processa uno step per frame
	if (AsyncState != EMapGenerationState::Idle && AsyncState != EMapGenerationState::Complete)
	{
		ProcessAsyncTick();
	}
}


void AMapGenerator::AddQuadFace(FGeneratedTerritory& Data, const FVector& V1, const FVector& V2, const FVector& Normal, const FLinearColor& Color)
{
    // Helper function to add a quad face (side face of voxel)
    // V1 = top-left corner, V2 = bottom-right corner of the quad
    // Generates 4 vertices forming a quad from V1 (top) to V2 (bottom) with ground projection

    int32 SideIdx = Data.Vertices.Num();

    FVector V1_Ground = V1; V1_Ground.Z = 0;
    FVector V2_Ground = V2; V2_Ground.Z = 0;

    // Add 4 vertices: V1(top), V1_Ground(bottom), V2(top), V2_Ground(bottom)
    Data.Vertices.Add(V1);
    Data.Vertices.Add(V1_Ground);
    Data.Vertices.Add(V2);
    Data.Vertices.Add(V2_Ground);

    // Add normals (all same direction)
    Data.Normals.Add(Normal);
    Data.Normals.Add(Normal);
    Data.Normals.Add(Normal);
    Data.Normals.Add(Normal);

    // Add vertex colors
    Data.VertexColors.Add(Color);
    Data.VertexColors.Add(Color);
    Data.VertexColors.Add(Color);
    Data.VertexColors.Add(Color);

    // Add triangles (2 triangles forming quad)
    Data.Triangles.Add(SideIdx + 0);
    Data.Triangles.Add(SideIdx + 2);
    Data.Triangles.Add(SideIdx + 1);

    Data.Triangles.Add(SideIdx + 2);
    Data.Triangles.Add(SideIdx + 3);
    Data.Triangles.Add(SideIdx + 1);
}



void AMapGenerator::GenerateMap()
{
    // Start timing
    double StartTime = FPlatformTime::Seconds();

    // New Voxel Entry Point
    // Use a fixed high resolution for now (e.g., 200xAspectRatio) or exposed config
    // New Voxel Entry Point
    int32 BaseResolution = (Configuration) ? Configuration->GridResolution : 200;

    // Auto-adjust Aspect Ratio Logic first
    if (Configuration && Configuration->GenerationMask)
    {
        int32 W = Configuration->GenerationMask->GetSizeX();
        int32 H = Configuration->GenerationMask->GetSizeY();
        if (W > 0 && H > 0)
        {
            float AspectRatio = (float)H / (float)W;
            float NewY = Configuration->MapSize.X * AspectRatio;
            if (!FMath::IsNearlyEqual(Configuration->MapSize.Y, NewY, 1.0f))
            {
                Configuration->MapSize.Y = NewY;
                UE_LOG(LogRosikoMapGen, Log, TEXT("AMapGenerator: Auto-Adjusted MapSize.Y to %f"), NewY);
            }
        }
    }

    GenerateVoxels(BaseResolution);

    // End timing and log
    double EndTime = FPlatformTime::Seconds();
    double TotalTime = EndTime - StartTime;

    UE_LOG(LogRosikoMapGen, Warning, TEXT("========================================"));
    UE_LOG(LogRosikoMapGen, Warning, TEXT("SYNC map generation COMPLETE!"));
    UE_LOG(LogRosikoMapGen, Warning, TEXT("Total generation time: %.3f seconds (%.0f ms)"), TotalTime, TotalTime * 1000.0);
    UE_LOG(LogRosikoMapGen, Warning, TEXT("Seed: %d | Territories: %d"), MapSeed, GeneratedData.Num());
    UE_LOG(LogRosikoMapGen, Warning, TEXT("========================================"));
}

void AMapGenerator::GenerateVoxels(int32 GridResolution)
{
    if (!Configuration || !Configuration->GenerationMask)
    {
        UE_LOG(LogRosikoMapGen, Error, TEXT("Voxel Gen Aborted: Missing Config or Mask!"));
        return;
    }

    ClearMap();
    GeneratedData.Empty();
    RNG.Initialize(MapSeed);

    // 1. Setup Grid Dimensions
    GridSizeX = GridResolution;
    // Aspect Ratio correction for Grid Y
    float AspectRatio = Configuration->MapSize.Y / Configuration->MapSize.X;
    GridSizeY = FMath::RoundToInt(GridResolution * AspectRatio);

    VoxelGrid.Empty();
    // Use Init instead of AddZeroed to ensure defaults (-1) are respected!
    VoxelGrid.Init(FVoxelCell(), GridSizeX * GridSizeY);

    UE_LOG(LogRosikoMapGen, Log, TEXT("Initializing Voxel Grid: %d x %d"), GridSizeX, GridSizeY);

    // 2. Texture Sampling Access
    FTexture2DMipMap* Mip = &Configuration->GenerationMask->GetPlatformData()->Mips[0];
    const uint8* RawData = (const uint8*)Mip->BulkData.LockReadOnly();
    int32 TexSizeX = Configuration->GenerationMask->GetSizeX();
    int32 TexSizeY = Configuration->GenerationMask->GetSizeY();

    auto GetColorAtUV = [&](float U, float V) -> FLinearColor {
        int32 X = FMath::Clamp(FMath::RoundToInt(U * (TexSizeX - 1)), 0, TexSizeX - 1);
        int32 Y = FMath::Clamp(FMath::RoundToInt(V * (TexSizeY - 1)), 0, TexSizeY - 1);
        int32 Index = (Y * TexSizeX + X) * 4;
        return FLinearColor(RawData[Index + 2] / 255.0f, RawData[Index + 1] / 255.0f, RawData[Index + 0] / 255.0f, 1.0f);
    };

    // STATS FOR DEBUG
    TArray<int32> CellsPerContinent;
    CellsPerContinent.AddZeroed(Configuration->ContinentSetup.Num());
    int32 TotalValidCells = 0;

    // 3. Populate Grid (Assign Continent IDs)

    // AUTO-DETECT BACKGROUND (OCEAN) COLOR from Top-Left corner (0,0)
    // This fixes the issue where Black background (0,0,0) is equidistant to Red(1,0,0)/Green/Blue
    // and gets claimed by the first continent in the list (North America).
    FLinearColor AutoOceanColor = FLinearColor::White; // Default
    if (RawData && TexSizeX > 0 && TexSizeY > 0)
    {
        // Sample 0,0
        AutoOceanColor = FLinearColor(RawData[2] / 255.0f, RawData[1] / 255.0f, RawData[0] / 255.0f, 1.0f);
        UE_LOG(LogRosikoMapGen, Warning, TEXT("Auto-Detected Ocean Color from (0,0): %s"), *AutoOceanColor.ToString());
    }

    // OPTIMIZATION: Pre-cache color conversions to FVector (eliminates 30k+ conversions in loop)
    TArray<FVector> ContinentColorsVec;
    ContinentColorsVec.Reserve(Configuration->ContinentSetup.Num());
    for(const FContinentDefinition& Cont : Configuration->ContinentSetup)
    {
        ContinentColorsVec.Add(FVector(Cont.Color.R, Cont.Color.G, Cont.Color.B));
    }

    FVector ConfigOceanVec(Configuration->OceanColor.R, Configuration->OceanColor.G, Configuration->OceanColor.B);
    FVector AutoOceanVec(AutoOceanColor.R, AutoOceanColor.G, AutoOceanColor.B);

    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            float U = (float)X / (float)GridSizeX;
            float V = (float)Y / (float)GridSizeY;

            FLinearColor Sample = GetColorAtUV(U, V);
            FVoxelCell* Cell = GetCell(X, Y);

            // 3.0 Explicit Ocean Checks

            // OPTIMIZATION: Convert Sample once to FVector
            FVector SampleVec(Sample.R, Sample.G, Sample.B);

            // A. Check against Config Ocean
            float DistConfigOcean = FVector::Dist(SampleVec, ConfigOceanVec);

            // B. Check against Auto-Detected Ocean
            float DistAutoOcean = FVector::Dist(SampleVec, AutoOceanVec);

            // C. Hardcoded Common Backgrounds (Black / White / Transparent)
            // Fixes issues where top-left is White but inner sea is Black (or vice versa).
            bool bIsBlack = (Sample.R < 0.15f && Sample.G < 0.15f && Sample.B < 0.15f);
            bool bIsWhite = (Sample.R > 0.85f && Sample.G > 0.85f && Sample.B > 0.85f);
            bool bIsTransparent = (Sample.A < 0.2f); // Alpha check

            // If match any, SKIP
            if (DistConfigOcean < 0.2f || DistAutoOcean < 0.2f || bIsBlack || bIsWhite || bIsTransparent)
            {
                Cell->ContinentIndex = -1;
                continue;
            }

            // FIND BEST MATCHING CONTINENT
            int32 BestCont = -1;
            float MinDistSq = FLT_MAX;

            // Strict Threshold for continent color matching
            float AcceptanceThresholdSq = Configuration->ContinentColorThreshold;

            // Check against Continents (using pre-cached vectors)
            for (int32 c = 0; c < ContinentColorsVec.Num(); c++)
            {
                float DistSq = FVector::DistSquared(SampleVec, ContinentColorsVec[c]);

                if (DistSq < MinDistSq && DistSq < AcceptanceThresholdSq)
                {
                    MinDistSq = DistSq;
                    BestCont = c;
                }
            }

            if (BestCont != -1)
            {
                Cell->ContinentIndex = BestCont;
                CellsPerContinent[BestCont]++;
                TotalValidCells++;
            }
        }
    }

    Mip->BulkData.Unlock();

    // 4. Territory Partitioning (Flood Fill or K-Means or Random Seeds?)
    // User wants "Voxel Style" territories.
    // If we just make "Continents", they will be huge.
    // We need to split continents into Territories.
    // SIMPLE APPROACH: Random Seeds + Voronoi ON GRID (Manhattan Distance).

    // A. Generate Seeds per Continent
    TArray<FGridSeed> Seeds;
    int32 GlobalID = 0;

    for (int32 c = 0; c < Configuration->ContinentSetup.Num(); c++)
    {
        const FContinentDefinition& Cont = Configuration->ContinentSetup[c];
        int32 TargetCount = Cont.TerritoryCount;

        // Find all valid cells for this continent
        TArray<int32> ValidIndices;
        for(int32 i=0; i<VoxelGrid.Num(); i++)
        {
            if (VoxelGrid[i].ContinentIndex == c) ValidIndices.Add(i);
        }

        if (ValidIndices.Num() == 0) continue;

        // Pick Random Cells as Centers
        for(int32 k=0; k<TargetCount; k++)
        {
            if (ValidIndices.Num() == 0) break; // Should not happen unless tiny continent

            int32 VisitIdx = RNG.RandRange(0, ValidIndices.Num()-1);
            int32 CellIdx = ValidIndices[VisitIdx];

            // Convert back to X,Y
            int32 EX = CellIdx % GridSizeX;
            int32 EY = CellIdx / GridSizeX;

            // Generate base height for this territory
            float TerritoryBaseHeight = RNG.FRandRange(Configuration->TerritoryHeightRange.X, Configuration->TerritoryHeightRange.Y);

            FGridSeed NewSeed;
            NewSeed.ID = GlobalID++;
            NewSeed.ContIndex = c;
            NewSeed.X = EX;
            NewSeed.Y = EY;
            NewSeed.BaseHeight = TerritoryBaseHeight;
            Seeds.Add(NewSeed);

            // Initial Assignment
            GetCell(EX, EY)->TerritoryID = NewSeed.ID;
        }
    }

    // B. Grow Territories using Jump Flood Algorithm (JFA) or Brute Force
    double VoronoiStartTime = FPlatformTime::Seconds();

    if (Configuration->bUseJumpFloodAlgorithm)
    {
        // JUMP FLOOD ALGORITHM: O(N log N) instead of O(N*M)
        // JFA propagates nearest seed information through "jumps" of decreasing size
        UE_LOG(LogRosikoMapGen, Log, TEXT("Using Jump Flood Algorithm for Voronoi"));

        // Step 1: Initialize seed cells (they point to themselves)
    for(const FGridSeed& S : Seeds)
    {
        FVoxelCell* SeedCell = GetCell(S.X, S.Y);
        if (SeedCell)
        {
            SeedCell->ClosestSeed = &S;
        }
    }

    // Step 2: Jump Flood iterations with decreasing jump size
    int32 MaxDimension = FMath::Max(GridSizeX, GridSizeY);
    int32 JumpSize = FMath::RoundUpToPowerOfTwo(MaxDimension) / 2; // Start from nearest power of 2

    UE_LOG(LogRosikoMapGen, Log, TEXT("Jump Flood Algorithm starting, MaxJump: %d"), JumpSize);

    while (JumpSize >= 1)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 X = 0; X < GridSizeX; X++)
            {
                FVoxelCell* Cell = GetCell(X, Y);
                if (!Cell || Cell->ContinentIndex == -1) continue; // Skip ocean/invalid

                const FGridSeed* CurrentBest = Cell->ClosestSeed;
                float CurrentBestDistSq = FLT_MAX;

                if (CurrentBest)
                {
                    CurrentBestDistSq = (float)(FMath::Square(CurrentBest->X - X) + FMath::Square(CurrentBest->Y - Y));
                }

                // Check 9 positions at distance JumpSize (3x3 grid centered on current cell)
                for(int32 DY = -1; DY <= 1; DY++)
                {
                    for(int32 DX = -1; DX <= 1; DX++)
                    {
                        int32 CheckX = X + DX * JumpSize;
                        int32 CheckY = Y + DY * JumpSize;

                        FVoxelCell* Neighbor = GetCell(CheckX, CheckY);
                        if (!Neighbor || !Neighbor->ClosestSeed) continue;

                        // IMPORTANT: Only propagate seeds within same continent
                        if (Neighbor->ClosestSeed->ContIndex != Cell->ContinentIndex) continue;

                        // Calculate distance from current cell to neighbor's closest seed
                        float DistToNeighborSeedSq = (float)(
                            FMath::Square(Neighbor->ClosestSeed->X - X) +
                            FMath::Square(Neighbor->ClosestSeed->Y - Y)
                        );

                        // If neighbor's seed is closer, adopt it
                        if (DistToNeighborSeedSq < CurrentBestDistSq)
                        {
                            CurrentBestDistSq = DistToNeighborSeedSq;
                            CurrentBest = Neighbor->ClosestSeed;
                        }
                    }
                }

                // Update cell with best seed found
                Cell->ClosestSeed = CurrentBest;
            }
        }

        JumpSize /= 2;
    }

    // Step 3: Assign TerritoryID and Height based on ClosestSeed
    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            FVoxelCell* Cell = GetCell(X, Y);
            if (!Cell || Cell->ContinentIndex == -1 || !Cell->ClosestSeed) continue;

            Cell->TerritoryID = Cell->ClosestSeed->ID;

            // Assign height: Territory Base + Cell Variation
            float CellVariation = RNG.FRandRange(Configuration->CellHeightRange.X, Configuration->CellHeightRange.Y);
            Cell->Height = Cell->ClosestSeed->BaseHeight + CellVariation;
        }
    }

        UE_LOG(LogRosikoMapGen, Log, TEXT("Jump Flood Algorithm completed"));
    }
    else
    {
        // BRUTE FORCE: O(N*M) - Check every cell against every seed
        UE_LOG(LogRosikoMapGen, Log, TEXT("Using Brute Force for Voronoi (slower, for comparison)"));

        // Put seeds in per-continent buckets for faster lookup
        TMap<int32, TArray<FGridSeed>> SeedsByCont;
        for(const FGridSeed& S : Seeds)
        {
            SeedsByCont.FindOrAdd(S.ContIndex).Add(S);
        }

        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            for (int32 X = 0; X < GridSizeX; X++)
            {
                FVoxelCell* Cell = GetCell(X, Y);
                if (!Cell || Cell->ContinentIndex == -1) continue;

                TArray<FGridSeed>* ContSeeds = SeedsByCont.Find(Cell->ContinentIndex);
                if (!ContSeeds) continue;

                float MinDistSq = FLT_MAX;
                const FGridSeed* BestSeed = nullptr;

                for(const FGridSeed& S : *ContSeeds)
                {
                    float DistSq = (float)(FMath::Square(S.X - X) + FMath::Square(S.Y - Y));
                    if (DistSq < MinDistSq)
                    {
                        MinDistSq = DistSq;
                        BestSeed = &S;
                    }
                }

                if (BestSeed)
                {
                    Cell->TerritoryID = BestSeed->ID;
                    float CellVariation = RNG.FRandRange(Configuration->CellHeightRange.X, Configuration->CellHeightRange.Y);
                    Cell->Height = BestSeed->BaseHeight + CellVariation;
                }
            }
        }
    }

    double VoronoiEndTime = FPlatformTime::Seconds();
    UE_LOG(LogRosikoMapGen, Warning, TEXT("Voronoi Generation Time: %.2f ms (JFA: %s)"),
        (VoronoiEndTime - VoronoiStartTime) * 1000.0,
        Configuration->bUseJumpFloodAlgorithm ? TEXT("ON") : TEXT("OFF"));

    // 5. Build Meshes
    // Create FGeneratedTerritory entries for each Seed (using global ID)
    GeneratedData.AddDefaulted(GlobalID);

    // Traccia i nomi usati per continente (per evitare duplicati)
    TMap<int32, TArray<FString>> UsedNamesPerContinent;
    for (int32 c = 0; c < Configuration->ContinentSetup.Num(); c++)
    {
        UsedNamesPerContinent.Add(c, TArray<FString>());
    }

    // Init Basic Info
    for(const FGridSeed& S : Seeds)
    {
        if (S.ID < GeneratedData.Num())
        {
            GeneratedData[S.ID].ID = S.ID;
            GeneratedData[S.ID].ContinentID = S.ContIndex;

            FLinearColor BaseColor = Configuration->ContinentSetup[S.ContIndex].Color;
            GeneratedData[S.ID].DebugColor = BaseColor;

            // Generate territory-specific color variation (darker shade)
            float BrightnessMultiplier = RNG.FRandRange(Configuration->TerritoryBrightnessRange.X, Configuration->TerritoryBrightnessRange.Y);
            FLinearColor TerritoryColor = BaseColor * BrightnessMultiplier;
            TerritoryColor.A = 1.0f; // Preserve alpha
            GeneratedData[S.ID].TerritoryColor = TerritoryColor;

            GeneratedData[S.ID].bIsOcean = false;

            // Assegna un nome random dal pool del continente
            const FContinentDefinition& Continent = Configuration->ContinentSetup[S.ContIndex];
            FString TerritoryName;

            if (Continent.TerritoryNames.Num() > 0)
            {
                // Crea un pool di nomi disponibili (non ancora usati)
                TArray<FString> AvailableNames;
                for (const FString& Name : Continent.TerritoryNames)
                {
                    if (!UsedNamesPerContinent[S.ContIndex].Contains(Name))
                    {
                        AvailableNames.Add(Name);
                    }
                }

                // Se abbiamo nomi disponibili, pescane uno random
                if (AvailableNames.Num() > 0)
                {
                    int32 RandomIndex = RNG.RandRange(0, AvailableNames.Num() - 1);
                    TerritoryName = AvailableNames[RandomIndex];
                    UsedNamesPerContinent[S.ContIndex].Add(TerritoryName);
                }
                else
                {
                    // Se abbiamo esaurito i nomi, genera un nome con suffisso numerico
                    int32 Suffix = UsedNamesPerContinent[S.ContIndex].Num() - Continent.TerritoryNames.Num() + 1;
                    TerritoryName = FString::Printf(TEXT("%s %d"), *Continent.Name, Suffix);
                    UsedNamesPerContinent[S.ContIndex].Add(TerritoryName);
                }
            }
            else
            {
                // Fallback: genera nome generico se il continente non ha nomi definiti
                TerritoryName = FString::Printf(TEXT("%s Territory %d"), *Continent.Name, S.ID);
            }

            GeneratedData[S.ID].Name = TerritoryName;

            // Center calculation using seed location and height
            float WorldX = (float)S.X / GridSizeX * (Configuration->MapSize.X*2) - Configuration->MapSize.X;
            float WorldY = (float)S.Y / GridSizeY * (Configuration->MapSize.Y*2) - Configuration->MapSize.Y;
            GeneratedData[S.ID].CenterPoint = FVector(WorldX, WorldY, S.BaseHeight);
        }
    }

    // Geometry Construction: "Cubes" per cell, but optimized (Face Culling)
    // Scale Factor
    float CellW = (Configuration->MapSize.X * 2.0f) / (float)GridSizeX;
    float CellH = (Configuration->MapSize.Y * 2.0f) / (float)GridSizeY;

    // OPTIMIZATION: Pre-allocate memory for mesh arrays
    // Estimate: ~16-20 vertices per cell (top + sides, no bottom)
    int32 TotalCells = GridSizeX * GridSizeY;
    int32 EstimatedCellsPerTerritory = (GlobalID > 0) ? (TotalCells / GlobalID) : TotalCells;
    int32 EstimatedVerticesPerTerritory = EstimatedCellsPerTerritory * 20;
    int32 EstimatedTrianglesPerTerritory = EstimatedVerticesPerTerritory * 2;

    for(FGeneratedTerritory& Data : GeneratedData)
    {
        Data.Vertices.Reserve(EstimatedVerticesPerTerritory);
        Data.Normals.Reserve(EstimatedVerticesPerTerritory);
        Data.VertexColors.Reserve(EstimatedVerticesPerTerritory);
        Data.Triangles.Reserve(EstimatedTrianglesPerTerritory);
    }

    // OPTIMIZATION: Cache half-width/height calculations (eliminates ~240k multiplications)
    float HalfW = 0.5f * CellW;
    float HalfH = 0.5f * CellH;

    for (int32 Y = 0; Y < GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            FVoxelCell* Cell = GetCell(X, Y);
            if (!Cell || Cell->TerritoryID == -1) continue;

            FGeneratedTerritory& Data = GeneratedData[Cell->TerritoryID];

            float WX = (float)X * CellW - Configuration->MapSize.X + HalfW; // Center of cell
            float WY = (float)Y * CellH - Configuration->MapSize.Y + HalfH;

            float CellHeight = Cell->Height; // Use dynamic height from cell

            // OPTIMIZATION: Generate vertices in LOCAL space (relative to territory center)
            // This eliminates the need to copy and transform vertices in SpawnVisuals
            FVector TerritoryCenter = Data.CenterPoint;
            FVector LocalBase(WX - TerritoryCenter.X, WY - TerritoryCenter.Y, -TerritoryCenter.Z);

            // Vertices for TOP Face (Z=+CellHeight)
            //   0 -- 1
            //   |    |
            //   2 -- 3
            FVector TL(-HalfW, -HalfH, CellHeight);
            FVector TR( HalfW, -HalfH, CellHeight);
            FVector BL(-HalfW,  HalfH, CellHeight);
            FVector BR( HalfW,  HalfH, CellHeight);

            // Top Face (Z-Up)
            int32 I = Data.Vertices.Num();
            Data.Vertices.Add(LocalBase + TL);
            Data.Vertices.Add(LocalBase + TR);
            Data.Vertices.Add(LocalBase + BR);
            Data.Vertices.Add(LocalBase + BL);

            FVector UpNormal(0,0,1);
            Data.Normals.Add(UpNormal); Data.Normals.Add(UpNormal);
            Data.Normals.Add(UpNormal); Data.Normals.Add(UpNormal);

            FLinearColor TerritoryCol = Data.TerritoryColor;
            Data.VertexColors.Add(TerritoryCol); Data.VertexColors.Add(TerritoryCol);
            Data.VertexColors.Add(TerritoryCol); Data.VertexColors.Add(TerritoryCol);

            // Triangle 1: TL-BR-TR (inverted winding)
            Data.Triangles.Add(I + 0);
            Data.Triangles.Add(I + 2);
            Data.Triangles.Add(I + 1);

            // Triangle 2: TL-BL-BR (inverted winding)
            Data.Triangles.Add(I + 0);
            Data.Triangles.Add(I + 3);
            Data.Triangles.Add(I + 2);

            // OPTIONAL: SIDE FACES (Only if Neighbor is different, Ocean, or different height)
            // OPTIMIZATION: Use helper function to reduce code duplication

            // Check Right (X+1)
            FVoxelCell* Right = GetCell(X + 1, Y);
            bool bNeedRightFace = !Right || Right->TerritoryID != Cell->TerritoryID || FMath::Abs(Right->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
            if (bNeedRightFace)
            {
                AddQuadFace(Data, LocalBase + TR, LocalBase + BR, FVector(1,0,0), Data.TerritoryColor);
            }

            // Check Left (X-1)
            FVoxelCell* Left = GetCell(X - 1, Y);
            bool bNeedLeftFace = !Left || Left->TerritoryID != Cell->TerritoryID || FMath::Abs(Left->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
            if (bNeedLeftFace)
            {
                AddQuadFace(Data, LocalBase + BL, LocalBase + TL, FVector(-1,0,0), Data.TerritoryColor);
            }

            // Check Top (Y-1)
            FVoxelCell* Top = GetCell(X, Y - 1);
            bool bNeedTopFace = !Top || Top->TerritoryID != Cell->TerritoryID || FMath::Abs(Top->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
            if (bNeedTopFace)
            {
                AddQuadFace(Data, LocalBase + TL, LocalBase + TR, FVector(0,-1,0), Data.TerritoryColor);
            }

            // Check Bottom (Y+1)
            FVoxelCell* Bot = GetCell(X, Y + 1);
            bool bNeedBotFace = !Bot || Bot->TerritoryID != Cell->TerritoryID || FMath::Abs(Bot->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
            if (bNeedBotFace)
            {
                AddQuadFace(Data, LocalBase + BR, LocalBase + BL, FVector(0,1,0), Data.TerritoryColor);
            }
        }
    }

    // Spawn Visuals
    SpawnVisuals();
    if (Configuration->bShowDebugGlobals) DrawDebugVisuals();
}






void AMapGenerator::ClearMap()
{
    // Pulisci linee di debug persistenti precedenti
    FlushPersistentDebugLines(GetWorld());

    // Distruggi tutti gli actor tracciati
    for (ATerritoryActor* Actor : SpawnedTerritories)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    SpawnedTerritories.Empty();
}

void AMapGenerator::DrawDebugVisuals()
{
    if (!Configuration)
    {
        UE_LOG(LogRosikoMapGen, Warning, TEXT("DrawDebugVisuals: Configuration is null"));
        return;
    }

    // A. Gestione Debug Plane (Texture Sottofondo)
    if (DebugPlane)
    {
        // 1. Assicuriamoci che la Mesh sia caricata (Fix per Hot Reload/Blueprint corruption)
        if (!DebugPlane->GetStaticMesh())
        {
             UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
             if (PlaneMesh)
             {
                 DebugPlane->SetStaticMesh(PlaneMesh);
             }
        }

        DebugPlane->SetVisibility(false); // Reset

        // Se abbiamo texture e material debug E il flag è attivo, mostriamolo
        if (Configuration->bShowDebugPlane && Configuration->GenerationMask && Configuration->DebugMaterial)
        {
            // Calcolo Scala: Il Plane è 100x100. La mappa è Size*2 x Size*2.
            // Es. Size=2000 -> Mappa 4000x4000.  Scala = 40.
            float ScaleX = (Configuration->MapSize.X * 2.0f) / 100.0f;
            float ScaleY = (Configuration->MapSize.Y * 2.0f) / 100.0f;

            DebugPlane->SetRelativeScale3D(FVector(ScaleX, ScaleY, 1.0f));
            DebugPlane->SetRelativeLocation(FVector(0,0,-10.0f)); // Abbassato a -50 per sicurezza

            // Texture
            UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(Configuration->DebugMaterial, this);
            if (DynMat)
            {
                DynMat->SetTextureParameterValue(FName("Mask"), Configuration->GenerationMask);
                DebugPlane->SetMaterial(0, DynMat);
                DebugPlane->SetVisibility(true);
                DebugPlane->SetHiddenInGame(false);
                DebugPlane->MarkRenderStateDirty(); // FORZA AGGIORNAMENTO GRAFICO

                UE_LOG(LogRosikoMapGen, Log, TEXT("DrawDebugVisuals: DebugPlane VISIBLE! Scale: %f, %f. Material: %s"), ScaleX, ScaleY, *DynMat->GetName());
            }
            else
            {
                UE_LOG(LogRosikoMapGen, Warning, TEXT("DrawDebugVisuals: Failed to create Dynamic Material Instance"));
            }
        }
        else
        {
            // Nascondi esplicitamente il plane se il flag è disattivato
            DebugPlane->SetVisibility(false);
            DebugPlane->SetHiddenInGame(true);

            if (!Configuration->bShowDebugPlane)
            {
                UE_LOG(LogRosikoMapGen, Log, TEXT("DrawDebugVisuals: DebugPlane hidden (flag disabled)"));
            }
            else
            {
                UE_LOG(LogRosikoMapGen, Warning, TEXT("DrawDebugVisuals: Missing GenerationMask or DebugMaterial in Config"));
            }
        }
    }
    else
    {
        UE_LOG(LogRosikoMapGen, Error, TEXT("DrawDebugVisuals: DebugPlane Component is NULL! Try deleting and re-placing the Actor in the level."));
    }

    // 2. Disegna i punti generati
    for (const FGeneratedTerritory& Data : GeneratedData)
    {
        // Se è Oceano, SKIPPALO. (Voxel mode doesn't really generate Ocean points anyway)
        if (Data.bIsOcean)
        {
            continue;
        }

        // Punti Terra
        FColor C = Data.DebugColor.ToFColor(true);
        DrawDebugPoint(GetWorld(), Data.CenterPoint + FVector(0,0,15), 10.0f, C, true);
    }

    // 3. (Opzionale) Disegna i bounds della mappa totale
    DrawDebugBox(GetWorld(), FVector::ZeroVector, FVector(Configuration->MapSize.X, Configuration->MapSize.Y, 10), FColor::White, true);
}





void AMapGenerator::SpawnVisuals()
{
    if (!Configuration || !Configuration->TerritoryClass)
    {
        UE_LOG(LogRosikoMapGen, Warning, TEXT("AMapGenerator::SpawnVisuals: Configuration or TerritoryClass is not set!"));
        return;
    }

    // Per evitare duplicati se chiamato più volte, ideale sarebbe tracciare gli actor spawnati.
    // Qui spawniamo e basta.


    for (const FGeneratedTerritory& Data : GeneratedData)
    {
        // Non spawnare mesh per l'oceano
        if (Data.bIsOcean) continue;

        FTransform SpawnTransform(FRotator::ZeroRotator, Data.CenterPoint);
        ATerritoryActor* NewActor = GetWorld()->SpawnActor<ATerritoryActor>(Configuration->TerritoryClass, SpawnTransform);

        if (NewActor)
        {
            // Imposta i dati del territorio (per logica di gioco e UI)
            NewActor->SetTerritoryData(Data);

            // OPTIMIZATION: Vertices are already in local space, pass directly without copy
            // This eliminates allocation + transformation of 10k-50k vertices
            NewActor->InitializeMesh(Data);

            // Tracciamo l'actor
            SpawnedTerritories.Add(NewActor);
        }
    }
}

// ============================================================================
// ASYNC GENERATION IMPLEMENTATION
// ============================================================================

void AMapGenerator::GenerateMapAsync()
{
	if (AsyncState != EMapGenerationState::Idle && AsyncState != EMapGenerationState::Complete)
	{
		UE_LOG(LogRosikoMapGen, Warning, TEXT("Generation already in progress! Cancel first."));
		return;
	}

	if (!Configuration || !Configuration->GenerationMask)
	{
		UE_LOG(LogRosikoMapGen, Error, TEXT("Async Gen Aborted: Missing Config or Mask!"));
		return;
	}

	// Auto-adjust Aspect Ratio (same as sync version)
	if (Configuration)
	{
		int32 W = Configuration->GenerationMask->GetSizeX();
		int32 H = Configuration->GenerationMask->GetSizeY();
		if (W > 0 && H > 0)
		{
			float AspectRatio = (float)H / (float)W;
			float NewY = Configuration->MapSize.X * AspectRatio;
			if (!FMath::IsNearlyEqual(Configuration->MapSize.Y, NewY, 1.0f))
			{
				Configuration->MapSize.Y = NewY;
			}
		}
	}

	// Reset state
	ClearMap();
	GeneratedData.Empty();
	RNG.Initialize(MapSeed);
	AsyncState = EMapGenerationState::Initializing;
	AsyncProgress = 0.0f;
	AsyncCurrentStep = 0;
	AsyncSeeds.Empty();
	AsyncUsedNamesPerContinent.Empty();

	// Start timing
	AsyncStartTime = FPlatformTime::Seconds();
	AsyncEndTime = 0.0;

	UE_LOG(LogRosikoMapGen, Log, TEXT("Starting ASYNC map generation with seed %d"), MapSeed);
	UpdateAsyncProgress(0.0f, TEXT("Initializing..."));
}

void AMapGenerator::CancelAsyncGeneration()
{
	if (AsyncState != EMapGenerationState::Idle && AsyncState != EMapGenerationState::Complete)
	{
		UE_LOG(LogRosikoMapGen, Warning, TEXT("Async generation canceled"));
		AsyncState = EMapGenerationState::Idle;
		AsyncProgress = 0.0f;
		UpdateAsyncProgress(0.0f, TEXT("Canceled"));
	}
}

void AMapGenerator::ProcessAsyncTick()
{
	switch (AsyncState)
	{
		case EMapGenerationState::Initializing:
			AsyncStep_Initialize();
			break;

		case EMapGenerationState::PopulatingGrid:
			AsyncStep_PopulateGrid();
			break;

		case EMapGenerationState::GeneratingSeeds:
			AsyncStep_GenerateSeeds();
			break;

		case EMapGenerationState::VoronoiPass:
			AsyncStep_VoronoiIteration();
			break;

		case EMapGenerationState::BuildingGeometry:
			AsyncStep_BuildGeometry();
			break;

		case EMapGenerationState::SpawningVisuals:
			AsyncStep_SpawnVisuals();
			break;

		default:
			break;
	}
}

void AMapGenerator::UpdateAsyncProgress(float NewProgress, const FString& StatusText)
{
	AsyncProgress = FMath::Clamp(NewProgress, 0.0f, 1.0f);
	OnGenerationProgress.Broadcast(AsyncProgress, StatusText);

	UE_LOG(LogRosikoMapGen, Log, TEXT("Generation Progress: %.1f%% - %s"), AsyncProgress * 100.0f, *StatusText);
}

void AMapGenerator::CompleteAsyncGeneration()
{
	AsyncState = EMapGenerationState::Complete;
	AsyncEndTime = FPlatformTime::Seconds();

	double TotalTime = AsyncEndTime - AsyncStartTime;

	UpdateAsyncProgress(1.0f, TEXT("Complete!"));
	OnGenerationComplete.Broadcast();

	UE_LOG(LogRosikoMapGen, Warning, TEXT("========================================"));
	UE_LOG(LogRosikoMapGen, Warning, TEXT("Async map generation COMPLETE!"));
	UE_LOG(LogRosikoMapGen, Warning, TEXT("Total generation time: %.3f seconds (%.0f ms)"), TotalTime, TotalTime * 1000.0);
	UE_LOG(LogRosikoMapGen, Warning, TEXT("Seed: %d | Territories: %d"), MapSeed, GeneratedData.Num());
	UE_LOG(LogRosikoMapGen, Warning, TEXT("========================================"));
}

void AMapGenerator::AsyncStep_Initialize()
{
	// Setup Grid Dimensions
	int32 BaseResolution = Configuration->GridResolution;
	GridSizeX = BaseResolution;
	float AspectRatio = Configuration->MapSize.Y / Configuration->MapSize.X;
	GridSizeY = FMath::RoundToInt(BaseResolution * AspectRatio);

	VoxelGrid.Empty();
	VoxelGrid.Init(FVoxelCell(), GridSizeX * GridSizeY);

	UE_LOG(LogRosikoMapGen, Log, TEXT("Async: Grid initialized %d x %d"), GridSizeX, GridSizeY);

	AsyncState = EMapGenerationState::PopulatingGrid;
	UpdateAsyncProgress(0.1f, TEXT("Grid initialized"));
}

void AMapGenerator::AsyncStep_PopulateGrid()
{
	// Texture Sampling Access
	FTexture2DMipMap* Mip = &Configuration->GenerationMask->GetPlatformData()->Mips[0];
	const uint8* RawData = (const uint8*)Mip->BulkData.LockReadOnly();
	int32 TexSizeX = Configuration->GenerationMask->GetSizeX();
	int32 TexSizeY = Configuration->GenerationMask->GetSizeY();

	auto GetColorAtUV = [&](float U, float V) -> FLinearColor {
		int32 X = FMath::Clamp(FMath::RoundToInt(U * (TexSizeX - 1)), 0, TexSizeX - 1);
		int32 Y = FMath::Clamp(FMath::RoundToInt(V * (TexSizeY - 1)), 0, TexSizeY - 1);
		int32 Index = (Y * TexSizeX + X) * 4;
		return FLinearColor(RawData[Index + 2] / 255.0f, RawData[Index + 1] / 255.0f, RawData[Index + 0] / 255.0f, 1.0f);
	};

	// Auto-detect ocean color
	FLinearColor AutoOceanColor = FLinearColor(RawData[2] / 255.0f, RawData[1] / 255.0f, RawData[0] / 255.0f, 1.0f);

	// Pre-cache continent colors
	TArray<FVector> ContinentColorsVec;
	for(const FContinentDefinition& Cont : Configuration->ContinentSetup)
	{
		ContinentColorsVec.Add(FVector(Cont.Color.R, Cont.Color.G, Cont.Color.B));
	}

	FVector ConfigOceanVec(Configuration->OceanColor.R, Configuration->OceanColor.G, Configuration->OceanColor.B);
	FVector AutoOceanVec(AutoOceanColor.R, AutoOceanColor.G, AutoOceanColor.B);

	// Populate grid (all at once - fast enough)
	for (int32 Y = 0; Y < GridSizeY; Y++)
	{
		for (int32 X = 0; X < GridSizeX; X++)
		{
			float U = (float)X / (float)GridSizeX;
			float V = (float)Y / (float)GridSizeY;

			FLinearColor Sample = GetColorAtUV(U, V);
			FVoxelCell* Cell = GetCell(X, Y);

			FVector SampleVec(Sample.R, Sample.G, Sample.B);

			float DistConfigOcean = FVector::Dist(SampleVec, ConfigOceanVec);
			float DistAutoOcean = FVector::Dist(SampleVec, AutoOceanVec);

			bool bIsBlack = (Sample.R < 0.15f && Sample.G < 0.15f && Sample.B < 0.15f);
			bool bIsWhite = (Sample.R > 0.85f && Sample.G > 0.85f && Sample.B > 0.85f);
			bool bIsTransparent = (Sample.A < 0.2f);

			if (DistConfigOcean < 0.2f || DistAutoOcean < 0.2f || bIsBlack || bIsWhite || bIsTransparent)
			{
				Cell->ContinentIndex = -1;
				continue;
			}

			int32 BestCont = -1;
			float MinDistSq = FLT_MAX;
			float AcceptanceThresholdSq = Configuration->ContinentColorThreshold;

			for (int32 c = 0; c < ContinentColorsVec.Num(); c++)
			{
				float DistSq = FVector::DistSquared(SampleVec, ContinentColorsVec[c]);
				if (DistSq < MinDistSq && DistSq < AcceptanceThresholdSq)
				{
					MinDistSq = DistSq;
					BestCont = c;
				}
			}

			if (BestCont != -1)
			{
				Cell->ContinentIndex = BestCont;
			}
		}
	}

	Mip->BulkData.Unlock();

	AsyncState = EMapGenerationState::GeneratingSeeds;
	UpdateAsyncProgress(0.3f, TEXT("Continents assigned"));
}

void AMapGenerator::AsyncStep_GenerateSeeds()
{
	// Generate territory seeds (same as sync version)
	AsyncSeeds.Empty();
	int32 GlobalID = 0;

	for (int32 c = 0; c < Configuration->ContinentSetup.Num(); c++)
	{
		const FContinentDefinition& Cont = Configuration->ContinentSetup[c];
		int32 TargetCount = Cont.TerritoryCount;

		TArray<int32> ValidIndices;
		for(int32 i=0; i<VoxelGrid.Num(); i++)
		{
			if (VoxelGrid[i].ContinentIndex == c) ValidIndices.Add(i);
		}

		if (ValidIndices.Num() == 0) continue;

		for(int32 k=0; k<TargetCount; k++)
		{
			if (ValidIndices.Num() == 0) break;

			int32 VisitIdx = RNG.RandRange(0, ValidIndices.Num()-1);
			int32 CellIdx = ValidIndices[VisitIdx];

			int32 EX = CellIdx % GridSizeX;
			int32 EY = CellIdx / GridSizeX;

			float TerritoryBaseHeight = RNG.FRandRange(Configuration->TerritoryHeightRange.X, Configuration->TerritoryHeightRange.Y);

			FGridSeed NewSeed;
			NewSeed.ID = GlobalID++;
			NewSeed.ContIndex = c;
			NewSeed.X = EX;
			NewSeed.Y = EY;
			NewSeed.BaseHeight = TerritoryBaseHeight;
			AsyncSeeds.Add(NewSeed);

			GetCell(EX, EY)->TerritoryID = NewSeed.ID;
		}
	}

	// Initialize Jump Flood
	for(const FGridSeed& S : AsyncSeeds)
	{
		FVoxelCell* SeedCell = GetCell(S.X, S.Y);
		if (SeedCell)
		{
			SeedCell->ClosestSeed = &S;
		}
	}

	int32 MaxDimension = FMath::Max(GridSizeX, GridSizeY);
	AsyncCurrentJumpSize = FMath::RoundUpToPowerOfTwo(MaxDimension) / 2;

	AsyncState = EMapGenerationState::VoronoiPass;
	UpdateAsyncProgress(0.4f, FString::Printf(TEXT("Seeds generated: %d territories"), AsyncSeeds.Num()));
}

void AMapGenerator::AsyncStep_VoronoiIteration()
{
	// Execute ONE Jump Flood iteration per tick
	if (AsyncCurrentJumpSize >= 1)
	{
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			for (int32 X = 0; X < GridSizeX; X++)
			{
				FVoxelCell* Cell = GetCell(X, Y);
				if (!Cell || Cell->ContinentIndex == -1) continue;

				const FGridSeed* CurrentBest = Cell->ClosestSeed;
				float CurrentBestDistSq = FLT_MAX;

				if (CurrentBest)
				{
					CurrentBestDistSq = (float)(FMath::Square(CurrentBest->X - X) + FMath::Square(CurrentBest->Y - Y));
				}

				for(int32 DY = -1; DY <= 1; DY++)
				{
					for(int32 DX = -1; DX <= 1; DX++)
					{
						int32 CheckX = X + DX * AsyncCurrentJumpSize;
						int32 CheckY = Y + DY * AsyncCurrentJumpSize;

						FVoxelCell* Neighbor = GetCell(CheckX, CheckY);
						if (!Neighbor || !Neighbor->ClosestSeed) continue;

						if (Neighbor->ClosestSeed->ContIndex != Cell->ContinentIndex) continue;

						float DistToNeighborSeedSq = (float)(
							FMath::Square(Neighbor->ClosestSeed->X - X) +
							FMath::Square(Neighbor->ClosestSeed->Y - Y)
						);

						if (DistToNeighborSeedSq < CurrentBestDistSq)
						{
							CurrentBestDistSq = DistToNeighborSeedSq;
							CurrentBest = Neighbor->ClosestSeed;
						}
					}
				}

				Cell->ClosestSeed = CurrentBest;
			}
		}

		AsyncCurrentJumpSize /= 2;

		// Calculate progress (Voronoi is 40% - 60% of total)
		int32 MaxDimension = FMath::Max(GridSizeX, GridSizeY);
		int32 InitialJump = FMath::RoundUpToPowerOfTwo(MaxDimension) / 2;
		float VoronoiProgress = 1.0f - ((float)AsyncCurrentJumpSize / (float)InitialJump);
		float OverallProgress = 0.4f + (VoronoiProgress * 0.2f);

		UpdateAsyncProgress(OverallProgress, FString::Printf(TEXT("Voronoi iteration (jump: %d)"), AsyncCurrentJumpSize));
	}
	else
	{
		// Voronoi complete, assign TerritoryID and Height
		for (int32 Y = 0; Y < GridSizeY; Y++)
		{
			for (int32 X = 0; X < GridSizeX; X++)
			{
				FVoxelCell* Cell = GetCell(X, Y);
				if (!Cell || Cell->ContinentIndex == -1 || !Cell->ClosestSeed) continue;

				Cell->TerritoryID = Cell->ClosestSeed->ID;
				float CellVariation = RNG.FRandRange(Configuration->CellHeightRange.X, Configuration->CellHeightRange.Y);
				Cell->Height = Cell->ClosestSeed->BaseHeight + CellVariation;
			}
		}

		AsyncState = EMapGenerationState::BuildingGeometry;
		AsyncCurrentGeometryIndex = 0;
		UpdateAsyncProgress(0.6f, TEXT("Voronoi complete, building geometry..."));
	}
}

void AMapGenerator::AsyncStep_BuildGeometry()
{
	// Build geometry for ALL territories at once (one frame)
	// Alternative: split into chunks if too slow

	GeneratedData.AddDefaulted(AsyncSeeds.Num());

	// Init names tracking
	AsyncUsedNamesPerContinent.Empty();
	for (int32 c = 0; c < Configuration->ContinentSetup.Num(); c++)
	{
		AsyncUsedNamesPerContinent.Add(c, TArray<FString>());
	}

	// Init basic info
	for(const FGridSeed& S : AsyncSeeds)
	{
		if (S.ID < GeneratedData.Num())
		{
			GeneratedData[S.ID].ID = S.ID;
			GeneratedData[S.ID].ContinentID = S.ContIndex;

			FLinearColor BaseColor = Configuration->ContinentSetup[S.ContIndex].Color;
			GeneratedData[S.ID].DebugColor = BaseColor;

			float BrightnessMultiplier = RNG.FRandRange(Configuration->TerritoryBrightnessRange.X, Configuration->TerritoryBrightnessRange.Y);
			FLinearColor TerritoryColor = BaseColor * BrightnessMultiplier;
			TerritoryColor.A = 1.0f;
			GeneratedData[S.ID].TerritoryColor = TerritoryColor;

			GeneratedData[S.ID].bIsOcean = false;

			// Assign name
			const FContinentDefinition& Continent = Configuration->ContinentSetup[S.ContIndex];
			FString TerritoryName;

			if (Continent.TerritoryNames.Num() > 0)
			{
				TArray<FString> AvailableNames;
				for (const FString& Name : Continent.TerritoryNames)
				{
					if (!AsyncUsedNamesPerContinent[S.ContIndex].Contains(Name))
					{
						AvailableNames.Add(Name);
					}
				}

				if (AvailableNames.Num() > 0)
				{
					int32 RandomIndex = RNG.RandRange(0, AvailableNames.Num() - 1);
					TerritoryName = AvailableNames[RandomIndex];
					AsyncUsedNamesPerContinent[S.ContIndex].Add(TerritoryName);
				}
				else
				{
					int32 Suffix = AsyncUsedNamesPerContinent[S.ContIndex].Num() - Continent.TerritoryNames.Num() + 1;
					TerritoryName = FString::Printf(TEXT("%s %d"), *Continent.Name, Suffix);
					AsyncUsedNamesPerContinent[S.ContIndex].Add(TerritoryName);
				}
			}
			else
			{
				TerritoryName = FString::Printf(TEXT("%s Territory %d"), *Continent.Name, S.ID);
			}

			GeneratedData[S.ID].Name = TerritoryName;

			float WorldX = (float)S.X / GridSizeX * (Configuration->MapSize.X*2) - Configuration->MapSize.X;
			float WorldY = (float)S.Y / GridSizeY * (Configuration->MapSize.Y*2) - Configuration->MapSize.Y;
			GeneratedData[S.ID].CenterPoint = FVector(WorldX, WorldY, S.BaseHeight);
		}
	}

	// Build mesh geometry
	float CellW = (Configuration->MapSize.X * 2.0f) / (float)GridSizeX;
	float CellH = (Configuration->MapSize.Y * 2.0f) / (float)GridSizeY;
	float HalfW = 0.5f * CellW;
	float HalfH = 0.5f * CellH;

	for (int32 Y = 0; Y < GridSizeY; Y++)
	{
		for (int32 X = 0; X < GridSizeX; X++)
		{
			FVoxelCell* Cell = GetCell(X, Y);
			if (!Cell || Cell->TerritoryID == -1) continue;

			FGeneratedTerritory& Data = GeneratedData[Cell->TerritoryID];

			float WX = (float)X * CellW - Configuration->MapSize.X + HalfW;
			float WY = (float)Y * CellH - Configuration->MapSize.Y + HalfH;
			float CellHeight = Cell->Height;

			FVector TerritoryCenter = Data.CenterPoint;
			FVector LocalBase(WX - TerritoryCenter.X, WY - TerritoryCenter.Y, -TerritoryCenter.Z);

			// Top face
			FVector TL(-HalfW, -HalfH, CellHeight);
			FVector TR( HalfW, -HalfH, CellHeight);
			FVector BL(-HalfW,  HalfH, CellHeight);
			FVector BR( HalfW,  HalfH, CellHeight);

			int32 I = Data.Vertices.Num();
			Data.Vertices.Add(LocalBase + TL);
			Data.Vertices.Add(LocalBase + TR);
			Data.Vertices.Add(LocalBase + BR);
			Data.Vertices.Add(LocalBase + BL);

			FVector UpNormal(0,0,1);
			Data.Normals.Add(UpNormal); Data.Normals.Add(UpNormal);
			Data.Normals.Add(UpNormal); Data.Normals.Add(UpNormal);

			FLinearColor TerritoryCol = Data.TerritoryColor;
			Data.VertexColors.Add(TerritoryCol); Data.VertexColors.Add(TerritoryCol);
			Data.VertexColors.Add(TerritoryCol); Data.VertexColors.Add(TerritoryCol);

			Data.Triangles.Add(I + 0);
			Data.Triangles.Add(I + 2);
			Data.Triangles.Add(I + 1);

			Data.Triangles.Add(I + 0);
			Data.Triangles.Add(I + 3);
			Data.Triangles.Add(I + 2);

			// Side faces
			FVoxelCell* Right = GetCell(X + 1, Y);
			bool bNeedRightFace = !Right || Right->TerritoryID != Cell->TerritoryID || FMath::Abs(Right->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
			if (bNeedRightFace)
			{
				AddQuadFace(Data, LocalBase + TR, LocalBase + BR, FVector(1,0,0), Data.TerritoryColor);
			}

			FVoxelCell* Left = GetCell(X - 1, Y);
			bool bNeedLeftFace = !Left || Left->TerritoryID != Cell->TerritoryID || FMath::Abs(Left->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
			if (bNeedLeftFace)
			{
				AddQuadFace(Data, LocalBase + BL, LocalBase + TL, FVector(-1,0,0), Data.TerritoryColor);
			}

			FVoxelCell* Top = GetCell(X, Y - 1);
			bool bNeedTopFace = !Top || Top->TerritoryID != Cell->TerritoryID || FMath::Abs(Top->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
			if (bNeedTopFace)
			{
				AddQuadFace(Data, LocalBase + TL, LocalBase + TR, FVector(0,-1,0), Data.TerritoryColor);
			}

			FVoxelCell* Bot = GetCell(X, Y + 1);
			bool bNeedBotFace = !Bot || Bot->TerritoryID != Cell->TerritoryID || FMath::Abs(Bot->Height - CellHeight) > Configuration->HeightDifferenceThreshold;
			if (bNeedBotFace)
			{
				AddQuadFace(Data, LocalBase + BR, LocalBase + BL, FVector(0,1,0), Data.TerritoryColor);
			}
		}
	}

	AsyncState = EMapGenerationState::SpawningVisuals;
	AsyncCurrentSpawnIndex = 0;
	UpdateAsyncProgress(0.8f, TEXT("Geometry built, spawning territories..."));
}

void AMapGenerator::AsyncStep_SpawnVisuals()
{
	// Spawn territories in chunks (e.g., 10 per frame)
	const int32 TerritoriesPerFrame = 10;
	int32 EndIndex = FMath::Min(AsyncCurrentSpawnIndex + TerritoriesPerFrame, GeneratedData.Num());

	for (int32 i = AsyncCurrentSpawnIndex; i < EndIndex; i++)
	{
		const FGeneratedTerritory& Data = GeneratedData[i];

		if (Data.bIsOcean) continue;

		FTransform SpawnTransform(FRotator::ZeroRotator, Data.CenterPoint);
		ATerritoryActor* NewActor = GetWorld()->SpawnActor<ATerritoryActor>(Configuration->TerritoryClass, SpawnTransform);

		if (NewActor)
		{
			NewActor->SetTerritoryData(Data);
			NewActor->InitializeMesh(Data);
			SpawnedTerritories.Add(NewActor);
		}
	}

	AsyncCurrentSpawnIndex = EndIndex;

	float SpawnProgress = (float)AsyncCurrentSpawnIndex / (float)GeneratedData.Num();
	float OverallProgress = 0.8f + (SpawnProgress * 0.2f);

	UpdateAsyncProgress(OverallProgress, FString::Printf(TEXT("Spawning: %d/%d"), AsyncCurrentSpawnIndex, GeneratedData.Num()));

	// Check if done
	if (AsyncCurrentSpawnIndex >= GeneratedData.Num())
	{
		// Draw debug visuals (if enabled)
		if (Configuration->bShowDebugGlobals)
		{
			DrawDebugVisuals();
		}

		CompleteAsyncGeneration();
	}
}
