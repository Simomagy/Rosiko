#include "RosikoGameManager.h"
#include "RosikoGameState.h"
#include "RosikoPlayerState.h"
#include "../Map/MapGenerator.h"
#include "../Map/Territory/TerritoryActor.h"
#include "../Troop/UI/TroopDisplayComponent.h"
#include "../Troop/UI/TroopVisualManager.h"
#include "../Configs/ObjectivesConfig.h"
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
			// Tutti hanno scelto → Assegna obiettivi e procedi con distribuzione territori
			UE_LOG(LogRosikoGameManager, Log, TEXT("All players selected colors. Assigning objectives..."));

			// NUOVO: Assegna obiettivi prima di distribuire territori
			AssignObjectivesToAllPlayers();

			UE_LOG(LogRosikoGameManager, Log, TEXT("Starting territory distribution..."));
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

	// NUOVO: Verifica completamento obiettivi del giocatore corrente prima di cambiare turno
	if (GS->CurrentPhase != EGamePhase::Setup && GS->CurrentPhase != EGamePhase::ColorSelection)
	{
		if (GS->TurnOrder.IsValidIndex(GS->CurrentPlayerTurn))
		{
			int32 CurrentPlayerID = GS->TurnOrder[GS->CurrentPlayerTurn];
			CheckPlayerObjectives(CurrentPlayerID);
		}
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

// === OBIETTIVI - PUBLIC API ===

void ARosikoGameManager::AssignObjectivesToAllPlayers()
{
	if (!HasAuthority())
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("AssignObjectivesToAllPlayers called on client - must be server!"));
		return;
	}

	if (!ObjectivesConfig)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("ObjectivesConfig is null! Assign an ObjectivesConfig Data Asset."));
		return;
	}

	UE_LOG(LogRosikoGameManager, Warning, TEXT("Assigning objectives to all players..."));

	// 1. Filtra e mescola obiettivi validi
	FilterAndShuffleObjectives();

	// 2. Verifica che ci siano abbastanza obiettivi
	if (ValidMainObjectives.Num() < NumPlayers && !ObjectivesConfig->bAllowDuplicateMainObjectives)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("Not enough valid main objectives (%d) for %d players!"),
		       ValidMainObjectives.Num(), NumPlayers);
		return;
	}

	// Verifica obiettivi secondari solo se ne dobbiamo assegnare
	if (ObjectivesConfig->NumSecondaryObjectivesPerPlayer > 0)
	{
		int32 RequiredSecondaryObjectives = NumPlayers * ObjectivesConfig->NumSecondaryObjectivesPerPlayer;
		if (ValidSecondaryObjectives.Num() < RequiredSecondaryObjectives && !ObjectivesConfig->bAllowDuplicateSecondaryObjectives)
		{
			UE_LOG(LogRosikoGameManager, Warning, TEXT("Not enough valid secondary objectives (%d) for %d players (need %d). Will allow duplicates."),
			       ValidSecondaryObjectives.Num(), NumPlayers, RequiredSecondaryObjectives);
		}
	}

	// 3. Assegna obiettivi a ogni giocatore in TurnOrder
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS) return;

	for (int32 PlayerID : GS->TurnOrder)
	{
		ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
		if (PS)
		{
			AssignObjectivesToPlayer(PS);
		}
	}

	UE_LOG(LogRosikoGameManager, Warning, TEXT("Objectives assigned to all players successfully!"));
}

