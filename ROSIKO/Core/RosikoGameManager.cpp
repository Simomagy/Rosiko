#include "RosikoGameManager.h"
#include "RosikoGameState.h"
#include "RosikoPlayerState.h"
#include "../Map/MapGenerator.h"
#include "../Map/Territory/TerritoryActor.h"
#include "../Troop/UI/TroopDisplayComponent.h"
#include "../Troop/UI/TroopVisualManager.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoGameManager, Log, All);

ARosikoGameManager::ARosikoGameManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// Abilita replicazione per multiplayer
	bReplicates = true;
	bAlwaysRelevant = true; // Visibile a tutti i client
}

void ARosikoGameManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// NOTA: Replicazione ora gestita da ARosikoGameState e ARosikoPlayerState
	// Nessuna proprietà da replicare qui
}

// DEPRECATO: Ora gestito da ARosikoGameState::OnRep_CurrentPhase
// void ARosikoGameManager::OnRep_CurrentPhase() { ... }

// DEPRECATO: Ora gestito da ARosikoGameState::OnRep_CurrentPlayerTurn
// void ARosikoGameManager::OnRep_CurrentPlayerTurn() { ... }

void ARosikoGameManager::Multicast_NotifyColorSelectionTurn_Implementation(int32 PlayerID, const TArray<FLinearColor>& AvailableColorsList)
{
	// Questa funzione viene eseguita su TUTTI i client (incluso il server)
	// Ma vogliamo che solo i CLIENT la eseguano (non il server, che ha già fatto il broadcast locale)
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Client: Received Multicast_NotifyColorSelectionTurn for Player %d"), PlayerID);
		OnColorSelectionRequired.Broadcast(PlayerID, AvailableColorsList);
	}
}

void ARosikoGameManager::Multicast_NotifyTerritoryUpdate_Implementation(int32 TerritoryID, int32 OwnerID, int32 TroopCount)
{
	// Eseguito su TUTTI i client (e server)
	// Trova TerritoryActor e aggiorna visual localmente
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return;

	for (TActorIterator<ATerritoryActor> It(GetWorld()); It; ++It)
	{
		if (It->GetTerritoryData().ID == TerritoryID)
		{
			FLinearColor OwnerColor = FLinearColor::Gray;

			if (OwnerID >= 0)
			{
				ARosikoPlayerState* PS = GetRosikoPlayerState(OwnerID);
				if (PS)
				{
					OwnerColor = PS->ArmyColor;
				}
			}

			// Aggiorna visual con nuovo sistema
			if (It->TroopVisualManager)
			{
				It->TroopVisualManager->UpdateTroopDisplay(TroopCount, OwnerColor);
			}
			// Fallback a legacy
			else if (It->TroopDisplay)
			{
				It->TroopDisplay->UpdateDisplay(TroopCount, OwnerColor);
				It->TroopDisplay->SetDisplayVisible(TroopCount > 0);
			}

			UE_LOG(LogRosikoGameManager, Verbose, TEXT("Multicast update Territory %d: Owner=%d, Troops=%d"),
			       TerritoryID, OwnerID, TroopCount);
			break;
		}
	}
}

void ARosikoGameManager::BeginPlay()
{
	Super::BeginPlay();

	// NOTE: Non chiamiamo StartGame() automaticamente qui.
	// StartGame() deve essere chiamato DOPO che MapGenerator ha generato la mappa.
	// Questo può essere fatto manualmente da Blueprint o UI.
}

void ARosikoGameManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cleanup timer
	if (GetWorld() && GameTimeUpdateTimer.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(GameTimeUpdateTimer);
		UE_LOG(LogRosikoGameManager, Log, TEXT("Game time update timer cleared"));
	}

	Super::EndPlay(EndPlayReason);
}

// === HELPER METHODS PER ACCESSO GAMESTATE/PLAYERSTATE ===

ARosikoGameState* ARosikoGameManager::GetRosikoGameState() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("GetRosikoGameState - World is null!"));
		return nullptr;
	}

	ARosikoGameState* GS = World->GetGameState<ARosikoGameState>();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("GetRosikoGameState - GameState is not ARosikoGameState!"));
	}
	return GS;
}

ARosikoPlayerState* ARosikoGameManager::GetRosikoPlayerState(int32 PlayerID) const
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return nullptr;

	// Itera su tutti i PlayerStates nel GameState
	for (APlayerState* PS : GS->PlayerArray)
	{
		ARosikoPlayerState* RPS = Cast<ARosikoPlayerState>(PS);
		if (RPS && RPS->GameManagerPlayerID == PlayerID)
		{
			return RPS;
		}
	}

	UE_LOG(LogRosikoGameManager, Warning, TEXT("GetRosikoPlayerState - PlayerID %d not found in GameState!"), PlayerID);
	return nullptr;
}

