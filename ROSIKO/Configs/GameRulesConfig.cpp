#include "GameRulesConfig.h"

int32 UGameRulesConfig::GetTroopsForPlayerCount(int32 NumPlayers) const
{
	if (bUseCustomTroopCount)
	{
		return CustomTroopsPerPlayer;
	}

	// Cerca nella tabella ufficiale
	for (const FPlayerDistributionRule& Rule : PlayerDistributionRules)
	{
		if (Rule.NumPlayers == NumPlayers)
		{
			return Rule.InitialTroops;
		}
	}

	// Fallback: calcolo proporzionale se numero giocatori non standard
	// Formula: ~220 carri totali / NumPlayers (basato su media 3-10 giocatori)
	return FMath::Max(10, 220 / NumPlayers);
}

bool UGameRulesConfig::IsValidPlayerCount(int32 NumPlayers) const
{
	return NumPlayers >= 3 && NumPlayers <= MaxPlayers;
}

int32 UGameRulesConfig::GetCardExchangeBonus(int32 ExchangeCount) const
{
	if (CardExchangeBonuses.IsValidIndex(ExchangeCount))
	{
		return CardExchangeBonuses[ExchangeCount];
	}

	// Se supera array, usa formula progressiva (+5 ogni volta dopo l'ultimo valore)
	if (CardExchangeBonuses.Num() > 0)
	{
		int32 LastBonus = CardExchangeBonuses.Last();
		int32 ExcessExchanges = ExchangeCount - (CardExchangeBonuses.Num() - 1);
		return LastBonus + (ExcessExchanges * 5);
	}

	// Fallback estremo
	return 4 + (ExchangeCount * 2);
}