bool ARosikoGameManager::CheckPlayerObjectives(int32 PlayerID)
{
	if (!HasAuthority())
	{
		return false; // Solo il server valuta obiettivi
	}

	ARosikoPlayerState* PS = GetRosikoPlayerState(PlayerID);
	ARosikoGameState* GS = GetRosikoGameState();

	if (!PS || !GS)
	{
		return false;
	}

	bool bAnyObjectiveCompleted = false;

	// Verifica obiettivo principale (se non già completato)
	if (!PS->MainObjective.bCompleted && PS->MainObjective.ObjectiveIndex >= 0)
	{
		bool bAllConditionsMet = true;
		for (const FObjectiveCondition& Condition : PS->MainObjective.Definition.Conditions)
		{
			if (!EvaluateObjectiveCondition(Condition, PS))
			{
				bAllConditionsMet = false;
				break;
			}
		}

		if (bAllConditionsMet)
		{
			PS->CompleteMainObjective(GS->CurrentPlayerTurn, GS->GameTimeSeconds);
			bAnyObjectiveCompleted = true;

			// TODO: Broadcast evento vittoria se obiettivo principale completato
			UE_LOG(LogRosikoGameManager, Warning, TEXT(">>> PLAYER %d COMPLETED MAIN OBJECTIVE! <<<"), PlayerID);
		}
	}

	// Verifica obiettivi secondari
	for (int32 i = 0; i < PS->SecondaryObjectives.Num(); i++)
	{
		FAssignedObjective& SecondaryObj = PS->SecondaryObjectives[i];

		if (SecondaryObj.bCompleted)
		{
			continue; // Già completato, skip
		}

		bool bAllConditionsMet = true;
		for (const FObjectiveCondition& Condition : SecondaryObj.Definition.Conditions)
		{
			if (!EvaluateObjectiveCondition(Condition, PS))
			{
				bAllConditionsMet = false;
				break;
			}
		}

		if (bAllConditionsMet)
		{
			PS->CompleteSecondaryObjective(i, GS->CurrentPlayerTurn, GS->GameTimeSeconds);
			bAnyObjectiveCompleted = true;
		}
	}

	return bAnyObjectiveCompleted;
}

void ARosikoGameManager::CheckAllObjectivesCompletion()
{
	if (!HasAuthority())
	{
		return;
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Checking objectives completion for all players..."));

	TArray<ARosikoPlayerState*> AllPlayers = GetAllPlayerStates();

	for (ARosikoPlayerState* PS : AllPlayers)
	{
		if (PS && PS->bIsAlive)
		{
			CheckPlayerObjectives(PS->GameManagerPlayerID);
		}
	}
}

// === OBIETTIVI - INTERNAL LOGIC ===

void ARosikoGameManager::FilterAndShuffleObjectives()
{
	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS || !ObjectivesConfig)
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("FilterAndShuffleObjectives - GameState or ObjectivesConfig is null!"));
		return;
	}

	// Ottieni colori attivi in partita (già selezionati dai giocatori)
	TArray<FLinearColor> ActiveColors;
	TArray<ARosikoPlayerState*> AllPlayers = GetAllPlayerStates();
	for (ARosikoPlayerState* PS : AllPlayers)
	{
		if (PS && PS->bHasSelectedColor)
		{
			ActiveColors.Add(PS->ArmyColor);
		}
	}

	// Filtra obiettivi validi
	ValidMainObjectives = ObjectivesConfig->FilterValidMainObjectives(NumPlayers, ActiveColors);
	ValidSecondaryObjectives = ObjectivesConfig->FilterValidSecondaryObjectives(NumPlayers, ActiveColors);

	UE_LOG(LogRosikoGameManager, Log, TEXT("Filtered objectives: %d main, %d secondary"),
	       ValidMainObjectives.Num(), ValidSecondaryObjectives.Num());

	// Shuffle deterministico con GameRNG
	// Fisher-Yates shuffle per main objectives
	for (int32 i = ValidMainObjectives.Num() - 1; i > 0; i--)
	{
		int32 j = GameRNG.RandRange(0, i);
		ValidMainObjectives.Swap(i, j);
	}

	// Fisher-Yates shuffle per secondary objectives
	for (int32 i = ValidSecondaryObjectives.Num() - 1; i > 0; i--)
	{
		int32 j = GameRNG.RandRange(0, i);
		ValidSecondaryObjectives.Swap(i, j);
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Objectives shuffled with deterministic RNG"));
}

