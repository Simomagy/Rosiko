#include "ObjectivesPanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
#include "../RosikoPlayerState.h"
#include "Components/ScrollBox.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectivesPanel, Log, All);

void UObjectivesPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	InitializePanel();
}

void UObjectivesPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-update se abilitato
	if (AutoUpdateInterval > 0.0f)
	{
		UpdateTimer += InDeltaTime;
		if (UpdateTimer >= AutoUpdateInterval)
		{
			UpdateTimer = 0.0f;
			UpdateObjectivesStatus();
		}
	}
}

void UObjectivesPanelWidget::InitializePanel()
{
	// Ottieni PlayerState locale
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogObjectivesPanel, Error, TEXT("Cannot get owning player controller!"));
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	OwningPlayerState = PC->GetPlayerState<ARosikoPlayerState>();
	if (!OwningPlayerState)
	{
		UE_LOG(LogObjectivesPanel, Error, TEXT("PlayerState is not ARosikoPlayerState!"));
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// === BIND EVENTO AGGIORNAMENTO OBIETTIVI ===
	if (!OwningPlayerState->OnObjectivesUpdated.IsAlreadyBound(this, &UObjectivesPanelWidget::OnPlayerObjectivesUpdated))
	{
		OwningPlayerState->OnObjectivesUpdated.AddDynamic(this, &UObjectivesPanelWidget::OnPlayerObjectivesUpdated);
		UE_LOG(LogObjectivesPanel, Log, TEXT("Bound to OnObjectivesUpdated event"));
	}

	// === VERIFICA SE OBIETTIVI SONO STATI ASSEGNATI ===
	if (OwningPlayerState->MainObjective.ObjectiveIndex < 0)
	{
		// Obiettivi non ancora assegnati: rimanda
		UE_LOG(LogObjectivesPanel, Warning, TEXT("⏳ Objectives not yet assigned (ObjectiveIndex = %d). Will retry on next update."),
		       OwningPlayerState->MainObjective.ObjectiveIndex);
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// === DEBUG: VERIFICA DATI OBIETTIVO ===
	UE_LOG(LogObjectivesPanel, Warning, TEXT("📋 MainObjective Data:"));
	UE_LOG(LogObjectivesPanel, Warning, TEXT("   - ObjectiveIndex: %d"), OwningPlayerState->MainObjective.ObjectiveIndex);
	UE_LOG(LogObjectivesPanel, Warning, TEXT("   - DisplayName: %s"), *OwningPlayerState->MainObjective.Definition.DisplayName.ToString());
	UE_LOG(LogObjectivesPanel, Warning, TEXT("   - Description: %s"), *OwningPlayerState->MainObjective.Definition.Description.ToString());
	UE_LOG(LogObjectivesPanel, Warning, TEXT("   - Completed: %s"), OwningPlayerState->MainObjective.bCompleted ? TEXT("Yes") : TEXT("No"));
	UE_LOG(LogObjectivesPanel, Warning, TEXT("📋 SecondaryObjectives Count: %d"), OwningPlayerState->SecondaryObjectives.Num());

	UE_LOG(LogObjectivesPanel, Log, TEXT("Initializing objectives panel for Player %d"),
	       OwningPlayerState->GameManagerPlayerID);

	// === VERIFICA WIDGET BINDINGS ===
	if (!MainObjectiveCard)
	{
		UE_LOG(LogObjectivesPanel, Error, TEXT("❌ MainObjectiveCard is NULL! Check BindWidget in Blueprint."));
	}
	if (!SecondaryCardsContainer)
	{
		UE_LOG(LogObjectivesPanel, Error, TEXT("❌ SecondaryCardsContainer is NULL! Check BindWidget in Blueprint."));
	}

	// Ora possiamo mostrare il pannello
	SetVisibility(ESlateVisibility::Visible);

	// Popola obiettivo principale
	if (MainObjectiveCard)
	{
		MainObjectiveCard->SetObjectiveData(OwningPlayerState->MainObjective, true);
		bWasMainCompleted = OwningPlayerState->MainObjective.bCompleted;
		UE_LOG(LogObjectivesPanel, Log, TEXT("✓ Main objective card populated"));
	}

	// Crea carte secondarie dinamicamente
	CreateSecondaryCards();

	// Segna come inizializzato
	bPanelInitialized = true;
	UE_LOG(LogObjectivesPanel, Warning, TEXT("✓ Objectives panel fully initialized!"));
}

void UObjectivesPanelWidget::CreateSecondaryCards()
{
	if (!SecondaryCardsContainer || !OwningPlayerState)
	{
		return;
	}

	// Pulisci container
	SecondaryCardsContainer->ClearChildren();
	SecondaryCardWidgets.Empty();
	PreviousSecondaryStates.Empty();

	// Verifica se ci sono obiettivi secondari
	int32 NumSecondaries = OwningPlayerState->SecondaryObjectives.Num();

	if (NumSecondaries == 0)
	{
		// Nessun obiettivo secondario: nascondi sezione
		if (SecondariesTitle)
		{
			SecondariesTitle->SetVisibility(ESlateVisibility::Collapsed);
		}
		SecondaryCardsContainer->SetVisibility(ESlateVisibility::Collapsed);

		UE_LOG(LogObjectivesPanel, Log, TEXT("No secondary objectives to display"));
		return;
	}

	// Mostra sezione
	if (SecondariesTitle)
	{
		SecondariesTitle->SetVisibility(ESlateVisibility::Visible);
	}
	SecondaryCardsContainer->SetVisibility(ESlateVisibility::Visible);

	// Determina classe widget da usare
	TSubclassOf<UObjectiveCardWidget> WidgetClass = SecondaryCardClass;
	if (!WidgetClass)
	{
		// Fallback: usa stessa classe della main card
		WidgetClass = MainObjectiveCard ? MainObjectiveCard->GetClass() : UObjectiveCardWidget::StaticClass();
	}

	// Crea widget per ogni obiettivo secondario
	for (int32 i = 0; i < NumSecondaries; i++)
	{
		const FAssignedObjective& SecondaryObj = OwningPlayerState->SecondaryObjectives[i];

		// Crea widget carta
		UObjectiveCardWidget* NewCard = CreateWidget<UObjectiveCardWidget>(this, WidgetClass);
		if (!NewCard)
		{
			UE_LOG(LogObjectivesPanel, Error, TEXT("Failed to create secondary card widget %d!"), i);
			continue;
		}

		// Configura carta
		NewCard->SetObjectiveData(SecondaryObj, false);

		// Aggiungi al container
		SecondaryCardsContainer->AddChild(NewCard);
		SecondaryCardWidgets.Add(NewCard);

		// Inizializza stato tracking
		PreviousSecondaryStates.Add(SecondaryObj.bCompleted);

		// Aggiungi spacer (tranne dopo l'ultima)
		if (i < NumSecondaries - 1 && CardSpacing > 0.0f)
		{
			USpacer* Spacer = NewObject<USpacer>(this);
			Spacer->SetSize(FVector2D(0.0f, CardSpacing));
			SecondaryCardsContainer->AddChild(Spacer);
		}

		UE_LOG(LogObjectivesPanel, Verbose, TEXT("Created secondary card %d: %s"),
		       i, *SecondaryObj.Definition.DisplayName.ToString());
	}

	UE_LOG(LogObjectivesPanel, Log, TEXT("Created %d secondary objective cards"), SecondaryCardWidgets.Num());
}

void UObjectivesPanelWidget::OnPlayerObjectivesUpdated()
{
	// Callback da PlayerState quando obiettivi vengono assegnati/aggiornati
	UE_LOG(LogObjectivesPanel, Warning, TEXT("🔔 OnPlayerObjectivesUpdated: Re-initializing panel!"));

	// Se pannello non era inizializzato, prova a inizializzarlo ora
	if (!bPanelInitialized)
	{
		InitializePanel();
	}
	else
	{
		// Se già inizializzato, refresha tutto
		RefreshAllCards();
	}
}

void UObjectivesPanelWidget::UpdateObjectivesStatus()
{
	if (!OwningPlayerState)
	{
		return;
	}

	// Se pannello non inizializzato, riprova a inizializzarlo
	if (!bPanelInitialized)
	{
		InitializePanel();
		return; // Se init fallisce, riproverà al prossimo update
	}

	// Check obiettivo principale
	if (MainObjectiveCard)
	{
		const FAssignedObjective& MainObj = OwningPlayerState->MainObjective;
		const FAssignedObjective& LocalData = MainObjectiveCard->GetObjectiveData();

		if (MainObj.bCompleted != LocalData.bCompleted)
		{
			// Stato cambiato: refresh
			MainObjectiveCard->SetObjectiveData(MainObj, true);
		}
	}

	// Check obiettivi secondari
	CheckForNewCompletions();

	for (int32 i = 0; i < SecondaryCardWidgets.Num(); i++)
	{
		if (!OwningPlayerState->SecondaryObjectives.IsValidIndex(i))
		{
			continue;
		}

		UObjectiveCardWidget* Card = SecondaryCardWidgets[i];
		if (!Card)
		{
			continue;
		}

		const FAssignedObjective& ServerObj = OwningPlayerState->SecondaryObjectives[i];
		const FAssignedObjective& LocalData = Card->GetObjectiveData();

		if (ServerObj.bCompleted != LocalData.bCompleted)
		{
			// Stato cambiato: refresh
			Card->SetObjectiveData(ServerObj, false);
		}
	}
}

void UObjectivesPanelWidget::CheckForNewCompletions()
{
	if (!OwningPlayerState)
	{
		return;
	}

	// Check main objective
	if (!bWasMainCompleted && OwningPlayerState->MainObjective.bCompleted)
	{
		// Appena completato!
		bWasMainCompleted = true;
		OnObjectiveCompleted.Broadcast(OwningPlayerState->MainObjective);

		UE_LOG(LogObjectivesPanel, Warning, TEXT("Main objective completed!"));
	}

	// Check secondari
	for (int32 i = 0; i < OwningPlayerState->SecondaryObjectives.Num(); i++)
	{
		if (!PreviousSecondaryStates.IsValidIndex(i))
		{
			continue;
		}

		const FAssignedObjective& SecondaryObj = OwningPlayerState->SecondaryObjectives[i];

		if (!PreviousSecondaryStates[i] && SecondaryObj.bCompleted)
		{
			// Appena completato!
			PreviousSecondaryStates[i] = true;
			OnObjectiveCompleted.Broadcast(SecondaryObj);

			UE_LOG(LogObjectivesPanel, Warning, TEXT("Secondary objective %d completed!"), i);
		}
	}
}

void UObjectivesPanelWidget::RefreshAllCards()
{
	if (!OwningPlayerState)
	{
		return;
	}

	// Refresh main
	if (MainObjectiveCard)
	{
		MainObjectiveCard->SetObjectiveData(OwningPlayerState->MainObjective, true);
	}

	// Refresh secondari
	for (int32 i = 0; i < SecondaryCardWidgets.Num(); i++)
	{
		if (OwningPlayerState->SecondaryObjectives.IsValidIndex(i))
		{
			SecondaryCardWidgets[i]->SetObjectiveData(OwningPlayerState->SecondaryObjectives[i], false);
		}
	}
}
