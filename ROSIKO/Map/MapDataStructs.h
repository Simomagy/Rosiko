#pragma once

#include "CoreMinimal.h"
#include "MapDataStructs.generated.h"



/**
 * Contiene i dati puri matematici di un territorio generato.
 */
USTRUCT(BlueprintType)
struct FGeneratedTerritory
{
    GENERATED_BODY()

    // ID univoco del territorio (0-69)
    UPROPERTY(BlueprintReadOnly)
    int32 ID;

    // Nome del territorio (pescato dal pool del continente)
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    // Se true, questo è un territorio "fantasma" che serve solo a dare forma alle coste, e non verrà spawnato.
    UPROPERTY(BlueprintReadOnly)
    bool bIsOcean = false;

    // Il punto centrale (utile per posizionare truppe o icone)
    UPROPERTY(BlueprintReadOnly)
    FVector CenterPoint;

    // Colore assegnato (o ID del continente)
    UPROPERTY(BlueprintReadOnly)
    FLinearColor DebugColor;

    // Colore specifico del territorio (variazione del colore del continente)
    UPROPERTY(BlueprintReadOnly)
    FLinearColor TerritoryColor;

    // I vertici che formano il perimetro del territorio (per disegnare la mesh)
    UPROPERTY(BlueprintReadOnly)
    TArray<FVector> Vertices;

    // Indici dei triangoli (opzionale, se generati dal C++)
    // Se popolato, passare questo a "Create Mesh Section" invece di usare algoritmi custom nel BP.
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> Triangles;

    // Normali dei vertici (opzionale, per shading corretto)
    UPROPERTY(BlueprintReadOnly)
    TArray<FVector> Normals;

    // Colori dei vertici (per colorare la mesh procedurale)
    UPROPERTY(BlueprintReadOnly)
    TArray<FLinearColor> VertexColors;

    // ID dei territori confinanti (per la logica di movimento/attacco)
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> NeighborIDs;

    // === PLACEHOLDER PER LOGICA DI GIOCO (Rosiko) ===

    // Numero di truppe presenti nel territorio
    UPROPERTY(BlueprintReadOnly)
    int32 Troops = 0;

    // ID del giocatore proprietario (-1 = neutrale)
    UPROPERTY(BlueprintReadOnly)
    int32 OwnerID = -1;

    // Se true, questo è una capitale/obiettivo speciale
    UPROPERTY(BlueprintReadOnly)
    bool bIsCapital = false;

    // Bonus armata garantito dal controllo del continente
    UPROPERTY(BlueprintReadOnly)
    int32 ContinentBonusArmies = 0;

    // ID del continente di appartenenza
    UPROPERTY(BlueprintReadOnly)
    int32 ContinentID = -1;
};