TArray<ARosikoPlayerState*> ARosikoGameManager::GetAllPlayerStates() const
{
	TArray<ARosikoPlayerState*> Result;
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return Result;

	for (APlayerState* PS : GS->PlayerArray)
	{
		ARosikoPlayerState* RPS = Cast<ARosikoPlayerState>(PS);
		if (RPS && RPS->GameManagerPlayerID >= 0)
		{
			Result.Add(RPS);
		}
	}

	return Result;
}

TArray<FLinearColor> ARosikoGameManager::GetAvailableColors() const
{
	ARosikoGameState* GS = GetRosikoGameState();
	return GS ? GS->AvailableColors : TArray<FLinearColor>();
}

void ARosikoGameManager::StartGame()
{
	UE_LOG(LogRosikoGameManager, Warning, TEXT("StartGame() called - HasAuthority: %s"),
	       HasAuthority() ? TEXT("TRUE (Server)") : TEXT("FALSE (Client)"));

	if (!GameRules)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("GameRules is null! Assign a GameRulesConfig Data Asset in the Details panel."));
		return;
	}

	if (!GameRules->IsValidPlayerCount(NumPlayers))
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("Invalid player count: %d. ROSIKO requires 3-10 players."), NumPlayers);
		return;
	}

	// 1. Trova MapGenerator nel livello
	for (TActorIterator<AMapGenerator> It(GetWorld()); It; ++It)
	{
		MapGenerator = *It;
		break;
	}

	if (!MapGenerator)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("MapGenerator not found in level! Place AMapGenerator in the level and generate map first."));
		return;
	}

	// 2. Verifica che mappa sia stata generata
	if (MapGenerator->GetGeneratedTerritories().Num() == 0)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("Map not generated yet! Call MapGenerator->GenerateMap() first."));
		return;
	}

	UE_LOG(LogRosikoGameManager, Warning, TEXT("Initializing game components..."));

	// 3. Inizializza RNG con stesso seed del MapGenerator (per determinismo)
	GameRNG.Initialize(MapGenerator->MapSeed + 1000); // +1000 per evitare overlap con generazione mappa

	// 4. Inizializza componenti gioco
	InitializePlayers();
	InitializeTerritories();
	InitializeCardDeck();
	InitializeTurnOrder();

	UE_LOG(LogRosikoGameManager, Warning, TEXT("Changing phase to ColorSelection and starting color selection..."));

	// 5. Avvia fase selezione colori
	ChangePhase(EGamePhase::ColorSelection);
	StartColorSelection();

	UE_LOG(LogRosikoGameManager, Warning, TEXT("Game Started! %d players, Phase: Color Selection"), NumPlayers);
}

void ARosikoGameManager::InitializePlayers()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("InitializePlayers - GameState is null!"));
		return;
	}

	int32 TroopsPerPlayer = GameRules->GetTroopsForPlayerCount(NumPlayers);

	// Inizializza PlayerStates (già creati dal GameMode in PostLogin)
	TArray<ARosikoPlayerState*> PlayerStates = GetAllPlayerStates();

	for (ARosikoPlayerState* PS : PlayerStates)
	{
		if (!PS) continue;

		// Imposta truppe iniziali
		PS->TroopsToPlace = TroopsPerPlayer;
		PS->bIsAlive = true;
		PS->bIsAI = false; // TODO: Implementare AI in futuro
		PS->CardExchangeCount = 0;
		PS->bHasSelectedColor = false;
		PS->ArmyColor = FLinearColor::White; // Placeholder - verrà scelto in fase ColorSelection

		UE_LOG(LogRosikoGameManager, Log, TEXT("Initialized Player %d (%s) - %d troops"),
		       PS->GameManagerPlayerID, *PS->GetPlayerName(), TroopsPerPlayer);
	}

	// Inizializza colori disponibili in GameState
	GS->AvailableColors = PlayerColors;

	UE_LOG(LogRosikoGameManager, Log, TEXT("Initialized %d players, %d troops each, %d colors available"),
	       PlayerStates.Num(), TroopsPerPlayer, GS->AvailableColors.Num());
}