void ARosikoGameManager::AssignObjectivesToPlayer(ARosikoPlayerState* PS)
{
	if (!PS)
	{
		return;
	}

	int32 PlayerID = PS->GameManagerPlayerID;

	// Assegna 1 obiettivo principale
	if (ValidMainObjectives.Num() > 0)
	{
		// Prendi il prossimo obiettivo principale disponibile
		// Se bAllowDuplicateMainObjectives = false, usa PlayerID come indice
		// Se bAllowDuplicateMainObjectives = true, usa modulo per permettere duplicati
		int32 MainIndex = ObjectivesConfig->bAllowDuplicateMainObjectives
			? (PlayerID % ValidMainObjectives.Num())
			: FMath::Min(PlayerID, ValidMainObjectives.Num() - 1);

		PS->AssignMainObjective(ValidMainObjectives[MainIndex], MainIndex);
	}
	else
	{
		UE_LOG(LogRosikoGameManager, Error, TEXT("No valid main objectives available for Player %d!"), PlayerID);
	}

	// Assegna N obiettivi secondari (configurabile)
	int32 NumSecondariesToAssign = ObjectivesConfig->NumSecondaryObjectivesPerPlayer;

	if (NumSecondariesToAssign > 0 && ValidSecondaryObjectives.Num() > 0)
	{
		int32 SecondaryStartIndex = PlayerID * NumSecondariesToAssign;
		TArray<int32> UsedIndices; // Per evitare duplicati per stesso giocatore

		for (int32 i = 0; i < NumSecondariesToAssign; i++)
		{
			int32 SecondaryIndex;

			if (ObjectivesConfig->bAllowDuplicateSecondaryObjectives)
			{
				// Permetti duplicati tra giocatori, ma evita duplicati per stesso giocatore se bAllowDuplicatesPerPlayer = false
				if (!ObjectivesConfig->bAllowDuplicatesPerPlayer)
				{
					// Cerca un indice non già usato da questo giocatore
					int32 Attempts = 0;
					do
					{
						SecondaryIndex = (SecondaryStartIndex + i + Attempts) % ValidSecondaryObjectives.Num();
						Attempts++;
					} while (UsedIndices.Contains(SecondaryIndex) && Attempts < ValidSecondaryObjectives.Num());

					UsedIndices.Add(SecondaryIndex);
				}
				else
				{
					// Permetti anche duplicati per stesso giocatore
					SecondaryIndex = (SecondaryStartIndex + i) % ValidSecondaryObjectives.Num();
				}
			}
			else
			{
				// No duplicati tra giocatori: usa indici sequenziali
				SecondaryIndex = (SecondaryStartIndex + i) % ValidSecondaryObjectives.Num();
			}

			PS->AssignSecondaryObjective(ValidSecondaryObjectives[SecondaryIndex], SecondaryIndex);
		}
	}

	UE_LOG(LogRosikoGameManager, Log, TEXT("Player %d - Assigned 1 main + %d secondary objectives"),
	       PlayerID, PS->SecondaryObjectives.Num());
}

