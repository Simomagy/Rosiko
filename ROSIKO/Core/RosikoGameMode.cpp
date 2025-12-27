#include "RosikoGameMode.h"
#include "Camera/RosikoCamera.h"
#include "RosikoGameManager.h"
#include "RosikoPlayerState.h"
#include "RosikoPlayerController.h"
#include "RosikoGameState.h"
#include "../Map/MapGenerator.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"

// Log category
DEFINE_LOG_CATEGORY_STATIC(LogRosikoGameMode, Log, All);

ARosikoGameMode::ARosikoGameMode()
{
	// Configura ARosikoCamera come pawn predefinito per tutti i player
	// Questo garantisce che ogni player controller (locale o remoto) spawni la propria camera
	DefaultPawnClass = ARosikoCamera::StaticClass();

	// Configura Custom GameState per stato globale partita
	GameStateClass = ARosikoGameState::StaticClass();

	// Configura Custom PlayerState per tracking player ID
	PlayerStateClass = ARosikoPlayerState::StaticClass();

	// Configura Custom PlayerController per gestire Server RPC
	PlayerControllerClass = ARosikoPlayerController::StaticClass();

	UE_LOG(LogRosikoGameMode, Log, TEXT("RosikoGameMode initialized - GameState: RosikoGameState, DefaultPawn: RosikoCamera, PlayerState: RosikoPlayerState, PlayerController: RosikoPlayerController"));
}

void ARosikoGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStartGame)
	{
		UE_LOG(LogRosikoGameMode, Log, TEXT("Auto-starting game flow..."));
		StartGameFlow();
	}
	else
	{
		UE_LOG(LogRosikoGameMode, Log, TEXT("Manual game start mode. Call StartGameFlow() when ready."));
	}
}

void ARosikoGameMode::StartGameFlow()
{
	FindGameActors();

	if (!MapGenerator)
	{
		UE_LOG(LogRosikoGameMode, Error, TEXT("MapGenerator not found! Place AMapGenerator in level."));
		return;
	}

	if (!GameManager)
	{
		UE_LOG(LogRosikoGameMode, Error, TEXT("RosikoGameManager not found! Place ARosikoGameManager in level."));
		return;
	}

	// Bind evento completion mappa
	MapGenerator->OnGenerationComplete.AddDynamic(this, &ARosikoGameMode::OnMapGenerationComplete);

	// Avvia generazione mappa asincrona
	UE_LOG(LogRosikoGameMode, Log, TEXT("Starting map generation..."));
	MapGenerator->GenerateMapAsync();
}

void ARosikoGameMode::FindGameActors()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Trova MapGenerator
	for (TActorIterator<AMapGenerator> It(World); It; ++It)
	{
		MapGenerator = *It;
		UE_LOG(LogRosikoGameMode, Log, TEXT("Found MapGenerator in level"));
		break;
	}

	// Trova GameManager
	for (TActorIterator<ARosikoGameManager> It(World); It; ++It)
	{
		GameManager = *It;
		UE_LOG(LogRosikoGameMode, Log, TEXT("Found RosikoGameManager in level"));
		break;
	}
}

void ARosikoGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		UE_LOG(LogRosikoGameMode, Warning, TEXT("PostLogin: NewPlayer is null"));
		return;
	}

	// Assegna GameManagerPlayerID al RosikoPlayerState
	ARosikoPlayerState* PS = NewPlayer->GetPlayerState<ARosikoPlayerState>();
	if (PS)
	{
		// Conta quanti player sono già connessi per determinare il PlayerID
		int32 NumConnectedPlayers = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (It->Get()) NumConnectedPlayers++;
		}

		// Assegna PlayerID sequenziale (0, 1, 2, ...)
		// -1 perché abbiamo già contato il nuovo player nell'iteratore
		int32 AssignedPlayerID = NumConnectedPlayers - 1;
		PS->SetGameManagerPlayerID(AssignedPlayerID);

		UE_LOG(LogRosikoGameMode, Log, TEXT("Player '%s' logged in - Assigned GameManagerPlayerID: %d (Total players: %d/%d)"),
		       *PS->GetPlayerName(), AssignedPlayerID, NumConnectedPlayers, ExpectedPlayerCount);

		// Verifica se possiamo avviare il gioco ora che un nuovo player si è connesso
		TryStartGame();
	}
	else
	{
		UE_LOG(LogRosikoGameMode, Error, TEXT("PostLogin: PlayerState is not ARosikoPlayerState! Check PlayerStateClass in GameMode."));
	}
}