void ARosikoGameManager::InitializeTerritories()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("InitializeTerritories - GameState is null!"));
		return;
	}

	GS->Territories.Empty();

	const TArray<FGeneratedTerritory>& GenTerritories = MapGenerator->GetGeneratedTerritories();

	for (const FGeneratedTerritory& GenTerritory : GenTerritories)
	{
		if (GenTerritory.bIsOcean) continue; // Salta oceani

		FTerritoryGameState NewState;
		NewState.TerritoryID = GenTerritory.ID;
		NewState.OwnerID = -1; // Neutrale inizialmente
		NewState.Troops = 0;

		GS->Territories.Add(NewState);
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Initialized %d territories"), GS->Territories.Num());
}

void ARosikoGameManager::InitializeTurnOrder()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("InitializeTurnOrder - GameState is null!"));
		return;
	}

	GS->TurnOrder.Empty();

	// Crea array sequenziale di PlayerID
	for (int32 i = 0; i < NumPlayers; i++)
	{
		GS->TurnOrder.Add(i);
	}

	// Shuffle random (Fisher-Yates) usando GameRNG per determinismo
	int32 LastIndex = GS->TurnOrder.Num() - 1;
	for (int32 i = LastIndex; i > 0; i--)
	{
		int32 RandomIndex = GameRNG.RandRange(0, i);
		GS->TurnOrder.Swap(i, RandomIndex);
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Turn order randomized: %s"),
	       *FString::JoinBy(GS->TurnOrder, TEXT(", "), [](int32 ID) { return FString::FromInt(ID); }));
}

void ARosikoGameManager::InitializeCardDeck()
{
	CardDeck.Empty();

	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("InitializeCardDeck - GameState is null!"));
		return;
	}

	// Ottieni lista territori validi (escludi oceani)
	TArray<int32> TerritoryIDs;
	for (const FTerritoryGameState& Territory : GS->Territories)
	{
		TerritoryIDs.Add(Territory.TerritoryID);
	}

	if (TerritoryIDs.Num() != GameRules->TotalTerritories)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Territory count mismatch: Expected %d, got %d"),
		       GameRules->TotalTerritories, TerritoryIDs.Num());
	}

	// Crea mazzo seguendo regole ROSIKO: 23 Fanti, 24 Cavalli, 24 Cannoni, 3 Jolly (70 totali)
	int32 CardIndex = 0;

	// Fanti (23)
	for (int32 i = 0; i < GameRules->NumInfantryCards && CardIndex < TerritoryIDs.Num(); i++, CardIndex++)
	{
		FTerritoryCard Card;
		Card.TerritoryID = TerritoryIDs[CardIndex];
		Card.CardType = ETerritoryCardType::Infantry;
		CardDeck.Add(Card);
	}

	// Cavalli (24)
	for (int32 i = 0; i < GameRules->NumCavalryCards && CardIndex < TerritoryIDs.Num(); i++, CardIndex++)
	{
		FTerritoryCard Card;
		Card.TerritoryID = TerritoryIDs[CardIndex];
		Card.CardType = ETerritoryCardType::Cavalry;
		CardDeck.Add(Card);
	}

	// Cannoni (24)
	for (int32 i = 0; i < GameRules->NumArtilleryCards && CardIndex < TerritoryIDs.Num(); i++, CardIndex++)
	{
		FTerritoryCard Card;
		Card.TerritoryID = TerritoryIDs[CardIndex];
		Card.CardType = ETerritoryCardType::Artillery;
		CardDeck.Add(Card);
	}

	// Jolly (3) - non hanno territorio associato
	for (int32 i = 0; i < GameRules->NumJollyCards; i++)
	{
		FTerritoryCard Card;
		Card.TerritoryID = -1; // Jolly universale
		Card.CardType = ETerritoryCardType::Jolly;
		CardDeck.Add(Card);
	}

	// Shuffle mazzo
	int32 LastIndex = CardDeck.Num() - 1;
	for (int32 i = LastIndex; i > 0; i--)
	{
		int32 RandomIndex = GameRNG.RandRange(0, i);
		CardDeck.Swap(i, RandomIndex);
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Card deck initialized: %d cards"), CardDeck.Num());
}

