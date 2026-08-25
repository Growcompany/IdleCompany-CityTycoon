#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Data/RankingData.h"
#include "RankingPlayerDetailWidget.generated.h"

class UTextBlock;
class UCommonButtonBase;
class UButton;
class UCloseButtonWidget;

/**
 * 랭킹 플레이어 상세 팝업
 * - 프로필 정보 (HQ, 등급, 빌딩, 직원, 매출)
 * - 도시 방문 버튼
 */
UCLASS()
class COMPANYGROWTHRENEWAL_API URankingPlayerDetailWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	// 플레이어 데이터 설정
	void SetPlayerData(const FRankingEntry& InEntry);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> CompanyTitleText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> HQLevelText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> BuildingInfoText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> EmployeeCountText = nullptr;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> RevenueText = nullptr;

	// 도시 방문 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> VisitCityButton = nullptr;

	// 닫기 버튼
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UCloseButtonWidget> CloseButton = nullptr;

	// 배경 클릭 닫기
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UButton> BackgroundBtn = nullptr;

private:
	FRankingEntry CachedEntry;

	UFUNCTION()
	void OnVisitCityClicked();

	UFUNCTION()
	void OnCloseClicked();

	UFUNCTION()
	void OnBackgroundClicked();
};
