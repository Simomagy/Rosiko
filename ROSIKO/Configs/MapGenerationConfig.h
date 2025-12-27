#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MapGenerationConfig.generated.h"

USTRUCT(BlueprintType)
struct FContinentDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color = FLinearColor::Red;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TerritoryCount = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ColorTolerance = 0.1f;

    // Pool di nomi possibili per i territori di questo continente
    // Durante la generazione, verrà pescato un nome random da questo array
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> TerritoryNames = {
        TEXT("Northern Plains"),
        TEXT("Southern Coast"),
        TEXT("Eastern Border"),
        TEXT("Western Highlands"),
        TEXT("Central Valley"),
        TEXT("Mountain Pass"),
        TEXT("Forest Region"),
        TEXT("Coastal Bay"),
        TEXT("River Delta"),
        TEXT("Desert Frontier")
    };
};

/**
 * Configurazione per la generazione della mappa.
 */
UCLASS(BlueprintType)
class ROSIKO_API UMapGenerationConfig : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Quanti territori totali (fallback se non usiamo setup continenti)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 NumTerritories = 70;

    // Dimensione del mondo di gioco (es. 2000x2000 unità)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    FVector2D MapSize = FVector2D(2000, 2000);

    // Texture ID Map: Colori diversi per ogni continente.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    UTexture2D* GenerationMask;

    // Setup dei continenti (Colore -> Numero Territori)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    TArray<FContinentDefinition> ContinentSetup;

    // Colore considerato Oceano (dove NON generare terra)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    FLinearColor OceanColor = FLinearColor::White;

    // Materiale per debuggare la texture
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    UMaterialInterface* DebugMaterial;

    // Mostra/Nascondi TUTTI i debug (Linee, Punti, ecc.) in viewport
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugGlobals = true;

    // Mostra/Nascondi il Debug Plane (texture sottofondo)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugPlane = true;

    // Risoluzione della griglia Voxel (es. 200 = griglia 200xAspectRatio).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation")
    int32 GridResolution = 200;

    // Il Blueprint visivo da spawnare per ogni territorio
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    TSubclassOf<AActor> TerritoryClass;

    // Range variazione altezza per territorio (ogni territorio avrà un offset Z casuale in questo range)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
    FVector2D TerritoryHeightRange = FVector2D(0.0f, 50.0f);

    // Range variazione altezza per singola cella (ogni cubetto avrà un offset aggiuntivo in questo range)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
    FVector2D CellHeightRange = FVector2D(-5.0f, 5.0f);

    // Range di variazione della luminosità per ogni territorio (0-1, dove 1.0 = colore pieno, 0.5 = 50% più scuro)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FVector2D TerritoryBrightnessRange = FVector2D(0.6f, 0.9f);

    // Soglia di distanza colore per riconoscimento continenti (squared distance, default 0.15)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation", meta = (ClampMin = "0.0", ClampMax = "3.0"))
    float ContinentColorThreshold = 0.15f;

    // Tolleranza per differenza di altezza tra celle (per generare facce laterali)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float HeightDifferenceThreshold = 0.01f;

    // Usa Jump Flood Algorithm per Voronoi (più veloce per griglie grandi [GridResolution > 1000 e NumTerritories > 200])
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
    bool bUseJumpFloodAlgorithm = true;
};
