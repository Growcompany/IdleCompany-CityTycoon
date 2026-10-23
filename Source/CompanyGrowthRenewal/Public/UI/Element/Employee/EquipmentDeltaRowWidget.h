#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/Element/Employee/EmployeeEquipmentDialogWidget.h"
#include "EquipmentDeltaRowWidget.generated.h"

class UBorder;
class UCommonTextBlock;

/** 상세 창 효과 표의 한 줄 — 이름 / 이전값 → 이후값 / 혜택 변화 색. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEquipmentDeltaRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FEquipmentDeltaRow& Row);

protected:
	virtual void NativePreConstruct() override;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Designer Preview") FText PreviewName;
	UPROPERTY(EditAnywhere, Category = "Designer Preview") FText PreviewBefore;
	UPROPERTY(EditAnywhere, Category = "Designer Preview") FText PreviewAfter;
	UPROPERTY(EditAnywhere, Category = "Designer Preview") bool bPreviewBenefit = true;
#endif

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> StatNameText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> BeforeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> AfterText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> TrendText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> ArrowText;
};

/** 교체로 잃거나 얻는 고유효과 칩. */
UCLASS(meta=(DisableNativeTick))
class COMPANYGROWTHRENEWAL_API UEquipmentUniqueChipWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	void Configure(const FEquipmentUniqueChange& Change);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCommonTextBlock> ChipText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> ChipBorder;
};
