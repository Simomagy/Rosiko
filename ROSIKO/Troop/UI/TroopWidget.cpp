#include "TroopWidget.h"

void UTroopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Inizializza valori precedenti
	PreviousTroopCount = TroopCount;
	PreviousOwnerColor = OwnerColor;
}

void UTroopWidget::UpdateDisplay(int32 NewCount, FLinearColor NewColor)
{
	int32 OldCount = TroopCount;
	FLinearColor OldColor = OwnerColor;

	TroopCount = NewCount;
	OwnerColor = NewColor;

	// Notifica cambiamenti (se implementati in Blueprint)
	if (OldCount != NewCount)
	{
		OnTroopCountChanged(OldCount, NewCount);
	}

	if (!OldColor.Equals(NewColor, 0.01f))
	{
		OnOwnerColorChanged(NewColor);
	}
}

