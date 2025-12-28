#include "ObjectiveCardWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectiveCard, Log, All);

void UObjectiveCardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Refresh iniziale se già abbiamo dati
	if (ObjectiveData.ObjectiveIndex >= 0)
	{
		RefreshDisplay();
	}
}

void UObjectiveCardWidget::SetObjectiveData(const FAssignedObjective& InObjectiveData, bool bInIsMainObjective)
{
	ObjectiveData = InObjectiveData;
	bIsMainObjective = bInIsMainObjective;
	bShowSecret = false; // Sempre visibile al proprietario

	RefreshDisplay();
}

void UObjectiveCardWidget::RefreshDisplay()
{
	if (!DisplayNameText || !DescriptionText)
	{
		UE_LOG(LogObjectiveCard, Warning, TEXT("RefreshDisplay called but widget components not bound!"));
		return;
	}

	// === NOME OBIETTIVO ===
	DisplayNameText->SetText(ObjectiveData.Definition.DisplayName);

	// === DESCRIZIONE ===
	if (bShowSecret && !ObjectiveData.bCompleted)
	{
		// Nascosto (per obiettivi di altri player in futuro)
		DescriptionText->SetText(FText::FromString(TEXT("???")));
	}
	else
	{
		// Visibile
		DescriptionText->SetText(ObjectiveData.Definition.Description);
	}

	// === TIPO OBIETTIVO ===
	if (TypeText)
	{
		if (bIsMainObjective)
		{
			TypeText->SetText(FText::FromString(TEXT("OBIETTIVO PRINCIPALE")));
			TypeText->SetColorAndOpacity(MainObjectiveColor);
		}
		else
		{
			TypeText->SetText(FText::FromString(TEXT("OBIETTIVO SECONDARIO")));
			TypeText->SetColorAndOpacity(SecondaryObjectiveColor);
		}
	}

	// === STATO COMPLETAMENTO ===
	if (CompletionIcon)
	{
		CompletionIcon->SetVisibility(ObjectiveData.bCompleted ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (CardBorder)
	{
		if (ObjectiveData.bCompleted)
		{
			CardBorder->SetBrushColor(CompletedColor);

			// Trigger evento Blueprint per animazioni custom
			OnObjectiveCompleted();
		}
		else
		{
			CardBorder->SetBrushColor(NormalColor);
		}
	}

	// === PUNTI VITTORIA (solo secondari) ===
	if (VictoryPointsText && VictoryPointsBox)
	{
		if (!bIsMainObjective && ObjectiveData.Definition.VictoryPoints > 0)
		{
			FString PointsStr = FString::FromInt(ObjectiveData.Definition.VictoryPoints) + TEXT(" pt");
			VictoryPointsText->SetText(FText::FromString(PointsStr));
			VictoryPointsBox->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			VictoryPointsBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	UE_LOG(LogObjectiveCard, Verbose, TEXT("Refreshed objective card: %s (Completed: %s)"),
	       *ObjectiveData.Definition.DisplayName.ToString(),
	       ObjectiveData.bCompleted ? TEXT("YES") : TEXT("NO"));
}
