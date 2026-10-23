#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "GachaOddsCellWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(meta = (DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UGachaOddsCellWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetOdds(const FText& Grade, float Percent, const FLinearColor& GradeColor, bool bShowSeparator, bool bLatinGrade = false);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> GradeText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LatinGradeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PercentText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Separator;
};