void ARosikoGameManager::DistributeInitialTerritories()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("DistributeInitialTerritories - GameState is null!"));
		return;
	}

	// Crea lista territori disponibili
	TArray<int32> AvailableTerritoryIDs;
	for (const FTerritoryGameState& Territory : GS->Territories)
	{
		AvailableTerritoryIDs.Add(Territory.TerritoryID);
	}

	// Shuffle random (deterministico tramite GameRNG)
	int32 LastIndex = AvailableTerritoryIDs.Num() - 1;
	for (int32 i = LastIndex; i > 0; i--)
	{
		int32 RandomIndex = GameRNG.RandRange(0, i);
		AvailableTerritoryIDs.Swap(i, RandomIndex);
	}

	// Distribuisci round-robin tra giocatori seguendo TurnOrder
	int32 TurnIndex = 0;
	for (int32 TerritoryID : AvailableTerritoryIDs)
	{
		int32 PlayerID = GS->TurnOrder[TurnIndex];
		ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
		if (!PS) continue;

		FTerritoryGameState* Territory = GS->GetTerritory(TerritoryID);
		if (!Territory) continue;

		Territory->OwnerID = PlayerID;
		Territory->Troops = 1; // Piazza 1 carro iniziale per "reclamare" il territorio

		PS->AddTerritory(TerritoryID);
		PS->RemoveTroops(1); // Consuma 1 carro

		BroadcastTerritoryUpdate(TerritoryID);

		TurnIndex = (TurnIndex + 1) % GS->TurnOrder.Num();
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Territories distributed. Players have remaining troops to place."));

	// Log carri rimanenti per player (in ordine TurnOrder)
	for (int32 PlayerID : GS->TurnOrder)
	{
		ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
		if (!PS) continue;

		UE_LOG(LogRosikoGameManager, Log, TEXT("  %s: %d territories, %d troops remaining"),
		       *PS->GetPlayerName(), PS->GetNumTerritoriesOwned(), PS->TroopsToPlace);
	}

	// Forza refresh di tutti i display per essere sicuri che siano visibili
	RefreshAllTerritoryDisplays();
}

void ARosikoGameManager::StartColorSelection()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("StartColorSelection - GameState is null!"));
		return;
	}

	if (!GS->TurnOrder.IsValidIndex(0))
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("TurnOrder is empty! Cannot start color selection."));
		return;
	}

	// Imposta primo player in ordine randomizzato
	GS->CurrentPlayerTurn = 0; // Index in TurnOrder, non PlayerID
	int32 FirstPlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];

	ARosikoPlayerState* FirstPS = GetRosikoPlayerState(FirstPlayerID);
	FString FirstPlayerName = FirstPS ? FirstPS->GetPlayerName() : TEXT("Unknown");

	UE_LOG(LogRosikoGameManager, Warning, TEXT("StartColorSelection() - First player: %s (ID: %d), HasAuthority: %s"),
	       *FirstPlayerName, FirstPlayerID, HasAuthority() ? TEXT("TRUE") : TEXT("FALSE"));

	// Notifica sul SERVER (chiamata locale)
	UE_LOG(LogRosikoGameManager, Warning, TEXT("Server: Broadcasting OnColorSelectionRequired for Player %d"), FirstPlayerID);
	OnColorSelectionRequired.Broadcast(FirstPlayerID, GS->AvailableColors);

	// Multicast RPC per notificare i client
	Multicast_NotifyColorSelectionTurn(FirstPlayerID, GS->AvailableColors);
}

void ARosikoGameManager::StartInitialPlacement()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("StartInitialPlacement - GameState is null!"));
		return;
	}

	// Avvia timer di gioco (persistente a crash/riconnessioni)
	GS->StartGameTimer();

	// Setup timer per aggiornare GameTimeSeconds ogni secondo
	if (GetWorld() && !GameTimeUpdateTimer.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(
			GameTimeUpdateTimer,
			[this, GS]()
			{
				if (GS)
				{
					GS->UpdateGameTime();
				}
			},
			1.0f,  // Ogni secondo
			true   // Loop
		);
		UE_LOG(LogRosikoGameManager, Log, TEXT("Game time update timer started (1s interval)"));
	}

	// Mostra UI per player corrente: "Piazza X carri rimanenti"
	// Il player clicca sui suoi territori per aggiungere carri

	if (GS->TurnOrder.IsValidIndex(GS->CurrentPlayerTurn))
	{
		int32 PlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];
		ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
		if (PS)
		{
			UE_LOG(LogRosikoGameManager, Log, TEXT("%s's turn to place troops. Remaining: %d"),
			       *PS->GetPlayerName(), PS->TroopsToPlace);
		}
	}
}

