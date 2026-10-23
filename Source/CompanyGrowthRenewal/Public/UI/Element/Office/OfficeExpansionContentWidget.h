#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "OfficeExpansionContentWidget.generated.h"

class UCommonTextBlock;
class UResourceWidget;

UCLASS()
class COMPANYGROWTHRENEWAL_API UOfficeExpansionContentWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    bool SetExpansionPreview(int32 Current, int32 Next, int32 Maximum, int64 Cost);
    void ClearPreview();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> CurrentValueText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> NextValueText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCommonTextBlock> MaximumText;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UResourceWidget> CostResource;

private:
    int32 PreviewCurrent = 0;
    int32 PreviewNext = 0;
    int32 PreviewMaximum = 0;
    int64 PreviewCost = 0;
    bool bHasPreview = false;
};
