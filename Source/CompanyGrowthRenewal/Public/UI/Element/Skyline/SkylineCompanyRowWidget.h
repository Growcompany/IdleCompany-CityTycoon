#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Enum/CompanyType.h"
#include "SkylineCompanyRowWidget.generated.h"

class UCommonButtonBase;
class UCommonTextBlock;
class UImage;
class UWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSkylineCompanyRowClicked, int32 /*BuildingIndex*/);

/**
 * 출전 회사 선택 목록의 1행 (WBP: UIE_SkylineCompanyRow).
 *
 * 선택 상태는 화면이 소유한다 — 행은 `SelectedMark` 만 켜고 자기 버튼의 선택 상태는 건드리지 않는다.
 * (CommonButtonBase 의 선택을 행이 직접 토글하면 목록 재구성 때 하이라이트와 실제 선택이 갈린다.)
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API USkylineCompanyRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetCompany(int32 BuildingIndex, ECompanyType Industry, float Power, int32 CrewCount, bool bSelected);

	int32 GetBuildingIndex() const { return CurrentBuildingIndex; }

	FOnSkylineCompanyRowClicked OnCompanyRowClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> RowButton = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> IndustryText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> PowerText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IndustryGlyph = nullptr;

	/** 산업 시그니처색만 받는 면. 흰색으로 저작하고 런타임에 색을 곱한다. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UImage> AccentImage = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UCommonTextBlock> CrewCountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SelectedMark = nullptr;

private:
	void HandleRowClicked();

	int32 CurrentBuildingIndex = INDEX_NONE;
};