void ARosikoGameManager::SelectPlayerColor(int32 PlayerID, FLinearColor ChosenColor)
{
	// DEPRECATO: Questa funzione ora viene chiamata tramite PlayerController
	// Mantenuta per compatibilità con Blueprint e host locale
	UE_LOG(LogRosikoGameManager, Warning, TEXT("SelectPlayerColor (DEPRECATED) called - PlayerID: %d, HasAuthority: %s"),
	       PlayerID, HasAuthority() ? TEXT("TRUE (Server)") : TEXT("FALSE (Client)"));

	if (HasAuthority())
	{
		// Se siamo sul server (host), esegui direttamente
		SelectPlayerColor_Direct(PlayerID, ChosenColor);
	}
	else
	{
		// Se siamo su un client, questa chiamata non funziona più
		// Il client deve chiamare tramite PlayerController
		UE_LOG(LogRosikoGameManager, Error, TEXT("Client called SelectPlayerColor directly! Use PlayerController->Server_SelectPlayerColor instead"));
	}
}

void ARosikoGameManager::SelectPlayerColor_Direct(int32 PlayerID, FLinearColor ChosenColor)
{
	// Questa è la funzione che viene eseguita SUL SERVER
	UE_LOG(LogRosikoGameManager, Warning, TEXT("Server_SelectPlayerColor_Implementation - PlayerID: %d"), PlayerID);

	ARosikoGameState* GS = GetRosikoGameState();
	ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);

	if (!GS || !PS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("SelectPlayerColor_Direct - GameState or PlayerState is null!"));
		return;
	}

	// Validazione fase
	if (GS->CurrentPhase != EGamePhase::ColorSelection)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Cannot select color outside ColorSelection phase"));
		return;
	}

	// Verifica che sia il turno di questo player
	if (!GS->TurnOrder.IsValidIndex(GS->CurrentPlayerTurn))
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("Invalid CurrentPlayerTurn index: %d"), GS->CurrentPlayerTurn);
		return;
	}

	int32 CurrentPlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];
	if (PlayerID != CurrentPlayerID)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Not %s's turn (current: Player %d)"),
		       *PS->GetPlayerName(), CurrentPlayerID);
		return;
	}

	// Verifica che player non abbia già scelto
	if (PS->bHasSelectedColor)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("%s has already selected a color"), *PS->GetPlayerName());
		return;
	}

	// Verifica che il colore sia disponibile (con tolleranza per confronto float)
	bool bColorAvailable = false;
	int32 ColorIndex = -1;
	for (int32 i = 0; i < GS->AvailableColors.Num(); i++)
	{
		if (GS->AvailableColors[i].Equals(ChosenColor, 0.01f)) // Tolleranza 0.01 per float
		{
			bColorAvailable = true;
			ColorIndex = i;
			break;
		}
	}

	if (!bColorAvailable)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Chosen color not available or already taken"));
		return;
	}

	// Assegna colore al player tramite PlayerState
	PS->SetArmyColor(ChosenColor); // Imposta anche bHasSelectedColor = true

	// Rimuovi colore da disponibili in GameState
	GS->AvailableColors.RemoveAt(ColorIndex);

	UE_LOG(LogRosikoGameManager, Log, TEXT("%s selected color RGB(%.2f, %.2f, %.2f)"),
	       *PS->GetPlayerName(), ChosenColor.R, ChosenColor.G, ChosenColor.B);

	// Broadcast aggiornamento player
	BroadcastPlayerUpdate(PlayerID);

	// Passa al prossimo player
	int32 NextTurnIndex = GS->CurrentPlayerTurn + 1;

	// Cerca prossimo player che non ha ancora scelto
	bool bFoundNextPlayer = false;
	int32 CheckCount = 0;
	while (NextTurnIndex < GS->TurnOrder.Num() && CheckCount < NumPlayers)
	{
		int32 NextPlayerID = GS->TurnOrder[NextTurnIndex];
		ARosikoPlayerState* NextPS = GetRosikoPlayerState(NextPlayerID);
		if (NextPS && !NextPS->bHasSelectedColor)
		{
			GS->CurrentPlayerTurn = NextTurnIndex;
			bFoundNextPlayer = true;
			UE_LOG(LogRosikoGameManager, Log, TEXT("Next player: %s (ID: %d)"),
			       *NextPS->GetPlayerName(), NextPlayerID);

			// Broadcast evento LOCALE sul server
			OnColorSelectionRequired.Broadcast(NextPlayerID, GS->AvailableColors);

			// Multicast RPC per notificare i client
			Multicast_NotifyColorSelectionTurn(NextPlayerID, GS->AvailableColors);
			break;
		}
		NextTurnIndex++;
		CheckCount++;
	}

	// Se non trovato, verifica se tutti hanno scelto
	if (!bFoundNextPlayer)
	{
		bool bAllSelected = true;
		TArray<ARosikoPlayerState*> AllPS = GetAllPlayerStates();
		for (ARosikoPlayerState* CheckPS : AllPS)
		{
			if (CheckPS && !CheckPS->bHasSelectedColor)
			{
				bAllSelected = false;
				break;
			}
		}

		if (bAllSelected)
		{
			// Tutti hanno scelto → Procedi con distribuzione territori
			UE_LOG(LogRosikoGameManager, Log, TEXT("All players selected colors. Starting territory distribution..."));

			DistributeInitialTerritories();
			ChangePhase(EGamePhase::InitialDistribution);
			GS->CurrentPlayerTurn = 0; // Reset al primo player in TurnOrder
			StartInitialPlacement();
		}
		else
		{
			// Alcuni player hanno skippato (disconnessi) → Riparti dal primo che non ha scelto
			for (int32 i = 0; i < GS->TurnOrder.Num(); i++)
			{
				int32 CheckPlayerID = GS->TurnOrder[i];
				ARosikoPlayerState* CheckPS = GetRosikoPlayerState(CheckPlayerID);
				if (CheckPS && !CheckPS->bHasSelectedColor)
				{
					GS->CurrentPlayerTurn = i;
					UE_LOG(LogRosikoGameManager, Log, TEXT("Looping back to %s (ID: %d) for color selection"),
					       *CheckPS->GetPlayerName(), CheckPlayerID);

					// Broadcast evento LOCALE sul server
					OnColorSelectionRequired.Broadcast(CheckPlayerID, GS->AvailableColors);

					// Multicast RPC per notificare i client
					Multicast_NotifyColorSelectionTurn(CheckPlayerID, GS->AvailableColors);
					break;
				}
			}
		}
	}
}

