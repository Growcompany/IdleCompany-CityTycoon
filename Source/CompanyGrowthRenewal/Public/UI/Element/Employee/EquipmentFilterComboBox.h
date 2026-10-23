#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "EquipmentFilterComboBox.generated.h"

/** Equipment filters keep counts in menu rows and display only the selected label. */
UCLASS()
class COMPANYGROWTHRENEWAL_API UEquipmentFilterComboBox : public UComboBoxString
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void HandleSelectionChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectionType) override;

private:
	void ResetSelectedLabel();
};
