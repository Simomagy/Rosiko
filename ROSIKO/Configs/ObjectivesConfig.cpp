#include "ObjectivesConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectivesConfig, Log, All);

TArray<FObjectiveDefinition> UObjectivesConfig::FilterValidMainObjectives(int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const
{
	TArray<FObjectiveDefinition> ValidObjectives;

	for (const FObjectiveDefinition& Objective : MainObjectives)
	{
		if (IsObjectiveValid(Objective, NumPlayers, ActiveColors))
		{
			ValidObjectives.Add(Objective);
		}
		else
		{
			UE_LOG(LogObjectivesConfig, Verbose, TEXT("Filtered out main objective '%s' (invalid for current game config)"),
			       *Objective.DisplayName.ToString());
		}
	}

	UE_LOG(LogObjectivesConfig, Log, TEXT("Filtered main objectives: %d/%d valid for %d players"),
	       ValidObjectives.Num(), MainObjectives.Num(), NumPlayers);

	return ValidObjectives;
}

TArray<FObjectiveDefinition> UObjectivesConfig::FilterValidSecondaryObjectives(int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const
{
	TArray<FObjectiveDefinition> ValidObjectives;

	for (const FObjectiveDefinition& Objective : SecondaryObjectives)
	{
		if (IsObjectiveValid(Objective, NumPlayers, ActiveColors))
		{
			ValidObjectives.Add(Objective);
		}
		else
		{
			UE_LOG(LogObjectivesConfig, Verbose, TEXT("Filtered out secondary objective '%s' (invalid for current game config)"),
			       *Objective.DisplayName.ToString());
		}
	}

	UE_LOG(LogObjectivesConfig, Log, TEXT("Filtered secondary objectives: %d/%d valid for %d players"),
	       ValidObjectives.Num(), SecondaryObjectives.Num(), NumPlayers);

	return ValidObjectives;
}

bool UObjectivesConfig::IsObjectiveValid(const FObjectiveDefinition& Objective, int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const
{
	// Un obiettivo è valido se TUTTE le sue condizioni sono valide
	for (const FObjectiveCondition& Condition : Objective.Conditions)
	{
		if (!IsConditionValid(Condition, NumPlayers, ActiveColors))
		{
			return false;
		}
	}

	return true;
}

bool UObjectivesConfig::IsConditionValid(const FObjectiveCondition& Condition, int32 NumPlayers, const TArray<FLinearColor>& ActiveColors) const
{
	// Verifica requisito numero minimo giocatori
	if (Condition.bRequiresMinPlayers && NumPlayers < Condition.MinPlayers)
	{
		UE_LOG(LogObjectivesConfig, Verbose, TEXT("Condition invalid: requires %d players, but only %d in game"),
		       Condition.MinPlayers, NumPlayers);
		return false;
	}

	// Verifica specifica per tipo di condizione
	switch (Condition.Type)
	{
		case EObjectiveConditionType::EliminatePlayerColor:
		{
			// Se richiede che il colore target sia in partita, verifica che esista
			if (Condition.bRequiresTargetColorInGame)
			{
				// Almeno uno dei colori target deve essere attivo in partita
				bool bFoundValidColor = false;
				for (const FLinearColor& TargetColor : Condition.TargetColors)
				{
					if (ContainsColor(ActiveColors, TargetColor))
					{
						bFoundValidColor = true;
						break;
					}
				}

				if (!bFoundValidColor)
				{
					UE_LOG(LogObjectivesConfig, Verbose, TEXT("Condition invalid: EliminatePlayerColor requires target color in game, but none found"));
					return false;
				}
			}
			break;
		}

		case EObjectiveConditionType::ConquerContinents:
		case EObjectiveConditionType::ConquerTerritoriesInContinents:
		case EObjectiveConditionType::ControlFullContinent:
		{
			// Verifica che siano specificati continenti target
			if (Condition.TargetContinentIDs.Num() == 0)
			{
				UE_LOG(LogObjectivesConfig, Warning, TEXT("Condition invalid: continent-based condition has no target continents"));
				return false;
			}
			break;
		}

		case EObjectiveConditionType::ConquerTerritories:
		case EObjectiveConditionType::ControlAdjacentTerritories:
		case EObjectiveConditionType::ExchangeCardSets:
		{
			// Verifica che RequiredCount sia valido
			if (Condition.RequiredCount <= 0)
			{
				UE_LOG(LogObjectivesConfig, Warning, TEXT("Condition invalid: RequiredCount must be > 0"));
				return false;
			}
			break;
		}

		case EObjectiveConditionType::SurviveUntilTurn:
		{
			// Verifica che RequiredTurn e MinTerritories siano validi
			if (Condition.RequiredTurn <= 0 || Condition.MinTerritories <= 0)
			{
				UE_LOG(LogObjectivesConfig, Warning, TEXT("Condition invalid: SurviveUntilTurn requires valid RequiredTurn and MinTerritories"));
				return false;
			}
			break;
		}

		case EObjectiveConditionType::Custom:
			// Custom objectives sono sempre considerati validi
			// La validazione custom va implementata nel GameManager
			break;

		default:
			UE_LOG(LogObjectivesConfig, Warning, TEXT("Unknown condition type: %d"), (int32)Condition.Type);
			return false;
	}

	return true;
}

bool UObjectivesConfig::ContainsColor(const TArray<FLinearColor>& ColorList, const FLinearColor& ColorToFind) const
{
	// Confronta colori con tolleranza per gestire imprecisioni float
	const float Tolerance = 0.01f;

	for (const FLinearColor& Color : ColorList)
	{
		if (Color.Equals(ColorToFind, Tolerance))
		{
			return true;
		}
	}

	return false;
}