bool ARosikoGameManager::PlaceTroops(int32 PlayerID, int32 TerritoryID, int32 Amount)
{
	// Validazione base
	if (Amount <= 0)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Invalid amount: %d"), Amount);
		return false;
	}

	ARosikoGameState* GS = GetRosikoGameState();
	ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);

	if (!GS || !PS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("PlaceTroops - GameState or PlayerState is null!"));
		return false;
	}

	FTerritoryGameState* Territory = GS->GetTerritory(TerritoryID);
	if (!Territory)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("Invalid territory ID: %d"), TerritoryID);
		return false;
	}

	// Solo il proprietario può piazzare carri
	if (Territory->OwnerID != PlayerID)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("%s doesn't own territory %d (owned by Player %d)"),
		       *PS->GetPlayerName(), TerritoryID, Territory->OwnerID);
		return false;
	}

	// Check carri disponibili
	if (PS->TroopsToPlace < Amount)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("%s doesn't have %d troops available (has %d)"),
		       *PS->GetPlayerName(), Amount, PS->TroopsToPlace);
		return false;
	}

	// Esegui piazzamento
	Territory->Troops += Amount;
	PS->RemoveTroops(Amount);

	BroadcastTerritoryUpdate(TerritoryID);
	BroadcastPlayerUpdate(PlayerID);

	UE_LOG(LogRosikoGameManager, Log, TEXT("%s placed %d troops on territory %d (now has %d)"),
	       *PS->GetPlayerName(), Amount, TerritoryID, Territory->Troops);

	// Se player ha finito carri, passa al prossimo automaticamente
	if (PS->TroopsToPlace <= 0)
	{
		UE_LOG(LogRosikoGameManager, Log, TEXT("%s has placed all troops"), *PS->GetPlayerName());
		EndTurn();
	}

	return true;
}