void ARosikoGameMode::OnMapGenerationComplete()
{
	UE_LOG(LogRosikoGameMode, Log, TEXT("Map generation complete (server)."));
	bMapGenerationComplete = true;

	// Imposta ExpectedPlayerCount nel GameState (per replicazione a client)
	ARosikoGameState* GS = GetWorld()->GetGameState<ARosikoGameState>();
	if (GS)
	{
		GS->ExpectedPlayerCount = ExpectedPlayerCount;

		// Il server è sempre pronto (segna PlayerID 0 come ready)
		GS->MarkPlayerReady(0);
	}

	// Verifica se possiamo avviare il gioco (mappa pronta + tutti i player connessi)
	TryStartGame();
}

void ARosikoGameMode::NotifyClientReady(APlayerController* ReadyClient)
{
	if (!ReadyClient)
	{
		UE_LOG(LogRosikoGameMode, Warning, TEXT("NotifyClientReady: ReadyClient is null"));
		return;
	}

	// Aggiungi alla lista se non già presente
	if (!ReadyClients.Contains(ReadyClient))
	{
		ReadyClients.Add(ReadyClient);

		// Ottieni PlayerID dal PlayerState
		ARosikoPlayerState* PS = ReadyClient->GetPlayerState<ARosikoPlayerState>();
		int32 PlayerID = PS ? PS->GameManagerPlayerID : -1;

		UE_LOG(LogRosikoGameMode, Warning, TEXT("Client ready: %s (PlayerID: %d, %d/%d ready)"),
		       *ReadyClient->GetName(), PlayerID, ReadyClients.Num(), ExpectedPlayerCount);

		// Notifica GameState per aggiornare UI su tutti i client
		ARosikoGameState* GS = GetWorld()->GetGameState<ARosikoGameState>();
		if (GS && PlayerID >= 0)
		{
			GS->MarkPlayerReady(PlayerID);
		}
	}

	// Verifica se possiamo avviare il gioco
	TryStartGame();
}

void ARosikoGameMode::TryStartGame()
{
	// Verifica se il gioco è già stato avviato
	if (bGameStarted)
	{
		return;
	}

	// Verifica che la mappa sia pronta (sul server)
	if (!bMapGenerationComplete)
	{
		UE_LOG(LogRosikoGameMode, Log, TEXT("TryStartGame: Map not ready yet, waiting..."));
		return;
	}

	// Conta quanti player sono connessi
	int32 NumConnectedPlayers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get()) NumConnectedPlayers++;
	}

	// Verifica che tutti i player attesi siano connessi
	if (NumConnectedPlayers < ExpectedPlayerCount)
	{
		UE_LOG(LogRosikoGameMode, Log, TEXT("TryStartGame: Waiting for more players (%d/%d connected)"),
		       NumConnectedPlayers, ExpectedPlayerCount);
		return;
	}

	// Verifica che tutti i CLIENT siano pronti (hanno completato generazione mappa)
	// Il server è sempre pronto, quindi contiamo solo i client (ExpectedPlayerCount - 1 se siamo listen server)
	int32 ExpectedReadyClients = ExpectedPlayerCount - 1; // -1 perché il server non invia NotifyClientReady
	if (ReadyClients.Num() < ExpectedReadyClients)
	{
		UE_LOG(LogRosikoGameMode, Warning, TEXT("TryStartGame: Waiting for clients to be ready (%d/%d clients ready)"),
		       ReadyClients.Num(), ExpectedReadyClients);
		return;
	}

	// Tutti i requisiti soddisfatti, avvia il gioco!
	UE_LOG(LogRosikoGameMode, Warning, TEXT("All prerequisites met! Starting game with %d players (all %d clients ready)..."),
	       NumConnectedPlayers, ReadyClients.Num());

	if (GameManager)
	{
		// Verifica che GameManager abbia replicazione abilitata (necessario per multiplayer)
		if (!GameManager->GetIsReplicated())
		{
			UE_LOG(LogRosikoGameMode, Warning, TEXT("GameManager does not have replication enabled! Enabling it now."));
			GameManager->SetReplicates(true);
			GameManager->bAlwaysRelevant = true;
		}

		// Imposta il numero di player nel GameManager
		GameManager->NumPlayers = NumConnectedPlayers;

		GameManager->StartGame();
		bGameStarted = true;
		UE_LOG(LogRosikoGameMode, Warning, TEXT("Game started successfully!"));
	}
	else
	{
		UE_LOG(LogRosikoGameMode, Error, TEXT("Cannot start game: GameManager is null"));
	}
}