bool ARosikoGameManager::EvaluateObjectiveCondition(const FObjectiveCondition& Condition, ARosikoPlayerState* PS)
{
	if (!PS)
	{
		return false;
	}

	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		return false;
	}

	int32 PlayerID = PS->GameManagerPlayerID;

	switch (Condition.Type)
	{
		case EObjectiveConditionType::ConquerTerritories:
		{
			// Verifica se il giocatore possiede almeno RequiredCount territori
			return PS->GetNumTerritoriesOwned() >= Condition.RequiredCount;
		}

		case EObjectiveConditionType::ConquerContinents:
		{
			// Conta quanti continenti target il giocatore controlla completamente
			int32 ControlledCount = 0;
			for (int32 ContinentID : Condition.TargetContinentIDs)
			{
				if (DoesPlayerControlContinent(PlayerID, ContinentID))
				{
					ControlledCount++;
				}
			}
			return ControlledCount >= Condition.RequiredCount;
		}

		case EObjectiveConditionType::ConquerTerritoriesInContinents:
		{
			// Conta territori posseduti nei continenti target
			int32 TotalTerritories = 0;
			for (int32 ContinentID : Condition.TargetContinentIDs)
			{
				TotalTerritories += CountPlayerTerritoriesInContinent(PlayerID, ContinentID);
			}
			return TotalTerritories >= Condition.RequiredCount;
		}

		case EObjectiveConditionType::ControlFullContinent:
		{
			// Verifica che il giocatore controlli completamente almeno RequiredCount continenti tra quelli target
			int32 FullyControlledCount = 0;
			for (int32 ContinentID : Condition.TargetContinentIDs)
			{
				if (DoesPlayerControlContinent(PlayerID, ContinentID))
				{
					FullyControlledCount++;
				}
			}
			return FullyControlledCount >= Condition.RequiredCount;
		}

		case EObjectiveConditionType::EliminatePlayerColor:
		{
			// Verifica che il giocatore abbia eliminato un giocatore con uno dei colori target
			TArray<ARosikoPlayerState*> AllPlayers = GetAllPlayerStates();
			for (ARosikoPlayerState* OtherPS : AllPlayers)
			{
				if (!OtherPS || OtherPS->GameManagerPlayerID == PlayerID)
				{
					continue; // Skip self
				}

				// Verifica se OtherPS ha un colore target ed è stato eliminato da PS
				for (const FLinearColor& TargetColor : Condition.TargetColors)
				{
					if (OtherPS->ArmyColor.Equals(TargetColor, 0.01f) &&
					    !OtherPS->bIsAlive &&
					    OtherPS->EliminatedBy == PlayerID)
					{
						return true;
					}
				}
			}
			return false;
		}

		case EObjectiveConditionType::ControlAdjacentTerritories:
		{
			// TODO: Implementare logica per territori confinanti consecutivi
			// Richiede graph traversal dei territori posseduti
			UE_LOG(LogRosikoGameManager, Warning, TEXT("ControlAdjacentTerritories not yet implemented"));
			return false;
		}

		case EObjectiveConditionType::SurviveUntilTurn:
		{
			// Verifica che il giocatore sia vivo, abbia raggiunto il turno richiesto e abbia abbastanza territori
			return PS->bIsAlive &&
			       GS->CurrentPlayerTurn >= Condition.RequiredTurn &&
			       PS->GetNumTerritoriesOwned() >= Condition.MinTerritories;
		}

		case EObjectiveConditionType::ExchangeCardSets:
		{
			// Verifica numero di scambi carte effettuati
			return PS->CardExchangeCount >= Condition.RequiredCount;
		}

		case EObjectiveConditionType::Custom:
		{
			// Custom objectives richiedono implementazione specifica
			UE_LOG(LogRosikoGameManager, Warning, TEXT("Custom objective condition - not implemented"));
			return false;
		}

		default:
			UE_LOG(LogRosikoGameManager, Warning, TEXT("Unknown objective condition type: %d"), (int32)Condition.Type);
			return false;
	}
}

bool ARosikoGameManager::DoesPlayerControlContinent(int32 PlayerID, int32 ContinentID)
{
	if (!MapGenerator)
	{
		return false;
	}

	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		return false;
	}

	// Ottieni tutti i territori del continente dal MapGenerator
	const TArray<FGeneratedTerritory>& AllTerritories = MapGenerator->GetGeneratedTerritories();

	int32 TotalInContinent = 0;
	int32 OwnedInContinent = 0;

	for (const FGeneratedTerritory& GenTerritory : AllTerritories)
	{
		if (GenTerritory.ContinentID == ContinentID && !GenTerritory.bIsOcean)
		{
			TotalInContinent++;

			// Verifica se il giocatore lo possiede
			FTerritoryGameState* TerritoryState = GS->GetTerritory(GenTerritory.ID);
			if (TerritoryState && TerritoryState->OwnerID == PlayerID)
			{
				OwnedInContinent++;
			}
		}
	}

	// Il giocatore controlla il continente se possiede TUTTI i suoi territori
	return (TotalInContinent > 0) && (OwnedInContinent == TotalInContinent);
}

int32 ARosikoGameManager::CountPlayerTerritoriesInContinent(int32 PlayerID, int32 ContinentID)
{
	if (!MapGenerator)
	{
		return 0;
	}

	ARosikoGameState* GS = GetRosikoGameState();
	if (!GS)
	{
		return 0;
	}

	const TArray<FGeneratedTerritory>& AllTerritories = MapGenerator->GetGeneratedTerritories();
	int32 Count = 0;

	for (const FGeneratedTerritory& GenTerritory : AllTerritories)
	{
		if (GenTerritory.ContinentID == ContinentID && !GenTerritory.bIsOcean)
		{
			FTerritoryGameState* TerritoryState = GS->GetTerritory(GenTerritory.ID);
			if (TerritoryState && TerritoryState->OwnerID == PlayerID)
			{
				Count++;
			}
		}
	}

	return Count;
}