void ARosikoGameManager::EndTurn()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("EndTurn - GameState is null!"));
		return;
	}

	// Cerca prossimo player con carri da piazzare (durante fase InitialDistribution)
	if (GS->CurrentPhase == EGamePhase::InitialDistribution)
	{
		int32 StartIndex = GS->CurrentPlayerTurn;
		int32 NextTurnIndex = (GS->CurrentPlayerTurn + 1) % GS->TurnOrder.Num();

		// Loop finché non trovi un player con carri da piazzare
		while (NextTurnIndex != StartIndex)
		{
			int32 NextPlayerID = GS->TurnOrder[NextTurnIndex];
			ARosikoPlayerState* NextPS = GetRosikoPlayerState(NextPlayerID);
			if (NextPS && NextPS->TroopsToPlace > 0)
			{
				ChangeTurn(NextTurnIndex);
				UE_LOG(LogRosikoGameManager, Log, TEXT("Next player: %s"), *NextPS->GetPlayerName());
				return;
			}
			NextTurnIndex = (NextTurnIndex + 1) % GS->TurnOrder.Num();
		}

		// Se siamo tornati al player iniziale, controlla se ha ancora carri
		int32 CurrentPlayerID = GS->TurnOrder[StartIndex];
		ARosikoPlayerState* CurrentPS = GetRosikoPlayerState(CurrentPlayerID);
		if (CurrentPS && CurrentPS->TroopsToPlace > 0)
		{
			return; // Continua con lo stesso player
		}

		// Tutti hanno piazzato tutti i carri → Fine fase iniziale
		UE_LOG(LogRosikoGameManager, Log, TEXT("Initial placement complete! Starting main game."));
		ChangePhase(EGamePhase::Reinforce);
		ChangeTurn(0); // Ricomincia dal primo in TurnOrder
	}
	else
	{
		// Turno normale (Reinforce/Attack/Fortify cycle)
		int32 NextTurnIndex = (GS->CurrentPlayerTurn + 1) % GS->TurnOrder.Num();
		ChangeTurn(NextTurnIndex);

		// TODO: Se siamo in fase Attack, permetti di continuare attacchi o passare a Fortify
		// TODO: Se siamo in fase Fortify, passa automaticamente a Reinforce del prossimo player
	}
}

void ARosikoGameManager::EndPhase()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("EndPhase - GameState is null!"));
		return;
	}

	// Transizione fasi: Reinforce → Attack → Fortify → (next player) Reinforce
	switch (GS->CurrentPhase)
	{
		case EGamePhase::Reinforce:
			ChangePhase(EGamePhase::Attack);
			break;

		case EGamePhase::Attack:
			ChangePhase(EGamePhase::Fortify);
			break;

		case EGamePhase::Fortify:
			EndTurn(); // Passa al prossimo giocatore, che inizierà con Reinforce
			ChangePhase(EGamePhase::Reinforce);
			break;

		default:
			UE_LOG(LogRosikoGameManager, Warning, TEXT("Cannot end phase from current state: %d"), (int32)GS->CurrentPhase);
			break;
	}
}

FTerritoryGameState ARosikoGameManager::GetTerritoryState(int32 TerritoryID) const
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		FTerritoryGameState Invalid;
		Invalid.TerritoryID = -1;
		return Invalid;
	}

	bool bFound = false;
	return GS->GetTerritoryByID(TerritoryID, bFound);
}

FPlayerGameState ARosikoGameManager::GetPlayerState(int32 PlayerID) const
{
	// DEPRECATO: Usare GetRosikoPlayerState() invece
	// Mantenuto per compatibilità Blueprint ma ricostruisce struct da PlayerState
	ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
	if (PS)
	{
		FPlayerGameState State;
		State.PlayerID = PS->GameManagerPlayerID;
		State.PlayerName = PS->GetPlayerName();
		State.PlayerColor = PS->ArmyColor;
		State.TroopsToPlace = PS->TroopsToPlace;
		State.OwnedTerritories = PS->OwnedTerritoryIDs;
		State.Hand = PS->Hand;
		State.CardExchangeCount = PS->CardExchangeCount;
		State.bIsAlive = PS->bIsAlive;
		State.bIsAI = PS->bIsAI;
		State.bHasSelectedColor = PS->bHasSelectedColor;
		return State;
	}

	// Ritorna stato invalido
	FPlayerGameState Invalid;
	Invalid.PlayerID = -1;
	return Invalid;
}

FPlayerGameState ARosikoGameManager::GetCurrentPlayer() const
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (GS && GS->TurnOrder.IsValidIndex(GS->CurrentPlayerTurn))
	{
		int32 PlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];
		return GetPlayerState(PlayerID);
	}

	// Ritorna stato invalido
	FPlayerGameState Invalid;
	Invalid.PlayerID = -1;
	return Invalid;
}

bool ARosikoGameManager::CanPlaceTroops(int32 PlayerID, int32 TerritoryID) const
{
	ARosikoGameState* GS = GetRosikoGameState();
	ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);

	if (!GS || !PS) return false;

	FTerritoryGameState* Territory = GS->GetTerritory(TerritoryID);
	if (!Territory) return false;

	// Player deve essere il proprietario e avere carri disponibili
	return (Territory->OwnerID == PlayerID) && (PS->TroopsToPlace > 0);
}

void ARosikoGameManager::BroadcastTerritoryUpdate(int32 TerritoryID)
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return;

	FTerritoryGameState* State = GS->GetTerritory(TerritoryID);
	if (!State) return;

	// Invia Multicast RPC per notificare tutti i client
	if (HasAuthority())
	{
		Multicast_NotifyTerritoryUpdate(TerritoryID, State->OwnerID, State->Troops);
	}

	// Trova TerritoryActor corrispondente e aggiorna visuals
	bool bFoundTerritory = false;
	for (TActorIterator<ATerritoryActor> It(GetWorld()); It; ++It)
	{
		if (It->GetTerritoryData().ID == TerritoryID)
		{
			bFoundTerritory = true;

			// Aggiorna display carri con nuovo sistema LOD
			if (It->TroopVisualManager)
			{
				FLinearColor OwnerColor = FLinearColor::Gray; // Default neutrale

				if (State->OwnerID >= 0)
				{
					ARosikoPlayerState* PS = GetRosikoPlayerState(State->OwnerID);
					if (PS)
					{
						OwnerColor = PS->ArmyColor;
					}
				}

				It->TroopVisualManager->UpdateTroopDisplay(State->Troops, OwnerColor);

				UE_LOG(LogRosikoGameManager, Verbose, TEXT("Updated Territory %d: Owner=%d, Troops=%d, Color=(%f,%f,%f)"),
				       TerritoryID, State->OwnerID, State->Troops, OwnerColor.R, OwnerColor.G, OwnerColor.B);
			}
			// Fallback a sistema legacy se nuovo non disponibile
			else if (It->TroopDisplay)
			{
				FLinearColor OwnerColor = FLinearColor::Gray;

				if (State->OwnerID >= 0)
				{
					ARosikoPlayerState* PS = GetRosikoPlayerState(State->OwnerID);
					if (PS)
					{
						OwnerColor = PS->ArmyColor;
					}
				}

				It->TroopDisplay->UpdateDisplay(State->Troops, OwnerColor);
				It->TroopDisplay->SetDisplayVisible(State->Troops > 0);

				UE_LOG(LogRosikoGameManager, Verbose, TEXT("Updated Territory %d (legacy mode): Owner=%d, Troops=%d"),
				       TerritoryID, State->OwnerID, State->Troops);
			}
			else
			{
				UE_LOG(LogRosikoGameManager, Warning, TEXT("Territory %d has no TroopVisualManager or TroopDisplay component!"), TerritoryID);
			}

			break;
		}
	}

	if (!bFoundTerritory)
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("BroadcastTerritoryUpdate: Territory %d actor not found!"), TerritoryID);
	}

	// Broadcast evento per UI
	OnTerritoryUpdated.Broadcast(TerritoryID);
}

void ARosikoGameManager::BroadcastPlayerUpdate(int32 PlayerID)
{
	OnPlayerUpdated.Broadcast(PlayerID);
}

void ARosikoGameManager::RefreshAllTerritoryDisplays()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return;

	UE_LOG(LogRosikoGameManager, Log, TEXT("Refreshing all territory displays..."));

	int32 RefreshedCount = 0;
	for (const FTerritoryGameState& Territory : GS->Territories)
	{
		BroadcastTerritoryUpdate(Territory.TerritoryID);
		RefreshedCount++;
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Refreshed %d territory displays"), RefreshedCount);
}

void ARosikoGameManager::ChangePhase(EGamePhase NewPhase)
{
	// Solo il server può cambiare fase
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoGameManager, Warning, TEXT("ChangePhase called on client - ignoring"));
		return;
	}

	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("ChangePhase - GameState is null!"));
		return;
	}

	EGamePhase OldPhase = GS->CurrentPhase;
	GS->CurrentPhase = NewPhase;

	UE_LOG(LogRosikoGameManager, Log, TEXT("Phase changed: %d → %d (Server)"), (int32)OldPhase, (int32)NewPhase);

	// Broadcast evento sul server
	OnPhaseChanged.Broadcast(OldPhase, NewPhase);

	// CurrentPhase verrà replicato automaticamente ai client tramite OnRep_CurrentPhase in GameState
}

void ARosikoGameManager::ChangeTurn(int32 NewTurnIndex)
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return;

	GS->CurrentPlayerTurn = NewTurnIndex;

	if (GS->TurnOrder.IsValidIndex(GS->CurrentPlayerTurn))
	{
		int32 PlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];
		ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
		if (PS)
		{
			UE_LOG(LogRosikoGameManager, Log, TEXT("Turn changed: %s (ID: %d, Turn Index: %d)"),
			       *PS->GetPlayerName(), PlayerID, NewTurnIndex);
		}
	}

	OnTurnChanged.Broadcast(GS->CurrentPlayerTurn);
}

